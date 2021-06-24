#ifndef ELECTRIC_CASH_STAKER_DB_H
#define ELECTRIC_CASH_STAKER_DB_H
#include <amount.h>
#include <uint256.h>
#include <script/script.h>


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
    std::string getKey() { return txid.GetHex(); }
    CScript getScript() { return script; }

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << txid;
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
        s >> txid;
        s >> amount;
        s >> reward;
        s >> period;
        s >> completeBlock;
        s >> numOutput;
        s >> complete;
        s >> script;
    }

};

#endif //ELECTRIC_CASH_STAKER_DB_H
