#ifndef ELECTRIC_CASH_STAKING_POOL_H
#define ELECTRIC_CASH_STAKING_POOL_H
#include <amount.h>
#include <serialize.h>

class CStakingPool {

private:
    CAmount m_balance = 0;
    CStakingPool(){};

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(m_balance);
    }
    CStakingPool(CAmount amount);
    void increaseBalance(CAmount amount);
    void increaseBalanceForNewBlock(int nHeight);
    void decreaseBalance(CAmount amount);
    void decreaseBalanceForHeight(int nHeight);
    CAmount getBalance();
    void setBalance(CAmount balance);
};

#endif //ELECTRIC_CASH_STAKING_POOL_H
