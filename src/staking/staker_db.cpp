#include <staking/staker_db.h>

CStakerDbEntry::CStakerDbEntry(uint256 txid, CAmount amount, CAmount reward, unsigned int period, unsigned int completeBlock, unsigned int numOutput, CScript script) {
    this->txid = txid;
    this->amount = amount;
    this->reward = reward;
    this->period = period;
    this->completeBlock = completeBlock;
    this->numOutput = numOutput;
    this->script = script;
}

void CStakerDbEntry::setReward(CAmount reward) {
    this->reward = reward;
}

void CStakerDbEntry::setComplete(bool completeFlag) {
    complete = completeFlag;
}

bool CStakerDB::addStakerEntry(CStakerDbEntry& entry) {
    try {
        staker_map[entry.getKey()] = entry;
        current_cache_size += sizeof(entry);
        if (current_cache_size >= max_cache_size)
            flushDB();
        return true;
    }catch(...) {
        LogPrintf("ERROR: Cannot add staker of id %s to database\n", entry.getKeyHex());
        return false;
    }
}

CStakerDbEntry CStakerDB::getStakerDbEntry(uint256 txid) {
    StakersMap::iterator it = staker_map.find(txid);
    CStakerDbEntry output;
    if(it == staker_map.end()) {
        if(staker_db.Read(txid, output))
            output.setKey(txid);
        else
            LogPrintf("ERROR: Cannot get staker of id %s from database\n", txid.GetHex());
        return output;
    }
    return it->second;
}

CStakerDbEntry CStakerDB::getStakerDbEntry(std::string txid) {
    return getStakerDbEntry(uint256S(txid));
}

bool CStakerDB::removeStakerEntry(uint256 txid) {
    StakersMap::iterator it = staker_map.find(txid);
    if(it == staker_map.end()) {
        if(!staker_db.Erase(txid)) {
            LogPrintf("ERROR: Cannot remove staker of id %s from database\n", txid.GetHex());
            return false;
        }
        return true;
    }
    else
        staker_map.erase(it);
    return true;
}

void CStakerDB::flushDB() {
    StakersMap::iterator it = staker_map.begin();
    while(it != staker_map.end()){
        if(!staker_db.Write(it->first, it->second)) {
            LogPrintf("ERROR: Cannot flush staker of id %s to file\n", it->first.GetHex());
            break;
        }
        size_t size = sizeof(it->second);
        staker_map.erase(it++);
        current_cache_size -= size;
    }
}

void CStakerDB::addAddressToMap(std::string address, uint256 txid) {
    AddressMap::iterator it = address_to_stakers_map.find(address);
    if(it == address_to_stakers_map.end())
        address_to_stakers_map[address] = std::set<uint256> {txid};
    else
        it->second.insert(txid);
}

std::set<uint256> CStakerDB::getStakerIdsForAddress(std::string address) {
    AddressMap::iterator it = address_to_stakers_map.find(address);
    if(it == address_to_stakers_map.end()) {
        LogPrintf("ERROR: Address %s not found\n", address);
        return std::set<uint256> {};
    }
    return it->second;
}
