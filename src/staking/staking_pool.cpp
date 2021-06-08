#include <staking/staking_pool.h>
#include <cassert>
#include <consensus/block_rewards.h>


CStakingPool* CStakingPool::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (srpInstance == nullptr)
        srpInstance = new CStakingPool();
    return srpInstance;
}

CStakingPool* CStakingPool::getInstance(CAmount balance) {
    CStakingPool* instance = CStakingPool::getInstance();
    instance->balance = balance;
    return instance;
}

void CStakingPool::increaseBalance(CAmount amount) {
    balance += amount;
}

void CStakingPool::increaseBalanceForNewBlock(int nHeight) {
    balance += GetStakingRewardForHeight(nHeight);
}

void CStakingPool::decreaseBalance(CAmount amount) {
    assert((balance - amount) > 0);
    balance -= amount;
}

CAmount CStakingPool::getBalance() {
    return balance;
}

void CStakingPool::setBalance(CAmount balance) {
    this->balance = balance;
}

CStakingPool* CStakingPool::srpInstance = nullptr;
std::mutex CStakingPool::mutex_;