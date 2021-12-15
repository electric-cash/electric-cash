#ifndef ELCASH_STAKINGPARAMS_H
#define ELCASH_STAKINGPARAMS_H

#include <amount.h>
#include <cstddef>

namespace stakingParams {
    constexpr size_t NUM_STAKING_PERIODS = 4;
    constexpr size_t BLOCKS_PER_DAY = 144;
    constexpr size_t BLOCKS_PER_YEAR = 360 * BLOCKS_PER_DAY;
    constexpr size_t STAKING_POOL_EXPIRY_BLOCKS = 180 * BLOCKS_PER_DAY;
    constexpr CAmount MIN_STAKING_AMOUNT = 5 * COIN;
    constexpr double STAKING_EARLY_WITHDRAWAL_PENALTY_PERCENTAGE = 3.0;
}



#endif //ELECTRIC_CASH_STAKINGPARAMS_H
