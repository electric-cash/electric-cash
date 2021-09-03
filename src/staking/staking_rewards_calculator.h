#ifndef ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H
#define ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H

#include <staking/stakes_db.h>
#include <staking/stakingparams.h>
#include <staking/staking_pool.h>
#include <consensus/block_rewards.h>

class CStakingRewardsCalculator {
public:
    static CAmount CalculateRewardForStake(const CChainParams& params, double globalRewardCoefficient, const CStakesDbEntry& stake);
    static CAmount CalculatePenaltyForStake(const CStakesDbEntry& stake);
    static double CalculateGlobalRewardCoefficient(const CChainParams& params, CStakesDBCache& stakes, uint32_t height, bool goingBackward = false);
};


#endif //ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H
