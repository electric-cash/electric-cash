#ifndef ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H
#define ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H

#include <staking/stakingparams.h>
#include <staking/staking_pool.h>
#include <consensus/block_rewards.h>

class CStakesDbEntry;
class CChainParams;
class CStakesDBCache;
namespace Consensus
{
    class Params;
}

class CStakingRewardsCalculator {
public:
    static CAmount CalculateBlockRewardForStake(const CChainParams& params, double globalRewardCoefficient, const CStakesDbEntry& stake);
    static CAmount CalculatePenaltyForStake(const CStakesDbEntry& stake);
    static double CalculateGlobalRewardCoefficient(const CChainParams& params, CStakesDBCache& stakes, uint32_t height, bool goingBackward = false);
};

class CFreeTxLimitCalculator {
public:
    static uint32_t CalculateFreeTxLimitForStakes(const Consensus::Params& params, const std::vector<CStakesDbEntry>& stakes);
};

class CGpCalculator {
public:
    static CAmount CalculateGpRewardForStake(const CChainParams& params, const CStakesDbEntry& stake);
};


#endif //ELECTRIC_CASH_STAKING_REWARDS_CALCULATOR_H
