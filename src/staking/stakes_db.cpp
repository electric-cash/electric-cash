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
        m_db_wrapper(GetDataDir() / leveldb_name, cache_size_bytes, in_memory, should_wipe, false), m_staking_pool(0) {
}

bool CStakesDB::addStakeEntry(const CStakesDbEntry& entry) {
    try {
        updateActiveStakes(entry);
        StakesMap map{{entry.getKey(), entry}};
        flushDB(map);
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add stake of id %s to database\n", entry.getKeyHex());
        return false;
    }
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

bool CStakesDB::deactivateStake(const uint256 txid, const bool fSetComplete) {
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

bool CStakesDB::removeStakeEntry(uint256 txid) {
    m_active_stakes.erase(txid);
    if (!m_db_wrapper.Erase(txid)) {
        LogPrintf("ERROR: Cannot remove stake of id %s from database\n", txid.GetHex());
        return false;
    }
    return true;
}

void CStakesDB::flushDB(StakesMap& stakes_map) {
    CDBBatch batch{m_db_wrapper};
    batch.Clear();
    size_t batch_size = static_cast<size_t>(gArgs.GetArg("-dbbatchsize", DEFAULT_BATCH_SIZE));

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
}

void CStakesDB::addAddressToMap(std::string address, uint256 txid) {
    auto it = m_address_to_stakes_map.find(address);
    if(it == m_address_to_stakes_map.end())
        m_address_to_stakes_map[address] = std::set<uint256> {txid};
    else
        it->second.insert(txid);
}

std::set<uint256> CStakesDB::getStakeIdsForAddress(std::string address) {
    auto it = m_address_to_stakes_map.find(address);
    if(it == m_address_to_stakes_map.end()) {
        LogPrintf("ERROR: Address %s not found\n", address);
        return std::set<uint256> {};
    }
    return it->second;
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

StakesVector CStakesDB::getStakesCompletedAtHeight(const uint32_t height) {
    std::vector<CStakesDbEntry> res;
    for (const auto& id : m_stakes_completed_at_block_height[height]) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

bool CStakesDB::reactivateStake(const uint256 txid, const uint32_t height) {
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

void CStakesDB::updateActiveStakes(const CStakesDbEntry& entry) {
    if (entry.isActive()) {
        m_active_stakes.insert(entry.getKey());
    }
}

CStakingPool& CStakesDB::stakingPool() {
    return m_staking_pool;
}

CStakingPool& CStakesDBCache::stakingPool() {
    return m_staking_pool;
}

CStakesDBCache::CStakesDBCache(CStakesDB* db, size_t max_cache_size): m_base_db{db}, m_max_cache_size{max_cache_size},
        m_staking_pool(m_base_db->stakingPool()) {
    m_active_stakes = m_base_db->getActiveStakesSet();
    m_stakes_completed_at_block_height = m_base_db->getStakesCompletedAtBlockHeightMap();
    m_address_to_stakes_map = m_base_db->getAddressMap();
}

bool CStakesDBCache::addStakeEntry(const CStakesDbEntry& entry) {
    try {
        m_stakes_map[entry.getKey()] = entry;
        m_current_cache_size += entry.estimateSize();
        updateActiveStakes(entry);
        if (m_current_cache_size >= m_max_cache_size)
            m_base_db->flushDB(m_stakes_map);
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add stake of id %s to database\n", entry.getKeyHex());
        return false;
    }
}

bool CStakesDBCache::removeStakeEntry(uint256 txid) {
    auto it = m_stakes_map.find(txid);
    if (it != m_stakes_map.end()) {
        m_stakes_map.erase(it);
        m_active_stakes.erase(txid);
        return true;
    }
    else
        return m_base_db->removeStakeEntry(txid);
}

CStakesDbEntry CStakesDBCache::getStakeDbEntry(uint256 txid) {
    auto it = m_stakes_map.find(txid);
    CStakesDbEntry output;
    if(it == m_stakes_map.end())
        return m_base_db->getStakeDbEntry(txid);
    return it->second;
}

CStakesDbEntry CStakesDBCache::getStakeDbEntry(std::string txid) {
    return getStakeDbEntry(uint256S(txid));
}

bool CStakesDBCache::deactivateStake(uint256 txid, const bool fSetComplete) {
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

void CStakesDBCache::flushDB() {
    m_base_db->flushDB(m_stakes_map);
}

void CStakesDBCache::addAddressToMap(std::string address, uint256 txid) {
    m_base_db->addAddressToMap(address, txid);
}

std::set<uint256> CStakesDBCache::getStakeIdsForAddress(std::string address) {
    return m_base_db->getStakeIdsForAddress(address);
}

StakesVector CStakesDBCache::getAllActiveStakes() {
    std::vector<CStakesDbEntry> res;
    for (auto id : m_active_stakes) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

StakesVector CStakesDBCache::getStakesCompletedAtHeight(const uint32_t height) {
    std::vector<CStakesDbEntry> res;
    for (const auto& id : m_stakes_completed_at_block_height[height]) {
        CStakesDbEntry stake = getStakeDbEntry(id);
        assert(stake.isValid());
        res.push_back(stake);
    }
    return res;
}

bool CStakesDBCache::reactivateStake(const uint256 txid, const uint32_t height) {
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

void CStakesDBCache::updateActiveStakes(const CStakesDbEntry& entry) {
    if (entry.isActive()) {
        m_active_stakes.insert(entry.getKey());
    }
}
