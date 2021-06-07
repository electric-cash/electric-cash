#ifndef ELECTRIC_CASH_STAKING_POOL_H
#define ELECTRIC_CASH_STAKING_POOL_H
#include <amount.h>
#include <mutex>

class CStakingPool {

private:
    static CStakingPool* srpInstance;
    static std::mutex mutex_;
    CAmount balance = 0;
    CStakingPool(){};

public:
    CStakingPool(CStakingPool& copy) = delete;
    void operator=(const CStakingPool&) = delete;
    static CStakingPool* getInstance();
    static CStakingPool* getInstance(CAmount balance);
    void increaseBalance(CAmount amount);
    void increaseBalanceForNewBlock(int nHeight);
    void decreaseBalance(CAmount amount);
    CAmount getBalance();
    void setBalance(CAmount balance);
};

#endif //ELECTRIC_CASH_STAKING_POOL_H
