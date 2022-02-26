#include <staking/stakes_db.h>
#include <staking/staking_rewards_calculator.h>
#include <staking/util.h>


namespace DBHeaders {
    const std::string ADDRESS_TO_STAKES_MAP = "address_to_stakes_map";
    const std::string ACTIVE_STAKES = "active_stakes";
    const std::string STAKES_COMPLETED_AT_BLOCK_HEIGHT = "stakes_completed_at_block_height";
    const std::string AMOUNT_BY_PERIOD = "amounts_by_periods";
    const std::string BEST_BLOCK_HASH = "best_block_hash";
    const std::string STAKING_POOL = "staking_pool";
    const std::string FLUSH_ONGOING = "flush_ongoing";
    const std::string FREE_TX_INFO = "free_tx_info";
    const std::string BLK_FREE_TX_SIZE_PREFIX = "blk_free_tx_size_";
    const std::string FREE_TX_WINDOW_END_HEIGHT_PREFIX = "ftx_window_end_";
    const std::string NUM_COMPLETE_STAKES = "num_complete_stakes";
    const std::string NUM_EARLY_WITHDRAWN_STAKES = "num_early_withdrawn_stakes";
    const std::string GOVERNANCE_POWER_PREFIX = "gp_";
}

CStakesDbEntry::CStakesDbEntry(const uint256& txidIn, const CAmount amountIn, const CAmount rewardIn, const unsigned int periodIn, const unsigned int completeBlockIn, const unsigned int numOutputIn, const CScript scriptIn, const bool activeIn) {
    txid = txidIn;
    amount = amountIn;
    reward = rewardIn;
    periodIdx = periodIn;
    completeBlock = completeBlockIn;
    numOutput = numOutputIn;
    script = scriptIn;
    // TODO: create a validation function which will fully verify the entry
    valid = true;
    active = activeIn;
}

void CStakesDbEntry::setReward(CAmount rewardIn) {
    reward = rewardIn;
}

void CStakesDbEntry::setComplete(bool completeFlag) {
    complete = completeFlag;
}

CStakesDB::CStakesDB(size_t cache_size_bytes, bool in_memory, bool should_wipe, const std::string& leveldb_name) :
        m_db_wrapper(GetDataDir() / leveldb_name, cache_size_bytes, in_memory, should_wipe, false),
        m_staking_pool(0) {
    initHelpStates();
    verify();
}

void CStakesDB::verify() {
    verifyFlushState();
    verifyTotalAmounts();
}

void CStakesDB::verifyTotalAmounts() const {
    AmountByPeriodArray totalStakedByPeriod = {0, 0, 0, 0};
    for (const auto& stake : getAllActiveStakes()) {
        CAmount amount = stake.getAmount();
        size_t periodIdx = stake.getPeriodIdx();
        totalStakedByPeriod[periodIdx] += amount;
    }
    for (int i = 0; i < stakingParams::NUM_STAKING_PERIODS; ++i) {
        assert(totalStakedByPeriod[i] == m_amounts_by_periods[i]);
    }
}

void CStakesDB::verifyFlushState() {
    bool flushOngoing = true;
    if (m_db_wrapper.Read(DBHeaders::FLUSH_ONGOING, flushOngoing)) {
        assert(!flushOngoing);
    } else {
        assert(m_db_wrapper.IsEmpty());
    }
}

CStakesDbEntry CStakesDB::getStakeDbEntry(const uint256& txid) const {
    CStakesDbEntry output;
    if(m_db_wrapper.Read(txid, output))
        output.setKey(txid);
    else
        LogPrintf("ERROR: Cannot get stake of id %s from database\n", txid.GetHex());
    return output;
}

ClosedFreeTxWindowInfoVector CStakesDB::getFreeTxWindowsCompletedAtHeight(const uint32_t nHeight) {
    ClosedFreeTxWindowInfoVector output;
    CSerializer<ClosedFreeTxWindowInfoVector> serializer{output, m_db_wrapper, DBHeaders::FREE_TX_WINDOW_END_HEIGHT_PREFIX + std::to_string(nHeight)};
    serializer.load();
    return output;
}

CStakesDbEntry CStakesDB::getStakeDbEntry(std::string txid) const {
    return getStakeDbEntry(uint256S(txid));
}

bool CStakesDB::removeStakeEntry(const uint256& txid) {
    if (!m_db_wrapper.Erase(txid)) {
        LogPrintf("ERROR: Cannot remove stake of id %s from database\n", txid.GetHex());
        return false;
    }
    return true;
}

StakesVector CStakesDB::getAllActiveStakes() const {
    std::vector<CStakesDbEntry> res;
    for (auto id : m_active_stakes) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

std::set<uint256> CStakesDB::getActiveStakeIdsForScript(const CScript& script) {
    auto it = m_script_to_active_stakes.find(scriptToStr(script));
    if(it == m_script_to_active_stakes.end()) {
        return std::set<uint256> {};
    }
    return it->second;
}

CFreeTxInfo CStakesDB::getFreeTxInfoForScript(const CScript &script) const {
    auto it = m_free_tx_info.find(scriptToStr(script));
    if (it == m_free_tx_info.end())
        return {};
    return it->second;
}

CAmount CStakesDB::getGpForScript(const CScript &script) const {
    CAmount output = 0;
    if(!m_db_wrapper.Read(DBHeaders::GOVERNANCE_POWER_PREFIX + scriptToStr(script), output))
    {
        LogPrintf("ERROR: Cannot get GP for script %s from database\n", scriptToStr(script));
        return 0;
    }
    return output;
}

bool CStakesDB::flushDB(CStakesDBCache* cache) {
    LOCK(cs_main);
    verifyFlushState();
    m_db_wrapper.Write(DBHeaders::FLUSH_ONGOING, true);
    CDBBatch batch{m_db_wrapper};
    batch.Clear();
    size_t batch_size = static_cast<size_t>(gArgs.GetArg("-dbbatchsize", DEFAULT_BATCH_SIZE));

    m_script_to_active_stakes = cache->m_script_to_active_stakes;
    m_active_stakes = cache->m_active_stakes;
    m_staking_pool = cache->m_staking_pool;
    m_stakes_completed_at_block_height = cache->m_stakes_completed_at_block_height;
    m_free_tx_info = cache->m_free_tx_info;
    m_amounts_by_periods = cache->m_amounts_by_periods;
    m_best_block_hash = cache->m_best_block_hash;
    m_num_complete_stakes = cache->m_num_complete_stakes;
    m_num_early_withdrawn_stakes = cache->m_num_early_withdrawn_stakes;
    dumpHelpStates();

    StakesMap stakes_map = cache->m_stakes_map;
    auto it = stakes_map.begin();
    while(it != stakes_map.end()) {
        batch.Write(it->first, it->second);
        if(batch.SizeEstimate() > batch_size) {
            LogPrint(BCLog::STAKESDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            m_db_wrapper.WriteBatch(batch);
            batch.Clear();
        }
        stakes_map.erase(it++);
    }

    LogPrint(BCLog::STAKESDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    m_db_wrapper.WriteBatch(batch);
    batch.Clear();
    for (const auto& txid : cache->m_stakes_to_remove) {
        removeStakeEntry(txid);
    }

    BlockFreeTxSizeMap block_free_tx_size_map = cache->m_block_free_tx_size_map;
    auto it_b = block_free_tx_size_map.begin();
    while(it_b != block_free_tx_size_map.end()) {
        batch.Write(DBHeaders::BLK_FREE_TX_SIZE_PREFIX + it_b->first.GetHex(), it_b->second);
        if(batch.SizeEstimate() > batch_size) {
            LogPrint(BCLog::STAKESDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            m_db_wrapper.WriteBatch(batch);
            batch.Clear();
        }
        block_free_tx_size_map.erase(it_b++);
    }
    LogPrint(BCLog::STAKESDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    m_db_wrapper.WriteBatch(batch);
    batch.Clear();
    std::set<uint32_t> free_tx_window_end_heights_to_remove = cache->m_free_tx_window_end_heights_to_remove;
    for (auto height : free_tx_window_end_heights_to_remove) {
        m_db_wrapper.Erase(DBHeaders::FREE_TX_WINDOW_END_HEIGHT_PREFIX + std::to_string(height));
    }

    FreeTxWindowEndHeightMap free_tx_window_end_height_map = cache->m_free_tx_info_end_height_map;
    auto it_f = free_tx_window_end_height_map.begin();
    while(it_f != free_tx_window_end_height_map.end()) {
        CSerializer<ClosedFreeTxWindowInfoVector> serializer{it_f->second, m_db_wrapper, DBHeaders::FREE_TX_WINDOW_END_HEIGHT_PREFIX + std::to_string(it_f->first)};
        serializer.dump();
        free_tx_window_end_height_map.erase(it_f++);
    }

    std::map<std::string, CAmount> gp_map = cache->m_gp_map;
    auto it_gp = gp_map.begin();
    while(it_gp != gp_map.end()) {
        batch.Write(DBHeaders::GOVERNANCE_POWER_PREFIX + it_gp->first, it_gp->second);
        if(batch.SizeEstimate() > batch_size) {
            LogPrint(BCLog::STAKESDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            m_db_wrapper.WriteBatch(batch);
            batch.Clear();
        }
        gp_map.erase(it_gp++);
    }

    LogPrint(BCLog::STAKESDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    m_db_wrapper.WriteBatch(batch);
    batch.Clear();

    m_db_wrapper.Write(DBHeaders::FLUSH_ONGOING, false);
    dropCache();
    return true;
}

StakesVector CStakesDB::getStakesCompletedAtHeight(const uint32_t height) {
    std::vector<CStakesDbEntry> res;
    for (const auto& id : m_stakes_completed_at_block_height[height]) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

CStakingPool& CStakesDB::stakingPool() {
    return m_staking_pool;
}

void CStakesDB::initCache() {
    m_cache_mutex.lock();
}

void CStakesDB::dropCache() {
    m_cache_mutex.unlock();
}

uint256 CStakesDB::getBestBlock() {
    if (m_best_block_hash.IsNull()) {
        m_db_wrapper.Read(DBHeaders::BEST_BLOCK_HASH, m_best_block_hash);
    }
    return m_best_block_hash;
}

void CStakesDB::initHelpStates() {
    {
        CSerializer<ScriptToStakesMap> serializer{m_script_to_active_stakes, m_db_wrapper, DBHeaders::ADDRESS_TO_STAKES_MAP};
        serializer.load();
    }
    {
        CSerializer<StakeIdsSet> serializer{m_active_stakes, m_db_wrapper, DBHeaders::ACTIVE_STAKES};
        serializer.load();
    }
    {
        CSerializer<StakesCompletedAtBlockHeightMap> serializer{m_stakes_completed_at_block_height, m_db_wrapper,
                                                                DBHeaders::STAKES_COMPLETED_AT_BLOCK_HEIGHT};
        serializer.load();
    }
    {
        CSerializer<AmountByPeriodArray> serializer{m_amounts_by_periods, m_db_wrapper, DBHeaders::AMOUNT_BY_PERIOD};
        serializer.load();
    }
    {
        CSerializer<FreeTxInfoMap> serializer{m_free_tx_info, m_db_wrapper, DBHeaders::FREE_TX_INFO};
        serializer.load();
    }

    m_db_wrapper.Read(DBHeaders::STAKING_POOL, m_staking_pool);
    m_db_wrapper.Read(DBHeaders::BEST_BLOCK_HASH, m_best_block_hash);
    m_db_wrapper.Read(DBHeaders::NUM_COMPLETE_STAKES, m_num_complete_stakes);
    m_db_wrapper.Read(DBHeaders::NUM_EARLY_WITHDRAWN_STAKES, m_num_early_withdrawn_stakes);
}

void CStakesDB::dumpHelpStates() {
    {
        CSerializer<ScriptToStakesMap> serializer{m_script_to_active_stakes, m_db_wrapper, DBHeaders::ADDRESS_TO_STAKES_MAP};
        serializer.dump();
    }
    {
        CSerializer<StakeIdsSet> serializer{m_active_stakes, m_db_wrapper, DBHeaders::ACTIVE_STAKES};
        serializer.dump();
    }
    {
        CSerializer<StakesCompletedAtBlockHeightMap> serializer{m_stakes_completed_at_block_height, m_db_wrapper,
                                                                DBHeaders::STAKES_COMPLETED_AT_BLOCK_HEIGHT};
        serializer.dump();
    }
    {
        CSerializer<AmountByPeriodArray> serializer{m_amounts_by_periods, m_db_wrapper, DBHeaders::AMOUNT_BY_PERIOD};
        serializer.dump();
    }
    {
        CSerializer<FreeTxInfoMap> serializer{m_free_tx_info, m_db_wrapper, DBHeaders::FREE_TX_INFO};
        serializer.dump();
    }

    m_db_wrapper.Write(DBHeaders::STAKING_POOL, m_staking_pool);
    m_db_wrapper.Write(DBHeaders::BEST_BLOCK_HASH, m_best_block_hash);
    m_db_wrapper.Write(DBHeaders::NUM_COMPLETE_STAKES, m_num_complete_stakes);
    m_db_wrapper.Write(DBHeaders::NUM_EARLY_WITHDRAWN_STAKES, m_num_early_withdrawn_stakes);
}

uint32_t CStakesDB::getFreeTxSizeForBlock(const uint256 &hash) const {
    uint32_t output = 0;
    if(!m_db_wrapper.Read(DBHeaders::BLK_FREE_TX_SIZE_PREFIX + hash.GetHex(), output)) {
        LogPrintf("WARNING: Cannot get free tx size of block %s from database\n", hash.GetHex());
    }
    return output;
}

CStakesDBCache::CStakesDBCache(CStakesDB* db, bool fViewOnly, size_t max_cache_size): m_viewonly(fViewOnly),
m_base_db{db}, m_max_cache_size{max_cache_size}, m_staking_pool(m_base_db->stakingPool()),
m_num_complete_stakes(m_base_db->getNumCompleteStakes()),
m_num_early_withdrawn_stakes(m_base_db->getNumEarlyWithdrawnStakes()) {
    if (!m_viewonly) {
        m_base_db->initCache();
        m_active_stakes.insert(m_base_db->getActiveStakesSet().begin(),  m_base_db->getActiveStakesSet().end());
        m_stakes_completed_at_block_height.insert(m_base_db->getStakesCompletedAtBlockHeightMap().begin(),  m_base_db->getStakesCompletedAtBlockHeightMap().end());
        m_script_to_active_stakes.insert(m_base_db->getScriptMap().begin(), m_base_db->getScriptMap().end());
        m_best_block_hash = m_base_db->getBestBlock();
        m_amounts_by_periods = m_base_db->getAmountsByPeriods();
        m_free_tx_info.insert(m_base_db->getFreeTxInfoMap().begin(), m_base_db->getFreeTxInfoMap().end());
    }
}

CStakingPool& CStakesDBCache::stakingPool() {
    return m_staking_pool;
}

size_t CStakesDBCache::getNumCompleteStakes() const {
    return m_num_complete_stakes;
}

size_t CStakesDBCache::getNumEarlyWithdrawnStakes() const {
    return m_num_early_withdrawn_stakes;
}

bool CStakesDBCache::addNewStakeEntry(const CStakesDbEntry& entry) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    try {
        m_stakes_map[entry.getKey()] = entry;
        m_current_cache_size += entry.estimateSize();
        m_stakes_completed_at_block_height[entry.getCompleteBlock()].insert(entry.getKey());
        if (entry.isActive()) {
            m_active_stakes.insert(entry.getKey());
            m_script_to_active_stakes[scriptToStr(entry.getScript())].insert(entry.getKey());
            size_t periodIdx = entry.getPeriodIdx();
            CAmount amount = entry.getAmount();
            m_amounts_by_periods[periodIdx] += amount;
        }
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add stake of id %s to database\n", entry.getKeyHex());
        return false;
    }
}

bool CStakesDBCache::removeStakeEntry(const uint256& txid) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    CStakesDbEntry stake = getStakeDbEntry(txid);
    // no possible scenario to remove an inactive stake
    assert(stake.isActive());
    size_t periodIdx = stake.getPeriodIdx();
    CAmount amount = stake.getAmount();
    m_amounts_by_periods[periodIdx] -= amount;

    const uint256 stakeId = stake.getKey();
    m_active_stakes.erase(stakeId);
    eraseStakeFromScriptMap(stake);
    auto it = m_stakes_map.find(stakeId);
    if (it != m_stakes_map.end()) {
        m_stakes_map.erase(it);
    }
    m_stakes_to_remove.insert(stakeId);
    return true;
}

std::set<uint256> CStakesDBCache::getActiveStakeIdsForScript(const CScript& script) const {
    if (m_viewonly) {
        return m_base_db->getActiveStakeIdsForScript(script);
    }
    auto it = m_script_to_active_stakes.find(scriptToStr(script));
    if(it == m_script_to_active_stakes.end()) {
        return std::set<uint256> {};
    }
    return it->second;
}

CStakesDbEntry CStakesDBCache::getStakeDbEntry(const uint256& txid) const {
    if (m_viewonly) {
        return m_base_db->getStakeDbEntry(txid);
    }
    auto it = m_stakes_map.find(txid);
    CStakesDbEntry output;
    if(it == m_stakes_map.end()) {
        if (!m_stakes_to_remove.count(txid)) {
            return m_base_db->getStakeDbEntry(txid);
        }
    }
    return it->second;
}

CStakesDbEntry CStakesDBCache::getStakeDbEntry(std::string txid) const {
    return getStakeDbEntry(uint256S(txid));
}

bool CStakesDBCache::deactivateStake(const uint256& txid, const bool fSetComplete) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    CStakesDbEntry stake = getStakeDbEntry(txid);
    if (stake.isValid() && stake.isActive()) {
        stake.setInactive();
        stake.setComplete(fSetComplete);
        m_active_stakes.erase(txid);
        eraseStakeFromScriptMap(stake);
        CAmount amount = stake.getAmount();
        size_t periodIdx = stake.getPeriodIdx();
        m_amounts_by_periods[periodIdx] -= amount;
        m_stakes_map[txid] = stake;
        if (fSetComplete) {
            ++m_num_complete_stakes;
        } else {
            ++m_num_early_withdrawn_stakes;
        }
    } else {
        return false;
    }
    return true;
}

void CStakesDBCache::eraseStakeFromScriptMap(const CStakesDbEntry &stake) {
    m_script_to_active_stakes[scriptToStr(stake.getScript())].erase(stake.getKey());
    if (m_script_to_active_stakes[scriptToStr(stake.getScript())].empty()) {
        m_script_to_active_stakes.erase(scriptToStr(stake.getScript()));
    }
}

bool CStakesDBCache::flushDB() {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    if (m_flushed) return true;
    m_base_db->flushDB(this);
    m_flushed = true;
    return true;
}

StakesVector CStakesDBCache::getAllActiveStakes() const {
    if (m_viewonly) {
        return m_base_db->getAllActiveStakes();
    }
    std::vector<CStakesDbEntry> res;
    for (auto id : m_active_stakes) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid() && stake.isActive() && !stake.isComplete());
        res.push_back(stake);
    }
    return res;
}

StakesVector CStakesDBCache::getStakesCompletedAtHeight(const uint32_t height) {
    if (m_viewonly) {
        return m_base_db->getStakesCompletedAtHeight(height);
    }
    std::vector<CStakesDbEntry> res;
    for (const auto& id : m_stakes_completed_at_block_height[height]) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

bool CStakesDBCache::reactivateStake(const uint256& txid, const uint32_t height) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    CStakesDbEntry stake = getStakeDbEntry(txid);
    if (stake.isValid() && !stake.isActive()) {
        stake.setActive();
        stake.setComplete(height > stake.getCompleteBlock());
        m_active_stakes.insert(stake.getKey());
        m_script_to_active_stakes[scriptToStr(stake.getScript())].insert(txid);
        CAmount amount = stake.getAmount();
        size_t periodIdx = stake.getPeriodIdx();
        m_amounts_by_periods[periodIdx] += amount;
        m_stakes_map[txid] = stake;
        if (height == stake.getCompleteBlock()) {
            --m_num_complete_stakes;
        } else {
            --m_num_early_withdrawn_stakes;
        }
    } else {
        return false;
    }
    return true;
}
bool CStakesDBCache::setBestBlock(const uint256& hash) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    m_best_block_hash = hash;
    return true;
}

bool CStakesDBCache::drop() {
    if (m_viewonly) {
        return false;
    }
    m_base_db->dropCache();
    return true;
}

uint256 CStakesDBCache::getBestBlock() const {
    if (m_viewonly) {
        return m_base_db->getBestBlock();
    }
    return m_best_block_hash;
}

const AmountByPeriodArray& CStakesDBCache::getAmountsByPeriods() const {
    if (m_viewonly) {
        return m_base_db->getAmountsByPeriods();
    }
    return m_amounts_by_periods;
}

// This method assumes that nothing else from stake reward is modified. Checks are not performed because they would need
// DB lookups, which are too expensive.
bool CStakesDBCache::updateStakeEntry(const CStakesDbEntry &entry) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    m_stakes_map[entry.getKey()] = entry;
    return true;
}

CFreeTxInfo CStakesDBCache::getFreeTxInfoForScript(const CScript &script) const {
    if (m_viewonly) {
        return m_base_db->getFreeTxInfoForScript(script);
    }
    auto it = m_free_tx_info.find(scriptToStr(script));
    if (it == m_free_tx_info.end())
        return {};
    return it->second;
}

CFreeTxInfo CStakesDBCache::createFreeTxInfoForScript(const CScript& script, const uint32_t nHeight, const Consensus::Params& params) {
    CFreeTxInfo result;
    std::string scriptString = scriptToStr(script);
    CFreeTxInfo freeTxInfoEx = getFreeTxInfoForScript(script);
    if (freeTxInfoEx.isValid()) {
        LogPrintf("ERROR: Free tx info for script %s already exists\n", scriptString);
        return result;
    }

    StakeIdsSet activeStakes = getActiveStakeIdsForScript(script);
    if (activeStakes.empty()) {
        return result;
    }
    uint32_t limit = calculateFreeTxLimit(activeStakes, params);
    result = CFreeTxInfo(limit, nHeight, activeStakes);

    return result;
}

bool CStakesDBCache::registerFreeTransaction(const CScript& script, const CTransaction& tx, const uint32_t nHeight, const Consensus::Params& params) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    std::string scriptString = scriptToStr(script);
    CFreeTxInfo freeTxInfo = getFreeTxInfoForScript(script);
    if (!freeTxInfo.isValid()) {
        freeTxInfo = createFreeTxInfoForScript(script, nHeight, params);
        if (!freeTxInfo.isValid()) {
            return false;
        }
        m_free_tx_info.insert(std::make_pair(scriptString, freeTxInfo));
    }
    if (nHeight > 0 && freeTxInfo.getCurrentWindowStartHeight() == 0) {
        freeTxInfo.setCurrentWindowStartHeight(nHeight);
    }
    if (nHeight != 0 && nHeight > freeTxInfo.getCurrentWindowEndHeight()) {
        return false;
    }

    std::set<uint256> activeStakeIdsChain = getActiveStakeIdsForScript(script);
    if (activeStakeIdsChain != freeTxInfo.getActiveStakeIds()) {
        freeTxInfo.setActiveStakeIds(activeStakeIdsChain);
        freeTxInfo.setLimit(calculateFreeTxLimit(activeStakeIdsChain, params));
    }
    if (nHeight == 0 && !freeTxInfo.addUnconfirmedTxId(tx.GetHash(), getTransactionVSize(tx))) {
        return false;
    }
    if (nHeight > 0) {
        if (!freeTxInfo.increaseUsedConfirmedLimit(getTransactionVSize(tx))) {
            return false;
        }
        freeTxInfo.removeUnconfirmedTxId(tx.GetHash());
    }
    m_free_tx_info[scriptString] = freeTxInfo;
    return true;
}

bool CStakesDBCache::undoFreeTransaction(const CScript& script, const CTransaction& tx) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    CFreeTxInfo freeTxInfo = getFreeTxInfoForScript(script);
    if (freeTxInfo.isValid()) {
        freeTxInfo.decreaseUsedConfirmedLimit(getTransactionVSize(tx));
        m_free_tx_info[scriptToStr(script)] = freeTxInfo;
    }
    return true;
}

void CStakesDBCache::addFreeTxSizeForBlock(const uint256& hash, const uint32_t size) {
    m_block_free_tx_size_map[hash] = size;
}

uint32_t CStakesDBCache::getFreeTxSizeForBlock(const uint256& hash) const {
    if (m_viewonly) {
        return m_base_db->getFreeTxSizeForBlock(hash);
    }
    auto it = m_block_free_tx_size_map.find(hash);
    if(it == m_block_free_tx_size_map.end()) {
        return m_base_db->getFreeTxSizeForBlock(hash);
    }
    return it->second;
}


uint32_t CStakesDBCache::calculateFreeTxLimitForScript(const CScript &script, const Consensus::Params& params) const {
    std::set<uint256> active_stake_ids = getActiveStakeIdsForScript(script);
    return calculateFreeTxLimit(active_stake_ids, params);
}

uint32_t CStakesDBCache::calculateFreeTxLimit(const std::set<uint256> &activeStakeIds, const Consensus::Params& params) const {
    std::vector<CStakesDbEntry> stakes = {};
    stakes.reserve(activeStakeIds.size());
    for (auto& stake_id : activeStakeIds) {
        stakes.push_back(getStakeDbEntry(stake_id));
    }
    return CFreeTxLimitCalculator::CalculateFreeTxLimitForStakes(params, stakes);
}

bool CStakesDBCache::removeInvalidFreeTxInfos(uint32_t nHeight, bool fReorg) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    for(auto it = m_free_tx_info.begin(); it != m_free_tx_info.end(); ) {
        if (!fReorg && it->second.getCurrentWindowEndHeight() <= nHeight) {
            m_free_tx_info_end_height_map[nHeight].push_back(std::pair<std::string, uint32_t>(it->first, it->second.getUsedConfirmedLimit()));
            it = m_free_tx_info.erase(it);
        } else if (fReorg && it->second.getCurrentWindowStartHeight() > nHeight) {
            it = m_free_tx_info.erase(it);
        } else {
                ++it;
            }
        }
    return true;
}

ClosedFreeTxWindowInfoVector CStakesDBCache::getFreeTxInfosCompletedAtHeight(uint32_t nHeight) {
    if (m_viewonly) {
        return m_base_db->getFreeTxWindowsCompletedAtHeight(nHeight);
    }

    auto it = m_free_tx_info_end_height_map.find(nHeight);
    if(it == m_free_tx_info_end_height_map.end()) {
        return m_base_db->getFreeTxWindowsCompletedAtHeight(nHeight);
    }
    return it->second;
}

bool CStakesDBCache::reactivateFreeTxInfos(uint32_t nHeight, const Consensus::Params& params) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    ClosedFreeTxWindowInfoVector closedFreeTxWinV = getFreeTxInfosCompletedAtHeight(nHeight);
    for (auto& closedFreeTxWin : closedFreeTxWinV) {
        CScript script = scriptFromStr(closedFreeTxWin.first);
        CFreeTxInfo freeTxInfo = createFreeTxInfoForScript(script, nHeight - stakingParams::BLOCKS_PER_DAY, params);
        freeTxInfo.setUsedConfirmedLimit(closedFreeTxWin.second);
        m_free_tx_info.insert(std::make_pair(scriptToStr(script), freeTxInfo));
    }
    m_free_tx_window_end_heights_to_remove.insert(nHeight);
    return true;
}

CAmount CStakesDBCache::getGpForScript(const CScript& script) const {
    if (m_viewonly) {
        return m_base_db->getGpForScript(script);
    }
    auto it = m_gp_map.find(scriptToStr(script));
    if(it == m_gp_map.end()) {
        return m_base_db->getGpForScript(script);
    }
    return it->second;
}

bool CStakesDBCache::setGpForScript(const CScript& script, const CAmount amount) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    m_gp_map[scriptToStr(script)] = amount;
    return true;
}