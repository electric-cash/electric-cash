#include <consensus/block_rewards.h>


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