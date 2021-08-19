// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/block_rewards.h>
#include <net.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    int maxReductions = 39;
    CAmount nInitialSubsidy = 500 * COIN;

    CAmount nPreviousSubsidy = nInitialSubsidy + 1; // for height == 0
    BOOST_CHECK_EQUAL(nPreviousSubsidy, nInitialSubsidy + 1);
    
    
    for (int nReductions = 0; nReductions < maxReductions; nReductions++) {
        int nHeight = nReductions + REWARD_REDUCTION_PERIOD;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, 1);
        BOOST_CHECK(nSubsidy <= nInitialSubsidy);
        nPreviousSubsidy = nSubsidy;
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(maxReductions * REWARD_REDUCTION_PERIOD + BOOTSTRAP_PERIOD, 1), 0);
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    CAmount nSum = 0;
    CAmount stakingBalance = 0;
    for (int nHeight = 0; nHeight < 2000000; nHeight += 100) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, 1);
        CAmount stakingReward = GetStakingRewardForHeight(nHeight);
        BOOST_CHECK(nSubsidy <= 500 * COIN);
        BOOST_CHECK(stakingReward <= FRACTION_OF_STAKING_REWARD * GetBlockRewardForHeight(nHeight));
        nSum += nSubsidy * 100;
        stakingBalance += stakingReward * 100;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{2100000000000000} - stakingBalance);
}

static bool ReturnFalse() { return false; }
static bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
