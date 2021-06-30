#ifndef ELECTRIC_CASH_STAKES_DB_H
#define ELECTRIC_CASH_STAKES_DB_H
#include <amount.h>
#include <uint256.h>
#include <script/script.h>
#include <map>
#include <set>
#include <dbwrapper.h>


class CStakesDbEntry {
private:
    CAmount amount, reward;
    unsigned int period, completeBlock, numOutput;
    bool complete = false;
    uint256 txid;
    CScript script;
public:
    CStakesDbEntry() {};
    CStakesDbEntry(uint256 txid, CAmount amount, CAmount reward, unsigned int period, unsigned int completeBlock, unsigned int numOutput, CScript script);
    CAmount getAmount() { return amount; }
    void setReward(CAmount reward);
    CAmount getReward() { return reward; }
    void setComplete(bool completeFlag);
    bool isComplete() { return complete; }
    unsigned int getCompleteBlock() { return completeBlock; }
    unsigned int getNumOutput() { return numOutput; }
    uint256 getKey() { return txid; }
    std::string getKeyHex() { return txid.GetHex(); }
    CScript getScript() { return script; }
    void setKey(uint256 txid) { this->txid = txid; }

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << amount;
        s << reward;
        s << period;
        s << completeBlock;
        s << numOutput;
        s << complete;
        s << script;
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> amount;
        s >> reward;
        s >> period;
        s >> completeBlock;
        s >> numOutput;
        s >> complete;
        s >> script;
    }

};

typedef std::map<uint256, CStakesDbEntry> StakesMap;
typedef std::map<std::string, std::set<uint256>> AddressMap;

class CStakesDB {
private:
    size_t current_cache_size {};
    // TODO set up max size param
    size_t max_cache_size {1000};
    // TODO check db params
    CDBWrapper db_wrapper;
    StakesMap stakes_map {};
    AddressMap address_to_stakes_map {};
public:
    CStakesDB(size_t cache_size_bytes, bool in_memory, bool should_wipe, const std::string& leveldb_name);
    CStakesDB(const CStakesDB& other) = delete;
    bool addStakeEntry(CStakesDbEntry& entry);
    CStakesDbEntry getStakeDbEntry(uint256 txid);
    CStakesDbEntry getStakeDbEntry(std::string txid);
    bool removeStakeEntry(uint256 txid);
    void flushDB();
    ~CStakesDB() { flushDB(); }
    std::set<uint256> getStakeIdsForAddress(std::string address);
    void addAddressToMap(std::string address, uint256 txid);
};

#endif //ELECTRIC_CASH_STAKES_DB_H
