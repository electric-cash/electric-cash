#include "staking_rewards_calculator.h"

CAmount CStakingRewardsCalculator::CalculateRewardForStake(double globalRewardCoefficient, const CStakesDbEntry &stake) {
    CAmount amount = stake.getAmount();
    size_t periodIdx = stake.getPeriodIdx();
    double percentage = stakingParams::STAKING_REWARD_PERCENTAGE[periodIdx];
    auto rewardForBlock = static_cast<CAmount>(floor(globalRewardCoefficient * percentage / 100.0 * static_cast<double>(amount)) / static_cast<double>(stakingParams::BLOCKS_PER_YEAR));
    return rewardForBlock;
}

CAmount CStakingRewardsCalculator::CalculatePenaltyForStake(const CStakesDbEntry &stake) {
    return static_cast<CAmount>(floor(
            stakingParams::STAKING_EARLY_WITHDRAWAL_PENALTY_PERCENTAGE *
            static_cast<double>(stake.getAmount()) / 100.0));
}

double CStakingRewardsCalculator::CalculateGlobalRewardCoefficient(CStakesDB& stakes, uint32_t height) {
    double max_possible_payout = std::floor(static_cast<double>(stakes.stakingPool().getBalance()) /
            stakingParams::STAKING_POOL_EXPIRY_BLOCKS + static_cast<double>(GetStakingRewardForHeight(height)));

    // TODO(mtwaro): THIS MUST BE CHANGED - store the mapping {period => total_staked_for_period} inside the staking DB and move the following logic there.
    std::array<CAmount, stakingParams::NUM_STAKING_PERIODS> totalStakedByPeriod = {0, 0, 0, 0};
    for (const auto& stake : stakes.getAllActiveStakes()) {
        CAmount amount = stake.getAmount();
        size_t periodIdx = stake.getPeriodIdx();
        totalStakedByPeriod[periodIdx] += amount;
    }

    double max_potential_payout = 0;
    for (int i = 0; i < stakingParams::NUM_STAKING_PERIODS; ++i) {
        max_potential_payout += stakingParams::STAKING_REWARD_PERCENTAGE[i] / 100.0 * static_cast<double>(totalStakedByPeriod[i]) / stakingParams::BLOCKS_PER_YEAR;
    }
    max_potential_payout = std::floor(max_potential_payout);
    return std::min(1.0, max_possible_payout / max_potential_payout);
}
