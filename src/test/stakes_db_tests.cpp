#include <dbwrapper.h>
#include <staking/stakes_db.h>
#include <test/util/setup_common.h>
#include <boost/test/unit_test.hpp>
#include <vector>
#include <boost/serialization/vector.hpp>

const size_t DEFAULT_CACHE_SIZE = 1000;
const std::string DEFAULT_DB_NAME = "stakes";


void script_fixture(CScript& script) {
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();
    script.clear();
    script << ToByteVector(pubkey) << OP_CHECKSIG;
}

BOOST_FIXTURE_TEST_SUITE(stakes_db_tests, BasicTestingSetup)

void check_entry_equals(CStakesDbEntry input, CStakesDbEntry output) {
    BOOST_CHECK(input.getAmount() == output.getAmount());
    BOOST_CHECK(input.getReward() == output.getReward());
    BOOST_CHECK(input.isComplete() == output.isComplete());
    BOOST_CHECK(input.getCompleteBlock() == output.getCompleteBlock());
    BOOST_CHECK(input.getNumOutput() == output.getNumOutput());
    BOOST_CHECK(input.getScript() == output.getScript());
    BOOST_CHECK(input.getKey() == output.getKey());
}

BOOST_AUTO_TEST_CASE(stakes_db_entry_serialization) {
    CScript script;
    script_fixture(script);
    CStakesDbEntry input(InsecureRand256(), 10, 15, 20, 25, 30, script, true);
    CStakesDbEntry output;

    fs::path ph = GetDataDir() / "test_stakes_db";
    CDBWrapper dbw(ph, (1 << 20), true, false, false);

    BOOST_CHECK(dbw.Write(input.getKey(), input));
    BOOST_CHECK(dbw.Read(input.getKey(), output));
    output.setKey(input.getKey());
    check_entry_equals(input, output);
}

/*
 * TODO(mtwaro): rewrite this test case to include new deactivation logic.
 * BOOST_AUTO_TEST_CASE(stakes_db_crud_operation) {
    CScript script;
    script_fixture(script);
    uint256 key = InsecureRand256();
    CStakesDbEntry input(key, 10, 15, 20, 25, 30, script, true);
    CStakesDbEntry output;
    CStakesDB db(DEFAULT_CACHE_SIZE, false, false, DEFAULT_DB_NAME);

    // test agains cache crud
    BOOST_CHECK(db.addStakeEntry(input));
    output = db.getStakeDbEntry(key);
    check_entry_equals(input, output);
    BOOST_CHECK(db.deactivateStake(key, false));
    output = db.getStakeDbEntry(key);
    BOOST_CHECK(input.getKey() != output.getKey());

    // test against levedb crud
    BOOST_CHECK(db.addNewStakeEntry(input));
    db.flushDB();
    output = db.getStakeDbEntry(key);
    check_entry_equals(input, output);
    BOOST_CHECK(db.deactivateStake(key, false));
    output = db.getStakeDbEntry(key);
    BOOST_CHECK(input.getKey() != output.getKey());
}*/

BOOST_AUTO_TEST_CASE(script_mapping) {
    std::set<uint256> list_of_ids;
    uint256 txid1 = InsecureRand256(), txid2 = InsecureRand256();
    CScript script(0x1234), otherScript(0x4321);
    {
        CStakesDB db(DEFAULT_CACHE_SIZE, false, false, DEFAULT_DB_NAME);
        CStakesDBCache cache(&db);
        std::set<uint256> list_of_ids;
        CStakesDbEntry entry1(txid1, 5 * COIN, 0, 0, 1, 1, script, true);
        cache.addNewStakeEntry(entry1);
        list_of_ids = cache.getActiveStakeIdsForScript(script);
        BOOST_CHECK(list_of_ids.size() == 1);

        CStakesDbEntry entry2(txid2, 5 * COIN, 0, 0, 1, 1, script, true);
        cache.addNewStakeEntry(entry2);
        list_of_ids = cache.getActiveStakeIdsForScript(script);
        BOOST_CHECK(list_of_ids.size() == 2);
        cache.flushDB();
    }
    // reload DB from disk
    CStakesDB db(DEFAULT_CACHE_SIZE, false, false, DEFAULT_DB_NAME);
    CStakesDBCache cache(&db);

    list_of_ids = cache.getActiveStakeIdsForScript(script);
    BOOST_CHECK(list_of_ids.size() == 2);

    cache.deactivateStake(txid1, true);
    list_of_ids = cache.getActiveStakeIdsForScript(script);
    BOOST_CHECK(list_of_ids.size() == 1);

    list_of_ids = cache.getActiveStakeIdsForScript(otherScript);
    BOOST_CHECK(list_of_ids.empty());

    cache.removeStakeEntry(txid2);
    list_of_ids = cache.getActiveStakeIdsForScript(script);
    BOOST_CHECK(list_of_ids.empty());
}

BOOST_AUTO_TEST_CASE(generic_serialization) {
    double d1{13.4}, d2{0};
    fs::path ph = GetDataDir() / "serialization";
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    BOOST_CHECK(d1 != d2);
    {
        std::string key{"double_key"};
        CSerializer<double> serializer_1{d1, dbw, key};
        CSerializer<double> serializer_2{d2, dbw, key};
        serializer_1.dump();
        serializer_2.load();
    }
    BOOST_CHECK(d1 == d2);

    // test stl collection
    std::vector<double> v1{1, 2, -10, 3.4, 0}, v2{};
    BOOST_CHECK(v1.size() != v2.size());
    {
        std::string key{"stl_key"};
        CSerializer<std::vector<double>> serializer_1{v1, dbw, key};
        CSerializer<std::vector<double>> serializer_2{v2, dbw, key};
        serializer_1.dump();
        serializer_2.load();
    }

    BOOST_CHECK(v1.size() == v2.size());
    for(int i=0; i<v1.size(); i++)
        BOOST_CHECK(v1[i] == v2[i]);

    // test empty collection
    std::vector<double> empty{};
    BOOST_CHECK(empty.size() == 0);
    {
        std::string key{"empty_stl_key"};
        CSerializer<std::vector<double>> serializer{empty, dbw, key};
        serializer.load();
    }

    uint256 somehash(InsecureRand256()), otherhash;
    {
        std::string key{"uint256"};
        CSerializer<uint256> serializer1{somehash, dbw, key};
        CSerializer<uint256> serializer2{otherhash, dbw, key};
        serializer1.dump();
        serializer2.load();
    }
    BOOST_CHECK(somehash == otherhash);
}

BOOST_AUTO_TEST_CASE(dump_and_load) {
    std::set<uint256> list_of_ids;
    uint256 txid1 = InsecureRand256(), txid2 = InsecureRand256(), bestBlockHash = InsecureRand256();
    CScript script1(0x1234), script2(0x4321), script3(0x1111);
    CAmount amount1 = 10 * COIN, amount2 = 5 * COIN;
    CAmount reward1 = 1234567, reward2 = 0;
    size_t period1 = 0, period2 = 2;
    size_t completeBlock1 = 1, completeBlock2 = 70200;
    size_t numOutput1 = 54345, numOutput2 = 1;

    CMutableTransaction mtx;
    mtx.vin.resize(3);
    mtx.vin[0].prevout.hash = InsecureRand256();
    mtx.vin[0].prevout.n = 1;
    mtx.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    mtx.vin[1].prevout.hash = InsecureRand256();
    mtx.vin[1].prevout.n = 0;
    mtx.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    mtx.vin[2].prevout.hash = InsecureRand256();
    mtx.vin[2].prevout.n = 1;
    mtx.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = 90 * CENT;
    mtx.vout[0].scriptPubKey << OP_1;
    CTransaction free_tx_dummy(mtx);

    CStakesDbEntry entry1(txid1, amount1, reward1, period1, completeBlock1, numOutput1, script1, true);
    CStakesDbEntry entry2(txid2, amount2, reward2, period2, completeBlock2, numOutput2, script2, true);
    CAmount stakingPoolBalance = 1000 * COIN;
    {
        CStakesDB db(DEFAULT_CACHE_SIZE, false, false, DEFAULT_DB_NAME);
        CStakesDBCache cache(&db);
        cache.stakingPool().setBalance(stakingPoolBalance);
        cache.setBestBlock(bestBlockHash);
        cache.addNewStakeEntry(entry1);
        cache.addNewStakeEntry(entry2);
        cache.createFreeTxInfoForScript(script1, 23456);
        cache.flushDB();
    }
    // reload DB from disk
    CStakesDB db(DEFAULT_CACHE_SIZE, false, false, DEFAULT_DB_NAME);
    CStakesDBCache cache(&db);

    CStakesDbEntry loadEntry1 = cache.getStakeDbEntry(txid1);
    CStakesDbEntry loadEntry2 = cache.getStakeDbEntry(txid2);
    check_entry_equals(entry1, loadEntry1);
    check_entry_equals(entry2, loadEntry2);

    auto activeStakes = cache.getAllActiveStakes();
    BOOST_CHECK(activeStakes.size() == 2);

    auto stakesForScript1 = cache.getActiveStakeIdsForScript(script1);
    auto stakesForScript2 = cache.getActiveStakeIdsForScript(script2);
    BOOST_CHECK(stakesForScript1.size() == 1);
    BOOST_CHECK(stakesForScript2.size() == 1);
    BOOST_CHECK(stakesForScript1.count(txid1) == 1);
    BOOST_CHECK(stakesForScript2.count(txid2) == 1);

    auto amountsByPeriods = cache.getAmountsByPeriods();
    BOOST_CHECK(amountsByPeriods[period1] == amount1);
    BOOST_CHECK(amountsByPeriods[period2] == amount2);

    auto stakesCompletedAtHeight1 = cache.getStakesCompletedAtHeight(completeBlock1);
    auto stakesCompletedAtHeight2 = cache.getStakesCompletedAtHeight(completeBlock2);
    BOOST_CHECK(stakesCompletedAtHeight1.size() == 1);
    BOOST_CHECK(stakesCompletedAtHeight2.size() == 1);
    BOOST_CHECK(stakesCompletedAtHeight1[0].getKey() == txid1);
    BOOST_CHECK(stakesCompletedAtHeight2[0].getKey() == txid2);

    BOOST_CHECK(cache.stakingPool().getBalance() == stakingPoolBalance);

    BOOST_CHECK(cache.getBestBlock() == bestBlockHash);

    CFreeTxInfo freeTxInfo1 = cache.getFreeTxInfoForScript(script1);
    CFreeTxInfo freeTxInfo2 = cache.getFreeTxInfoForScript(script2);
    BOOST_CHECK(freeTxInfo1.isValid());
    BOOST_CHECK(!freeTxInfo2.isValid());
    BOOST_CHECK(freeTxInfo1.getLimit() == 1000000); // TODO(mtwaro): change this after limit calculation is ready
    BOOST_CHECK(freeTxInfo1.getUsedConfirmedLimit() == 0);
    BOOST_CHECK(freeTxInfo1.getActiveStakeIds().size() == 1);

    BOOST_CHECK(cache.registerFreeTransaction(script1, free_tx_dummy, 23456));
    BOOST_CHECK(cache.registerFreeTransaction(script2, free_tx_dummy, 23456));
    BOOST_CHECK(!cache.registerFreeTransaction(script3, free_tx_dummy, 23456));

    freeTxInfo1 = cache.getFreeTxInfoForScript(script1);
    freeTxInfo2 = cache.getFreeTxInfoForScript(script2);
    BOOST_CHECK(freeTxInfo1.getUsedConfirmedLimit() == free_tx_dummy.GetTotalSize());
    BOOST_CHECK(freeTxInfo2.isValid());
    BOOST_CHECK(freeTxInfo1.getUsedConfirmedLimit() == free_tx_dummy.GetTotalSize());


}

BOOST_AUTO_TEST_SUITE_END()
