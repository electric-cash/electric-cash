#ifndef ELECTRIC_CASH_STAKES_DB_H
#define ELECTRIC_CASH_STAKES_DB_H
#include <amount.h>
#include <uint256.h>
#include <script/script.h>
#include <staking/stakingparams.h>
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
    unsigned int getDepositBlock() const { return completeBlock - stakingParams::STAKING_PERIOD[periodIdx] + 1; };
    unsigned int getNumOutput() const { return numOutput; }
    uint256 getKey() const { return txid; }
    std::string getKeyHex() const { return txid.GetHex(); }
    CScript getScript() const { return script; }
    void setKey(const uint256 txidIn) { txid = txidIn; }
    bool isValid() const { return valid; }
    void setInactive() { active = false; }
    void setActive() { active = true; }
    bool isActive() const { return active; }
    size_t estimateSize() const {
        return 2 * sizeof(CAmount) +
               2 * sizeof(uint32_t) +
               sizeof(uint8_t) +
               3 * sizeof(bool) +
               txid.size() +
               script.size();
    }

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
typedef std::map<uint32_t, StakeIdsSet> StakesCompletedAtBlockHeightMap;
typedef std::vector<CStakesDbEntry> StakesVector;

class CStakesDB {
private:
    size_t current_cache_size {};
    // TODO move cache size var into separate class
    static const size_t max_cache_size {500 * (1 << 20)};
    // TODO check db params
    CDBWrapper db_wrapper;
    StakesMap stakes_map {};
    AddressMap address_to_stakes_map {};
    StakeIdsSet active_stakes {};
    StakesCompletedAtBlockHeightMap stakes_completed_at_block_height {};

public:
    CStakesDB(size_t cache_size_bytes, bool in_memory, bool should_wipe, const std::string& leveldb_name);
    CStakesDB(const CStakesDB& other) = delete;
    bool addStakeEntry(const CStakesDbEntry& entry);
    CStakesDbEntry getStakeDbEntry(uint256 txid);
    CStakesDbEntry getStakeDbEntry(std::string txid);
    bool removeStakeEntry(uint256 txid);
    bool deactivateStake(uint256 txid, const bool fSetComplete);
    bool reactivateStake(uint256 txid, uint32_t height);
    void flushDB();
    ~CStakesDB() { flushDB(); }
    std::set<uint256> getStakeIdsForAddress(std::string address);
    void addAddressToMap(std::string address, uint256 txid);
    StakesVector getAllActiveStakes();
    StakesVector getStakesCompletedAtHeight(const uint32_t height);
};

#endif //ELECTRIC_CASH_STAKES_DB_H
