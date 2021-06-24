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


