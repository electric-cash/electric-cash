#include <consensus/block_rewards.h>
#include <math.h>


CAmount GetBlockRewardForHeight(uint32_t height)
 {
     for(uint32_t i = 0; i < NUMBER_OF_REWARD_REDUCTIONS; i++)
     {
         if (height < BOOTSTRAP_PERIOD + i * REWARD_REDUCTION_PERIOD)
         {
             return REWARD_AMOUNTS[i];
         }
     }
     return 0;
 }

CAmount GetStakingRewardForHeight(uint32_t height)
{
    return (CAmount) floor(FRACTION_OF_STAKING_REWARD * GetBlockRewardForHeight(height));

}
