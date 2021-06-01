#ifndef ELECTRIC_CASH_STAKING_POOL_H
#define ELECTRIC_CASH_STAKING_POOL_H
#include "amount.h"

class StakingPool {

private:
    static StakingPool* srpInstance;
    CAmount balance = 0;
    StakingPool(){};

public:
    StakingPool(StakingPool& copy) = delete;
    static StakingPool* getInstance();
    void increaseBalance(CAmount amount);
    void decreaseBalance(CAmount amount);
    CAmount getBalance();
};



#endif //ELECTRIC_CASH_STAKING_POOL_H
