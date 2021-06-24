#include <dbwrapper.h>
#include <staking/staker_db.h>
#include <test/util/setup_common.h>
#include <boost/test/unit_test.hpp>


void script_fixture(CScript& script) {
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();
    script.clear();
    script << ToByteVector(pubkey) << OP_CHECKSIG;
}

BOOST_FIXTURE_TEST_SUITE(stakerdb_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(staker_db_entry_serialization) {
    CScript script;
    script_fixture(script);
    CStakerDbEntry input(InsecureRand256(), 10, 15, 20, 25, 30, script);
    CStakerDbEntry output;

    fs::path ph = GetDataDir() / "test_staker_db";
    CDBWrapper dbw(ph, (1 << 20), true, false, false);

    BOOST_CHECK(dbw.Write(input.getKey(), input));
    BOOST_CHECK(dbw.Read(input.getKey(), output));
    BOOST_CHECK(input.getAmount() == output.getAmount());
    BOOST_CHECK(input.getReward() == output.getReward());
    BOOST_CHECK(input.isComplete() == output.isComplete());
    BOOST_CHECK(input.getCompleteBlock() == output.getCompleteBlock());
    BOOST_CHECK(input.getNumOutput() == output.getNumOutput());
    BOOST_CHECK(input.getScript() == output.getScript());
}

BOOST_AUTO_TEST_SUITE_END()
