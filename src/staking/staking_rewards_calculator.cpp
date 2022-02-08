#include "staking_rewards_calculator.h"
#include <staking/stakes_db.h>

CAmount CStakingRewardsCalculator::CalculateBlockRewardForStake(const CChainParams& params, double globalRewardCoefficient, const CStakesDbEntry &stake) {
    CAmount amount = stake.getAmount();
    size_t periodIdx = stake.getPeriodIdx();
    double percentage = params.GetConsensus().stakingRewardPercentage[periodIdx];
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
        max_potential_payout += params.GetConsensus().stakingRewardPercentage[i] / 100.0 * static_cast<double>(totalStakedByPeriod[i]) / stakingParams::BLOCKS_PER_YEAR;
    }
    max_potential_payout = std::floor(max_potential_payout);
    return std::min(1.0, max_possible_payout / max_potential_payout);
}

uint32_t CFreeTxLimitCalculator::CalculateFreeTxLimitForStakes(const Consensus::Params& params,
                                                               const std::vector<CStakesDbEntry> &stakes) {
    uint32_t limit = 0;
    for (auto& stake : stakes) {
        CAmount amount = stake.getAmount();
        size_t idx = stake.getPeriodIdx();
        limit += std::floor((static_cast<double>(amount) / stakingParams::MIN_STAKING_AMOUNT - 1.0) * params.freeTxLimitCoefficient[idx] + params.freeTxBaseLimit);
    }
    return limit;
}

CAmount CGpCalculator::CalculateGpRewardForStake(const CChainParams& params, const CStakesDbEntry &stake) {
    CAmount amount = stake.getAmount();
    size_t periodIdx = stake.getPeriodIdx();
    double percentage = params.GetConsensus().stakingRewardPercentage[periodIdx];
    CAmount rewardForBlock = stakingParams::GP_TO_STAKING_COEFFICIENT * static_cast<CAmount>(floor(percentage / 100.0 * static_cast<double>(amount)) / static_cast<double>(stakingParams::BLOCKS_PER_YEAR));
    return rewardForBlock;
}
