#ifndef ELCASH_STAKINGPARAMS_H
#define ELCASH_STAKINGPARAMS_H

#include <amount.h>

constexpr CAmount MIN_STAKING_AMOUNT = 5 * COIN;
constexpr float STAKING_EARLY_WITHDRAWAL_PENALTY_PERCENTAGE = 3.0;
constexpr std::array<size_t, 4> STAKING_PERIOD = {
        4320,
        12960,
        25920,
        51840
};
constexpr std::array<float, 4> STAKING_REWARD_PERCENTAGE = {
        5.0,
        6.0,
        7.25,
        10.0
};



#endif //ELECTRIC_CASH_STAKINGPARAMS_H
