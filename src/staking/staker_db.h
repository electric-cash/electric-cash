#ifndef ELECTRIC_CASH_STAKER_DB_H
#define ELECTRIC_CASH_STAKER_DB_H
#include <amount.h>
#include <uint256.h>
#include <script/script.h>
#include <map>
#include <set>
#include <dbwrapper.h>


class CStakerDbEntry {
private:
    CAmount amount, reward;
    unsigned int period, completeBlock, numOutput;
    bool complete = false;
    uint256 txid;
    CScript script;
public:
    CStakerDbEntry() {};
    CStakerDbEntry(uint256 txid, CAmount amount, CAmount reward, unsigned int period, unsigned int completeBlock, unsigned int numOutput, CScript script);
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

static const std::string STAKERS_DB_NAME{"stakersDB"};
typedef std::map<uint256, CStakerDbEntry> StakersMap;
typedef std::map<std::string, std::set<uint256>> AddressMap;

class CStakerDB {
private:
    size_t current_cache_size {};
    // TODO set up max size param
    size_t max_cache_size {1000};
    // TODO check db params
    CDBWrapper staker_db {GetDataDir() / "stakers" / STAKERS_DB_NAME, (1 << 20), true, false, false};
    StakersMap staker_map {};
    AddressMap address_to_stakers_map {};
public:
    bool addStakerEntry(CStakerDbEntry& entry);
    CStakerDbEntry getStakerDbEntry(uint256 txid);
    CStakerDbEntry getStakerDbEntry(std::string txid);
    bool removeStakerEntry(uint256 txid);
    void flushDB();
    ~CStakerDB() { flushDB(); }
    std::set<uint256> getStakerIdsForAddress(std::string address);
    void addAddressToMap(std::string address, uint256 txid);
};

#endif //ELECTRIC_CASH_STAKER_DB_H
