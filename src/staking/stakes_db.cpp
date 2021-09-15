#include <staking/stakes_db.h>


namespace DBHeaders {
    const std::string ADDRESS_TO_STAKES_MAP = "address_to_stakes_map";
    const std::string ACTIVE_STAKES = "active_stakes";
    const std::string STAKES_COMPLETED_AT_BLOCK_HEIGHT = "stakes_completed_at_block_height";
    const std::string AMOUNT_BY_PERIOD = "amounts_by_periods";
    const std::string BEST_BLOCK_HASH = "best_block_hash";
    const std::string STAKING_POOL = "staking_pool";
    const std::string FLUSH_ONGOING = "flush_ongoing";
}

std::string scriptToStr(const CScript& script) {
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << script;
    return ds.str();
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
    m_amounts_by_periods = cache->m_amounts_by_periods;
    m_best_block_hash = cache->m_best_block_hash;
    dumpHelpStates();

    StakesMap stakes_map = cache->m_stakes_map;
    StakesMap::iterator it = stakes_map.begin();
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

    m_db_wrapper.Read(DBHeaders::STAKING_POOL, m_staking_pool);
    m_db_wrapper.Read(DBHeaders::BEST_BLOCK_HASH, m_best_block_hash);
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

    m_db_wrapper.Write(DBHeaders::STAKING_POOL, m_staking_pool);
    m_db_wrapper.Write(DBHeaders::BEST_BLOCK_HASH, m_best_block_hash);
}

CStakesDBCache::CStakesDBCache(CStakesDB* db, bool fViewOnly, size_t max_cache_size): m_viewonly(fViewOnly),
        m_base_db{db}, m_max_cache_size{max_cache_size}, m_staking_pool(m_base_db->stakingPool()) {
    if (!m_viewonly) {
        m_base_db->initCache();
        m_active_stakes.insert(m_base_db->getActiveStakesSet().begin(),  m_base_db->getActiveStakesSet().end());
        m_stakes_completed_at_block_height.insert(m_base_db->getStakesCompletedAtBlockHeightMap().begin(),  m_base_db->getStakesCompletedAtBlockHeightMap().end());
        m_script_to_active_stakes.insert(m_base_db->getScriptMap().begin(), m_base_db->getScriptMap().end());
        m_best_block_hash = m_base_db->getBestBlock();
        m_amounts_by_periods = m_base_db->getAmountsByPeriods();
    }
}

CStakingPool& CStakesDBCache::stakingPool() {
    return m_staking_pool;
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
    m_script_to_active_stakes[scriptToStr(stake.getScript())].erase(stakeId);
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
        m_script_to_active_stakes[scriptToStr(stake.getScript())].erase(txid);
        CAmount amount = stake.getAmount();
        size_t periodIdx = stake.getPeriodIdx();
        m_amounts_by_periods[periodIdx] -= amount;
        m_stakes_map[txid] = stake;
    } else {
        return false;
    }
    return true;
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
// DB lookups, which are too expensive. If
bool CStakesDBCache::updateStakeEntry(const CStakesDbEntry &entry) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    m_stakes_map[entry.getKey()] = entry;
    return true;
}