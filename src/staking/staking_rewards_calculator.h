#ifndef ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H
#define ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H

#include <staking/stakes_db.h>
#include <staking/stakingparams.h>
#include <staking/staking_pool.h>
#include <consensus/block_rewards.h>

class CStakingRewardsCalculator {
public:
    static CAmount CalculateRewardForStake(double globalRewardCoefficient, const CStakesDbEntry &stake);
    static double CalculateGlobalRewardCoefficient(CStakesDB& stakes, uint32_t height);
};


#endif //ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H
