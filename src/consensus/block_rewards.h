// Copyright (c) 2020 Electric Cash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ELCASH_CONSENSUS_BLOCK_REWARDS_H
#define ELCASH_CONSENSUS_BLOCK_REWARDS_H

#include <array>
#include <cstdint>
#include <amount.h>


const uint32_t BOOTSTRAP_PERIOD = 4200;
const uint32_t REWARD_REDUCTION_PERIOD = 52500;
const uint32_t NUMBER_OF_REWARD_REDUCTIONS = 39;
const float FRACTION_OF_STAKING_REWARD = 0.1;


const std::array<CAmount, NUMBER_OF_REWARD_REDUCTIONS> REWARD_AMOUNTS = {
    50000000000,
    7500000000,
    7000000000,
    6500000000,
    5500000000,
    4000000000,
    2500000000,
    1500000000,
    750000000,
    375000000,
    187500000,
    93750000,
    46875000,
    23437500,
    11718750,
    5859375,
    2929688,
    1464844,
    732422,
    366210,
    183104,
    91552,
    45776,
    22888,
    11444,
    5722,
    2861,
    1430,
    715,
    358,
    179,
    90,
    45,
    23,
    12,
    6,
    3,
    2,
    1
};

CAmount GetBlockRewardForHeight(uint32_t height);

CAmount GetStakingRewardForHeight(uint32_t height);

#endif //ELCASH_CONSENSUS_BLOCK_REWARDS_H