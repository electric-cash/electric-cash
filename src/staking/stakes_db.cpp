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
        db_wrapper(GetDataDir() / leveldb_name, cache_size_bytes, in_memory, should_wipe, false) {
}

bool CStakesDB::addStakeEntry(const CStakesDbEntry& entry) {
    try {
        stakes_map[entry.getKey()] = entry;
        current_cache_size += entry.estimateSize();
        if (current_cache_size >= max_cache_size)
            flushDB();
        if (entry.isActive()) {
            active_stakes.insert(entry.getKey());
        }
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add stake of id %s to database\n", entry.getKeyHex());
        return false;
    }
}



CStakesDbEntry CStakesDB::getStakeDbEntry(uint256 txid) {
    auto it = stakes_map.find(txid);
    CStakesDbEntry output;
    if(it == stakes_map.end()) {
        if(db_wrapper.Read(txid, output))
            output.setKey(txid);
        else
            LogPrintf("ERROR: Cannot get stake of id %s from database\n", txid.GetHex());
        return output;
    }
    return it->second;
}

CStakesDbEntry CStakesDB::getStakeDbEntry(std::string txid) {
    return getStakeDbEntry(uint256S(txid));
}

bool CStakesDB::deactivateStake(const uint256 txid, const bool fSetComplete) {
    CStakesDbEntry stake = getStakeDbEntry(txid);
    if (stake.isValid() && stake.isActive()) {
        stake.setInactive();
        stake.setComplete(fSetComplete);
        active_stakes.erase(txid);
        addStakeEntry(stake);
    } else {
        return false;
    }
    return true;
}

void CStakesDB::flushDB() {
    CDBBatch batch{db_wrapper};
    batch.Clear();
    // TODO: set batch size params
    auto batch_size = static_cast<size_t>(0.1 * max_cache_size);

    auto it = stakes_map.begin();
    while(it != stakes_map.end()) {
        batch.Write(it->first, it->second);
        if(batch.SizeEstimate() > batch_size) {
            LogPrint(BCLog::STAKESDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            db_wrapper.WriteBatch(batch);
            batch.Clear();
        }
        stakes_map.erase(it++);
    }

    LogPrint(BCLog::STAKESDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    db_wrapper.WriteBatch(batch);
    batch.Clear();

}

void CStakesDB::addAddressToMap(std::string address, uint256 txid) {
    auto it = address_to_stakes_map.find(address);
    if(it == address_to_stakes_map.end())
        address_to_stakes_map[address] = std::set<uint256> {txid};
    else
        it->second.insert(txid);
}

std::set<uint256> CStakesDB::getStakeIdsForAddress(std::string address) {
    auto it = address_to_stakes_map.find(address);
    if(it == address_to_stakes_map.end()) {
        LogPrintf("ERROR: Address %s not found\n", address);
        return std::set<uint256> {};
    }
    return it->second;
}

std::vector<CStakesDbEntry> CStakesDB::getAllActiveStakes() {
    std::vector<CStakesDbEntry> res;
    for (auto id : active_stakes) {
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
            if (stake.getCompleteBlock() <= height) {
                stake.setComplete(false);
            active_stakes.erase(txid);
            if (!db_wrapper.Write(txid, stake)) {
                LogPrintf("ERROR: Cannot deactivate stake of id %s to file\n", txid.GetHex());
                return false;
            }
        } else {
            return false;
        }
        return true;
    }
}
