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
    uint32_t completeBlock, numOutput;
    uint8_t periodIdx;
    bool complete = false;
    uint256 txid;
    CScript script;
    bool valid = false;
    bool active;
public:
    CStakesDbEntry() {};
    CStakesDbEntry(const uint256 txidIn, CAmount amountIn, const CAmount rewardIn, const unsigned int periodIn, const unsigned int completeBlockIn, const unsigned int numOutputIn, const CScript scriptIn, const bool activeIn);
    CAmount getAmount() const { return amount; }
    void setReward(const CAmount rewardIn);
    CAmount getReward() const { return reward; }
    unsigned int getPeriodIdx() const { return periodIdx; }
    void setComplete(bool completeFlag);
    bool isComplete() const { return complete; }
    unsigned int getCompleteBlock() const { return completeBlock; }
    unsigned int getNumOutput() const { return numOutput; }
    uint256 getKey() const { return txid; }
    std::string getKeyHex() const { return txid.GetHex(); }
    CScript getScript() const { return script; }
    void setKey(const uint256 txidIn) { txid = txidIn; }
    bool isValid() const { return valid; }
    void setInactive() { active = false; }
    void setActive() { active = true; }
    bool isActive() const { return active; }

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << amount;
        s << reward;
        s << periodIdx;
        s << completeBlock;
        s << numOutput;
        s << complete;
        s << script;
        s << valid;
        s << active;
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> amount;
        s >> reward;
        s >> periodIdx;
        s >> completeBlock;
        s >> numOutput;
        s >> complete;
        s >> script;
        s >> valid;
        s >> active;
    }

};

typedef std::map<uint256, CStakesDbEntry> StakesMap;
typedef std::map<std::string, std::set<uint256>> AddressMap;
typedef std::set<uint256> StakeIdsSet;

class CStakesDB {
private:
    size_t current_cache_size {};
    // TODO set up max size param
    static const size_t max_cache_size {1000};
    // TODO check db params
    CDBWrapper db_wrapper;
    StakesMap stakes_map {};
    AddressMap address_to_stakes_map {};
    StakeIdsSet active_stakes {};
public:
    CStakesDB(size_t cache_size_bytes, bool in_memory, bool should_wipe, const std::string& leveldb_name);
    CStakesDB(const CStakesDB& other) = delete;
    bool addStakeEntry(const CStakesDbEntry& entry);
    CStakesDbEntry getStakeDbEntry(uint256 txid);
    CStakesDbEntry getStakeDbEntry(std::string txid);
    bool deactivateStake(uint256 txid);
    void flushDB();
    ~CStakesDB() { flushDB(); }
    std::set<uint256> getStakeIdsForAddress(std::string address);
    void addAddressToMap(std::string address, uint256 txid);
    std::vector<CStakesDbEntry> getAllActiveStakes();
};

#endif //ELECTRIC_CASH_STAKES_DB_H
