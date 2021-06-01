#include "staking_pool.h"
#include <cassert>


StakingPool* StakingPool::getInstance() {
    if (srpInstance == nullptr)
        srpInstance = new StakingPool();
    return srpInstance;
}

void StakingPool::increaseBalance(CAmount amount) {
    balance += amount;
}

void StakingPool::decreaseBalance(CAmount amount) {
    assert((balance - amount) > 0);
    balance -= amount;
}

CAmount StakingPool::getBalance() {
    return balance;
}

StakingPool* StakingPool::srpInstance = nullptr;
