#include <staking/stakes_db.h>


CStakesDbEntry::CStakesDbEntry(uint256 txid, CAmount amount, CAmount reward, unsigned int period, unsigned int completeBlock, unsigned int numOutput, CScript script) {
    this->txid = txid;
    this->amount = amount;
    this->reward = reward;
    this->period = period;
    this->completeBlock = completeBlock;
    this->numOutput = numOutput;
    this->script = script;
}

void CStakesDbEntry::setReward(CAmount reward) {
    this->reward = reward;
}

void CStakesDbEntry::setComplete(bool completeFlag) {
    complete = completeFlag;
}

bool CStakesDB::addStakeEntry(CStakesDbEntry &entry) {
    try {
        stakes_map[entry.getKey()] = entry;
        current_cache_size += sizeof(entry);
        if (current_cache_size >= max_cache_size)
            flushDB();
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add stake of id %s to database\n", entry.getKeyHex());
        return false;
    }
}

CStakesDbEntry CStakesDB::getStakeDbEntry(uint256 txid) {
    StakesMap::iterator it = stakes_map.find(txid);
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

bool CStakesDB::removeStakeEntry(uint256 txid) {
    StakesMap::iterator it = stakes_map.find(txid);
    if(it == stakes_map.end()) {
        if(!db_wrapper.Erase(txid)) {
            LogPrintf("ERROR: Cannot remove stake of id %s from database\n", txid.GetHex());
            return false;
        }
        return true;
    }
    else
        stakes_map.erase(it);
    return true;
}

void CStakesDB::flushDB() {
    StakesMap::iterator it = stakes_map.begin();
    while(it != stakes_map.end()){
        if(!db_wrapper.Write(it->first, it->second)) {
            LogPrintf("ERROR: Cannot flush stake of id %s to file\n", it->first.GetHex());
            break;
        }
        size_t size = sizeof(it->second);
        stakes_map.erase(it++);
        current_cache_size -= size;
    }
}

void CStakesDB::addAddressToMap(std::string address, uint256 txid) {
    AddressMap::iterator it = address_to_stakes_map.find(address);
    if(it == address_to_stakes_map.end())
        address_to_stakes_map[address] = std::set<uint256> {txid};
    else
        it->second.insert(txid);
}

std::set<uint256> CStakesDB::getStakeIdsForAddress(std::string address) {
    AddressMap::iterator it = address_to_stakes_map.find(address);
    if(it == address_to_stakes_map.end()) {
        LogPrintf("ERROR: Address %s not found\n", address);
        return std::set<uint256> {};
    }
    return it->second;
}

CStakesDB::CStakesDB(size_t cache_size_bytes, bool in_memory, bool should_wipe, const std::string& leveldb_name) :
                    db_wrapper(GetDataDir() / leveldb_name, cache_size_bytes, in_memory, should_wipe, false) {
}
