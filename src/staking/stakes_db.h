#ifndef ELECTRIC_CASH_STAKES_DB_H
#define ELECTRIC_CASH_STAKES_DB_H
#include <amount.h>
#include <uint256.h>
#include <script/script.h>
#include <staking/staking_pool.h>
#include <staking/stakingparams.h>
#include <map>
#include <set>
#include <sync.h>
#include <dbwrapper.h>
#include <sstream>
#include <string_view>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/array.hpp>
#include <chainparams.h>


static const size_t MAX_CACHE_SIZE{450 * (1 << 20)};
static const size_t DEFAULT_BATCH_SIZE{45 * (1 << 20)};

class CStakesDBCache;
extern RecursiveMutex cs_main;

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
    CStakesDbEntry(const uint256& txidIn, CAmount amountIn, const CAmount rewardIn, const unsigned int periodIn, const unsigned int completeBlockIn, const unsigned int numOutputIn, const CScript scriptIn, const bool activeIn);
    CAmount getAmount() const { return amount; }
    void setReward(const CAmount rewardIn);
    CAmount getReward() const { return reward; }
    unsigned int getPeriodIdx() const { return periodIdx; }
    void setComplete(bool completeFlag);
    bool isComplete() const { return complete; }
    unsigned int getCompleteBlock() const { return completeBlock; }
    unsigned int getDepositBlock(const CChainParams& params) const { return completeBlock - params.StakingPeriod()[periodIdx] + 1; };
    unsigned int getNumOutput() const { return numOutput; }
    uint256 getKey() const { return txid; }
    std::string getKeyHex() const { return txid.GetHex(); }
    CScript getScript() const { return script; }
    void setKey(const uint256& txidIn) { txid = txidIn; }
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

template <typename T>
class CSerializer {
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive& archive, const unsigned int version) {
        archive & varToSave;
    }
    T& varToSave;
    std::string dbKey;
    CDBWrapper& dbWrapper;
public:
    CSerializer(T& varToSave, CDBWrapper& dbWrapper, const std::string& dbKey):
        varToSave{varToSave}, dbWrapper(dbWrapper), dbKey{dbKey} {}
    void dump() {

        std::stringstream ss;
        boost::archive::binary_oarchive oa{ss};
        oa << *this;

        dbWrapper.Write(dbKey, ss.str());
    }
    void load() {

        std::string value;
        dbWrapper.Read(dbKey, value);

        if(!value.empty()) {
            std::stringstream ss{value};
            boost::archive::binary_iarchive ia{ss};
            ia >> *this;
        }
    }
};

typedef std::map<uint256, CStakesDbEntry> StakesMap;
typedef std::map<std::string, std::set<uint256>> ScriptToStakesMap;
typedef std::set<uint256> StakeIdsSet;
typedef std::map<uint32_t, StakeIdsSet> StakesCompletedAtBlockHeightMap;
typedef std::vector<CStakesDbEntry> StakesVector;
typedef std::array<CAmount, stakingParams::NUM_STAKING_PERIODS> AmountByPeriodArray;

class CStakesDB {
private:
    CDBWrapper m_db_wrapper GUARDED_BY(cs_main);
    ScriptToStakesMap m_script_to_active_stakes {};
    StakeIdsSet m_active_stakes {};
    StakesCompletedAtBlockHeightMap m_stakes_completed_at_block_height {};
    AmountByPeriodArray m_amounts_by_periods {0, 0, 0, 0};
    CStakingPool m_staking_pool;
    uint256 m_best_block_hash;

    // mutex to assure that no more than one editable cache is open.
    std::mutex m_cache_mutex;

public:
    explicit CStakesDB(size_t cache_size_bytes, bool in_memory, bool should_wipe, const std::string& leveldb_name);
    CStakesDB() = delete;
    CStakesDB(const CStakesDB& other) = delete;
    CStakesDbEntry getStakeDbEntry(const uint256& txid) const;
    CStakesDbEntry getStakeDbEntry(std::string txid) const;
    bool removeStakeEntry(const uint256& txid);
    bool flushDB(CStakesDBCache* cache);
    std::set<uint256> getActiveStakeIdsForScript(const CScript& script);
    StakesVector getAllActiveStakes() const;
    StakesVector getStakesCompletedAtHeight(const uint32_t height);
    CStakingPool& stakingPool();
    uint256 getBestBlock();
    const AmountByPeriodArray& getAmountsByPeriods() const { return m_amounts_by_periods; }
    const ScriptToStakesMap& getScriptMap() const { return m_script_to_active_stakes;}
    const StakeIdsSet& getActiveStakesSet() const { return m_active_stakes; }
    const StakesCompletedAtBlockHeightMap& getStakesCompletedAtBlockHeightMap() const { return m_stakes_completed_at_block_height; }
    void initCache();
    void dropCache();
    void initHelpStates();
    void dumpHelpStates();
    void verify();
    void verifyFlushState();
    void verifyTotalAmounts() const;
};


class CStakesDBCache {
    friend CStakesDB;
private:
    CStakesDB* m_base_db;
    bool m_viewonly;
    bool m_flushed = false;
    uint256 m_best_block_hash;
    size_t m_current_cache_size {};
    size_t m_max_cache_size{};
    StakesMap m_stakes_map {};
    CStakingPool m_staking_pool;
    StakeIdsSet m_active_stakes {};
    StakesCompletedAtBlockHeightMap m_stakes_completed_at_block_height {};
    ScriptToStakesMap m_script_to_active_stakes {};
    StakeIdsSet m_stakes_to_remove {};
    AmountByPeriodArray m_amounts_by_periods {0, 0, 0, 0};

public:
    CStakesDBCache(CStakesDB* db, bool fViewOnly = false, size_t max_cache_size=MAX_CACHE_SIZE);
    CStakesDBCache(const CStakesDBCache& other) = delete;
    bool addNewStakeEntry(const CStakesDbEntry& entry);
    CStakesDbEntry getStakeDbEntry(const uint256& txid) const;
    CStakesDbEntry getStakeDbEntry(std::string txid) const;
    bool removeStakeEntry(const uint256& txid);
    bool deactivateStake(const uint256& txid, const bool fSetComplete);
    bool reactivateStake(const uint256& txid, const uint32_t height);
    bool updateStakeEntry(const CStakesDbEntry& entry);
    CStakingPool& stakingPool();
    bool flushDB();
    ~CStakesDBCache() { drop(); }
    std::set<uint256> getActiveStakeIdsForScript(const CScript& script) const;
    StakesVector getAllActiveStakes() const;
    StakesVector getStakesCompletedAtHeight(const uint32_t height);
    bool setBestBlock(const uint256& hash);
    uint256 getBestBlock() const;
    const AmountByPeriodArray& getAmountsByPeriods() const;
    bool drop();
};

#endif //ELECTRIC_CASH_STAKES_DB_H