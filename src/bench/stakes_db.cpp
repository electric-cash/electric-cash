#include <bench/bench.h>
#include <staking/stakes_db.h>
#include <arith_uint256.h>
#include <pubkey.h>
#include <key.h>
#include <cmath>


static CStakesDBCache* fill_cache(size_t cache_size_in_bytes) {
    CStakesDB* base_db = new CStakesDB(cache_size_in_bytes / 10, false, true,
        (GetDataDir() / "bench_test" / "stakes_db").string());
    CStakesDBCache* db = new CStakesDBCache(base_db, cache_size_in_bytes);

    CScript script;
    script.clear();
    CKey key;
    key.MakeNewKey(true);
    CPubKey pub{key.GetPubKey()};
    script << ToByteVector(pub);
    script << OP_CHECKSIG;

    CStakesDbEntry entry{uint256{}, 10, 15, 20, 25, 30, script, true};
    size_t entry_size{entry.estimateSize()};

    for(arith_uint256 key{0}; key < static_cast<int>(floor(cache_size_in_bytes / entry_size)); ++key) {
        entry.setKey(ArithToUint256(key));
        db->addStakeEntry(entry);
    }
    return db;
}

static void StakesDB(benchmark::State& state) {
    CStakesDBCache *db = fill_cache(500 * (1 << 20));

    while(state.KeepRunning()) {
        db->flushDB();
    }

    delete db;
}

BENCHMARK(StakesDB, 1);
