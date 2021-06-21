// Copyright (c) 2017-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/transaction.h>
#include <test/util/setup_common.h>
#include <script/script.h>
#include <boost/test/unit_test.hpp>

const unsigned char vchKey0[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
const CPubKey dummyPubkey = CPubKey(vchKey0, vchKey0 + 32);

BOOST_FIXTURE_TEST_SUITE(staking_tests, BasicTestingSetup)

/**
 * Check staking deposit transaction recognition
 */
BOOST_AUTO_TEST_CASE(recognize_valid_deposit_tx)
{
    const size_t stakingHeaderSize = 5;
    const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, 0x01};
    CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
    CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

    CMutableTransaction depositTx;
    depositTx.nVersion = 1;
    depositTx.vin.resize(1);
    depositTx.vout.resize(2);
    depositTx.vout[0].scriptPubKey = stakingTxHeader;
    depositTx.vout[1].nValue = 10 * COIN;
    depositTx.vout[1].scriptPubKey = stakingDepositScript;
    CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
    StakingTransactionType txType = stakingTxParser.GetStakingTxType();

    BOOST_CHECK(txType == StakingTransactionType::DEPOSIT);
}

BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_corrupted_varint)
    {
        const size_t stakingHeaderSize = 5;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0xfe, 0x01};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();
        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_no_period)
    {
        const size_t stakingHeaderSize = 4;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_invalid_period)
    {
        const size_t stakingHeaderSize = 5;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, 0x04};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_invalid_header)
    {
        const size_t stakingHeaderSize = 5;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, 0x01, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, 0x01};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_correct_header_no_data)
    {
        const size_t stakingHeaderSize = 3;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_output_index_too_small)
    {
        const size_t stakingHeaderSize = 5;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x00, 0x01};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_output_index_too_big)
    {
        const size_t stakingHeaderSize = 5;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x02, 0x01};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 10 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_deposit_tx_amount_too_small)
    {
        const size_t stakingHeaderSize = 5;
        const unsigned char stakingDepHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, 0x01};
        CScript stakingTxHeader = CScript(stakingDepHeader, stakingDepHeader + stakingHeaderSize);
        CScript stakingDepositScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 3 * COIN;
        depositTx.vout[1].scriptPubKey = stakingDepositScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_valid_burn_tx)
    {
        const size_t stakingHeaderSize = 11;
        const unsigned char stakingBurnHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_BURN_SUBHEADER, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}; // 42.94967296 ELC to be burned
        CScript stakingTxHeader = CScript(stakingBurnHeader, stakingBurnHeader + stakingHeaderSize);
        CScript dummyScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 3 * COIN;
        depositTx.vout[1].scriptPubKey = dummyScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();

        BOOST_CHECK(txType == StakingTransactionType::BURN);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_burn_tx_invalid_amount)
    {
        const size_t stakingHeaderSize = 11;
        const unsigned char stakingBurnHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_BURN_SUBHEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00}; // 22.5 M ELC to burn
        CScript stakingTxHeader = CScript(stakingBurnHeader, stakingBurnHeader + stakingHeaderSize);
        CScript dummyScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 3 * COIN;
        depositTx.vout[1].scriptPubKey = dummyScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();
        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

    BOOST_AUTO_TEST_CASE(recognize_invalid_burn_tx_corrupted_amount)
    {
        const size_t stakingHeaderSize = 10;
        const unsigned char stakingBurnHeader[stakingHeaderSize] = {OP_RETURN, STAKING_TX_HEADER, STAKING_TX_BURN_SUBHEADER, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}; // script too short
        CScript stakingTxHeader = CScript(stakingBurnHeader, stakingBurnHeader + stakingHeaderSize);
        CScript dummyScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(dummyPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction depositTx;
        depositTx.nVersion = 1;
        depositTx.vin.resize(1);
        depositTx.vout.resize(2);
        depositTx.vout[0].scriptPubKey = stakingTxHeader;
        depositTx.vout[1].nValue = 3 * COIN;
        depositTx.vout[1].scriptPubKey = dummyScript;
        CStakingTransactionParser stakingTxParser(MakeTransactionRef(CTransaction(depositTx)));
        StakingTransactionType txType = stakingTxParser.GetStakingTxType();
        BOOST_CHECK(txType == StakingTransactionType::NONE);
    }

BOOST_AUTO_TEST_SUITE_END()


