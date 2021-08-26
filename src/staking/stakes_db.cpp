#include <staking/stakes_db.h>


CStakesDbEntry::CStakesDbEntry(const uint256 txidIn, const CAmount amountIn, const CAmount rewardIn, const unsigned int periodIn, const unsigned int completeBlockIn, const unsigned int numOutputIn, const CScript scriptIn, const bool activeIn) {
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
        m_staking_pool(0), leveldb_name{leveldb_name} {
            initHelpStates();
}

CStakesDbEntry CStakesDB::getStakeDbEntry(uint256 txid) {
    CStakesDbEntry output;
    if(m_db_wrapper.Read(txid, output))
        output.setKey(txid);
    else
        LogPrintf("ERROR: Cannot get stake of id %s from database\n", txid.GetHex());
    return output;
}

CStakesDbEntry CStakesDB::getStakeDbEntry(std::string txid) {
    return getStakeDbEntry(uint256S(txid));
}

bool CStakesDB::removeStakeEntry(uint256 txid) {
    if (!m_db_wrapper.Erase(txid)) {
        LogPrintf("ERROR: Cannot remove stake of id %s from database\n", txid.GetHex());
        return false;
    }
    return true;
}

StakesVector CStakesDB::getAllActiveStakes() {
    std::vector<CStakesDbEntry> res;
    for (auto id : m_active_stakes) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

bool CStakesDB::flushDB(CStakesDBCache* cache) {
    LOCK(cs_main);
    bool flushOngoing = true;
    // TODO(mtwaro): flush the value above here

    CDBBatch batch{m_db_wrapper};
    batch.Clear();
    size_t batch_size = static_cast<size_t>(gArgs.GetArg("-dbbatchsize", DEFAULT_BATCH_SIZE));

    m_address_to_stakes_map = cache->m_address_to_stakes_map;
    m_active_stakes = cache->m_active_stakes;
    m_staking_pool = cache->m_staking_pool;
    m_stakes_completed_at_block_height = cache->m_stakes_completed_at_block_height;
    m_best_block_hash = cache->m_best_block_hash;
    dumpHelpStates();
    // TODO(mtwaro): flush the above fields here, return false if failed
    m_db_wrapper.Write(BEST_HASH_HEADER_PREFIX, m_best_block_hash);

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

    flushOngoing = false;
    //TODO(mtwaro): flush above value here
    dropCache();
    return true;
}

std::set<uint256> CStakesDB::getStakeIdsForAddress(std::string address) {
    auto it = m_address_to_stakes_map.find(address);
    if(it == m_address_to_stakes_map.end()) {
        LogPrintf("ERROR: Address %s not found\n", address);
        return std::set<uint256> {};
    }
    return it->second;
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
        m_db_wrapper.Read(BEST_HASH_HEADER_PREFIX, m_best_block_hash);
    }
    return m_best_block_hash;
}

void CStakesDB::initHelpStates() {
    {
        CSerializer<AddressMap> serializer{m_address_to_stakes_map, leveldb_name, "m_address_to_stakes_map"};
        serializer.load();
    }
    {
        CSerializer<StakeIdsSet> serializer{m_active_stakes, leveldb_name, "m_active_stakes"};
        serializer.load();
    }
    {
        CSerializer<StakesCompletedAtBlockHeightMap> serializer{m_stakes_completed_at_block_height, leveldb_name, "m_stakes_completed_at_block_height"};
        serializer.load();
    }
    {
        CSerializer<uint256> serializer{m_best_block_hash, leveldb_name, "m_best_block_hash"};
        serializer.load();
    }
}

void CStakesDB::dumpHelpStates() {
    {
        CSerializer<AddressMap> serializer{m_address_to_stakes_map, leveldb_name, "m_address_to_stakes_map"};
        serializer.dump();
    }
    {
        CSerializer<StakeIdsSet> serializer{m_active_stakes, leveldb_name, "m_active_stakes"};
        serializer.dump();
    }
    {
        CSerializer<StakesCompletedAtBlockHeightMap> serializer{m_stakes_completed_at_block_height, leveldb_name, "m_stakes_completed_at_block_height"};
        serializer.dump();
    }
    {
        CSerializer<uint256> serializer{m_best_block_hash, leveldb_name, "m_best_block_hash"};
        serializer.dump();
    }
}

CStakesDBCache::CStakesDBCache(CStakesDB* db, bool fViewOnly, size_t max_cache_size): m_viewonly(fViewOnly),
        m_base_db{db}, m_max_cache_size{max_cache_size}, m_staking_pool(m_base_db->stakingPool()) {
    if (!m_viewonly) {
        m_base_db->initCache();
        m_active_stakes.insert(m_base_db->getActiveStakesSet().begin(),  m_base_db->getActiveStakesSet().end());
        m_stakes_completed_at_block_height.insert(m_base_db->getStakesCompletedAtBlockHeightMap().begin(),  m_base_db->getStakesCompletedAtBlockHeightMap().end());
        m_address_to_stakes_map.insert(m_base_db->getAddressMap().begin(),  m_base_db->getAddressMap().end());
        m_best_block_hash = m_base_db->getBestBlock();
    }
}

CStakingPool& CStakesDBCache::stakingPool() {
    return m_staking_pool;
}

bool CStakesDBCache::addStakeEntry(const CStakesDbEntry& entry) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    try {
        m_stakes_map[entry.getKey()] = entry;
        m_current_cache_size += entry.estimateSize();
        updateActiveStakes(entry);
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add stake of id %s to database\n", entry.getKeyHex());
        return false;
    }
}

bool CStakesDBCache::removeStakeEntry(uint256 txid) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    m_active_stakes.erase(txid);
    auto it = m_stakes_map.find(txid);
    if (it != m_stakes_map.end()) {
        m_stakes_map.erase(it);
    }
    m_stakes_to_remove.insert(txid);
    return true;
}

std::set<uint256> CStakesDBCache::getStakeIdsForAddress(std::string address) {
    if (m_viewonly) {
        return m_base_db->getStakeIdsForAddress(address);
    }
    auto it = m_address_to_stakes_map.find(address);
    if(it == m_address_to_stakes_map.end()) {
        LogPrintf("ERROR: Address %s not found\n", address);
        return std::set<uint256> {};
    }
    return it->second;
}

CStakesDbEntry CStakesDBCache::getStakeDbEntry(uint256 txid) {
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

CStakesDbEntry CStakesDBCache::getStakeDbEntry(std::string txid) {
    return getStakeDbEntry(uint256S(txid));
}

bool CStakesDBCache::deactivateStake(uint256 txid, const bool fSetComplete) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    CStakesDbEntry stake = getStakeDbEntry(txid);
    if (stake.isValid() && stake.isActive()) {
        stake.setInactive();
        stake.setComplete(fSetComplete);
        m_active_stakes.erase(txid);
        addStakeEntry(stake);
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

bool CStakesDBCache::addAddressToMap(std::string address, uint256 txid) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    auto it = m_address_to_stakes_map.find(address);
    if(it == m_address_to_stakes_map.end())
        m_address_to_stakes_map[address] = std::set<uint256> {txid};
    else
        it->second.insert(txid);
    return true;
}

StakesVector CStakesDBCache::getAllActiveStakes() {
    if (m_viewonly) {
        return m_base_db->getAllActiveStakes();
    }
    std::vector<CStakesDbEntry> res;
    for (auto id : m_active_stakes) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid() && stake.isActive());
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

bool CStakesDBCache::reactivateStake(const uint256 txid, const uint32_t height) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    CStakesDbEntry stake = getStakeDbEntry(txid);
    if (stake.isValid() && !stake.isActive()) {
        stake.setActive();
        stake.setComplete(stake.getCompleteBlock() > height);
        m_active_stakes.insert(stake.getKey());
        addStakeEntry(stake);
    } else {
        return false;
    }
    return true;
}

bool CStakesDBCache::updateActiveStakes(const CStakesDbEntry& entry) {
    if (m_viewonly) {
        LogPrintf("ERROR: Cannot modify a viewonly cache");
        return false;
    }
    if (entry.isActive()) {
        m_active_stakes.insert(entry.getKey());
    }
    return true;
}

bool CStakesDBCache::setBestBlock(const uint256 hash) {
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
    LogPrintf("DEB: dropped cache %s\n", static_cast<void*>(this));
    m_base_db->dropCache();
    return true;
}

uint256 CStakesDBCache::getBestBlock() const {
    if (m_viewonly) {
        return m_base_db->getBestBlock();
    }
    return m_best_block_hash;
}
