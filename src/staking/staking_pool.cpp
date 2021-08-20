#include <staking/staking_pool.h>
#include <logging.h>
#include <consensus/block_rewards.h>


CStakingPool::CStakingPool(CAmount amount) {
    m_balance = amount;
}

void CStakingPool::increaseBalance(CAmount amount) {
    m_balance += amount;
}

void CStakingPool::increaseBalanceForNewBlock(int nHeight) {
    m_balance += GetStakingRewardForHeight(nHeight);
}

void CStakingPool::decreaseBalance(CAmount amount) {
    if ((m_balance - amount) > 0) {
        m_balance -= amount;
    }
    else
        LogPrintf("ERROR: Current staking pool balance %d can not be decreased by %d\n", m_balance, amount);
}

void CStakingPool::decreaseBalanceForHeight(int nHeight) {
    CAmount amount = GetStakingRewardForHeight(nHeight);
    this->decreaseBalance(amount);
}

CAmount CStakingPool::getBalance() {
    return m_balance;
}

void CStakingPool::setBalance(CAmount balance) {
    this->m_balance = balance;
}
