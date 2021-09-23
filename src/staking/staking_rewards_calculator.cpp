#include "staking_rewards_calculator.h"
#include <staking/stakes_db.h>

CAmount CStakingRewardsCalculator::CalculateBlockRewardForStake(const CChainParams& params, double globalRewardCoefficient, const CStakesDbEntry &stake) {
    CAmount amount = stake.getAmount();
    size_t periodIdx = stake.getPeriodIdx();
    double percentage = params.StakingRewardPercentage()[periodIdx];
    auto rewardForBlock = static_cast<CAmount>(floor(globalRewardCoefficient * percentage / 100.0 * static_cast<double>(amount)) / static_cast<double>(stakingParams::BLOCKS_PER_YEAR));
    return rewardForBlock;
}

CAmount CStakingRewardsCalculator::CalculatePenaltyForStake(const CStakesDbEntry &stake) {
    return static_cast<CAmount>(floor(
            stakingParams::STAKING_EARLY_WITHDRAWAL_PENALTY_PERCENTAGE *
            static_cast<double>(stake.getAmount()) / 100.0));
}

double CStakingRewardsCalculator::CalculateGlobalRewardCoefficient(const CChainParams& params, CStakesDBCache& stakes, uint32_t height, bool goingBackward) {
    // In case of reorg (going backward) the balance of staking pool is different than when going forward, so the max_possible_reward
    // must be calculated using a different algebraic formula
    double max_possible_payout = 0;
    if (goingBackward) {
        max_possible_payout = std::floor(static_cast<double>(stakes.stakingPool().getBalance() +
                stakingParams::STAKING_POOL_EXPIRY_BLOCKS * GetStakingRewardForHeight(height)) / static_cast<double>(stakingParams::STAKING_POOL_EXPIRY_BLOCKS - 1));
    } else {
        max_possible_payout = std::floor(static_cast<double>(stakes.stakingPool().getBalance()) /
                stakingParams::STAKING_POOL_EXPIRY_BLOCKS + static_cast<double>(GetStakingRewardForHeight(height)));
    }

    const AmountByPeriodArray totalStakedByPeriod = stakes.getAmountsByPeriods();
    double max_potential_payout = 0;
    for (int i = 0; i < stakingParams::NUM_STAKING_PERIODS; ++i) {
        max_potential_payout += params.StakingRewardPercentage()[i] / 100.0 * static_cast<double>(totalStakedByPeriod[i]) / stakingParams::BLOCKS_PER_YEAR;
    }
    max_potential_payout = std::floor(max_potential_payout);
    return std::min(1.0, max_possible_payout / max_potential_payout);
}
