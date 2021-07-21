#ifndef ELCASH_STAKINGPARAMS_H
#define ELCASH_STAKINGPARAMS_H

#include <amount.h>
namespace stakingParams {
    constexpr size_t NUM_STAKING_PERIODS = 4;
    constexpr size_t BLOCKS_PER_YEAR = 360 * 144;
    constexpr size_t STAKING_POOL_EXPIRY_BLOCKS = 180 * 144;
    constexpr CAmount MIN_STAKING_AMOUNT = 5 * COIN;
    constexpr double STAKING_EARLY_WITHDRAWAL_PENALTY_PERCENTAGE = 3.0;
    constexpr std::array<size_t, NUM_STAKING_PERIODS> STAKING_PERIOD = {
            4320,
            12960,
            25920,
            51840
    };
    constexpr std::array<double, NUM_STAKING_PERIODS> STAKING_REWARD_PERCENTAGE = {
            5.0,
            6.0,
            7.25,
            10.0
    };
}



#endif //ELECTRIC_CASH_STAKINGPARAMS_H
