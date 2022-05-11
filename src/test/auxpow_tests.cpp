// Copyright (c) 2014-2020 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <arith_uint256.h>
#include <auxpow.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/merkle.h>
#include <validation.h>
#include <pow.h>
#include <primitives/block.h>
#include <rpc/auxpow_miner.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <uint256.h>
#include <univalue.h>


#include <algorithm>
#include <vector>
#include <string>

/* No space between BOOST_AUTO_TEST_SUITE and '(', so that extraction of
   the test-suite name works with grep as done in the Makefile.  */
BOOST_AUTO_TEST_SUITE(auxpow_tests)

/* ************************************************************************** */

/**
 * Tamper with a uint256 (modify it).
 * @param num The number to modify.
 */
static void tamperWith(uint256& num) {
  arith_uint256 modifiable = UintToArith256(num);
  modifiable += 1;
  num = ArithToUint256(modifiable);
}


/**
 * Utility class to construct auxpow's and manipulate them.  This is used
 * to simulate various scenarios.
 */
class CAuxpowBuilder {
public:

  /** The parent block (with coinbase, not just header).  */
  CBlock parentBlock;

  /** The auxpow's merkle branch (connecting it to the coinbase).  */
  std::vector<uint256> auxpowChainMerkleBranch;
  /** The auxpow's merkle tree index.  */
  int auxpowChainIndex;

  /**
   * Initialise everything.
   * @param baseVersion The parent block's base version to use.
   * @param chainId The parent block's chain ID to use.
   */
  CAuxpowBuilder(int baseVersion, int chainId);

  /**
   * Set the coinbase's script.
   * @param scr Set it to this script.
   */
  void setCoinbase(const CScript& scr);

  /**
   * Build the auxpow merkle branch.  The member variables will be
   * set accordingly.  This has to be done before constructing the coinbase
   * itself (which must contain the root merkle hash).  When we have the
   * coinbase afterwards, the member variables can be used to initialise
   * the CAuxPow object from it.
   * @param hashAux The merge-mined chain's block hash.
   * @param h Height of the merkle tree to build.
   * @param index Index to use in the merkle tree.
   * @return The root hash, with reversed endian.
   */
  valtype buildAuxpowChain(const uint256& hashAux, unsigned h, int index);

  /**
   * Build the finished CAuxPow object.  We assume that the auxpowChain
   * member variables are already set.  We use the passed in transaction
   * as the base.  It should (probably) be the parent block's coinbase.
   * @param tx The base tx to use.
   * @return The constructed CAuxPow object.
   */
  CAuxBlockHeader get(CTransactionRef tx) const;

  /**
   * Build the finished CAuxPow object from the parent block's coinbase.
   * @return The constructed CAuxPow object.
   */
  CAuxBlockHeader get() const {
    assert(!parentBlock.vtx.empty());
    return get(parentBlock.vtx[0]);
  }

  /**
   * Returns the finished CAuxPow object and returns it as std::unique_ptr.
   */
  std::unique_ptr<CAuxBlockHeader> getUnique() const {
	  CAuxBlockHeader* auxHeader = new CAuxBlockHeader();
	  *auxHeader = get();
	  return std::unique_ptr<CAuxBlockHeader>(auxHeader);
  }

  /**
   * Build a data vector to be included in the coinbase.  It consists
   * of the aux hash, the merkle tree size and the nonce.  Optionally,
   * the header can be added as well.
   * @param header Add the header?
   * @param hashAux The aux merkle root hash.
   * @param h Height of the merkle tree.
   * @param nonce The nonce value to use.
   * @return The constructed data.
   */
  static valtype buildCoinbaseData(bool header, const valtype& auxRoot, unsigned h, int nonce);
};

CAuxpowBuilder::CAuxpowBuilder(int baseVersion, int chainId)
		: auxpowChainIndex(-1) {
	parentBlock.SetBaseVersion(baseVersion, chainId);
}

void CAuxpowBuilder::setCoinbase(const CScript& scr) {
	CMutableTransaction mtx;
	mtx.vin.resize (1);
	mtx.vin[0].prevout.SetNull();
	mtx.vin[0].scriptSig = scr;

	parentBlock.vtx.clear();
	parentBlock.vtx.push_back(MakeTransactionRef(std::move(mtx)));
	parentBlock.hashMerkleRoot = BlockMerkleRoot(parentBlock);
}

valtype CAuxpowBuilder::buildAuxpowChain(const uint256& hashAux, unsigned h, int index) {
	auxpowChainIndex = index;

	/* Just use "something" for the branch.  Doesn't really matter.  */
	auxpowChainMerkleBranch.clear();
	for (unsigned i = 0; i < h; ++i)
		auxpowChainMerkleBranch.push_back(ArithToUint256(arith_uint256(i)));

	const uint256 hash = CAuxPow::checkMerkleBranch(const_cast<uint256&>(hashAux), auxpowChainMerkleBranch, index);

	valtype res = ToByteVector(hash);
	std::reverse(res.begin(), res.end());

	return res;
}

CAuxBlockHeader CAuxpowBuilder::get(CTransactionRef tx) const {
	LOCK(cs_main);

	CAuxBlockHeader res(std::forward<CTransactionRef>(tx));
	res.vMerkleBranch = merkle_tests::BlockMerkleBranch(parentBlock, 0);

	res.vChainMerkleBranch = auxpowChainMerkleBranch;
	res.nChainIndex = auxpowChainIndex;
	res.parentBlock = parentBlock;

	return res;
}

valtype CAuxpowBuilder::buildCoinbaseData(bool header, const valtype& auxRoot, unsigned h, int nonce)
{
	valtype res;

	if (header)
		res.insert (res.end(), pchMergedMiningHeader, pchMergedMiningHeader + sizeof(pchMergedMiningHeader));
	res.insert(res.end(), auxRoot.begin(), auxRoot.end());

	int size = (1 << h);
	for (int i = 0; i < 4; ++i) {
		res.insert(res.end(), size & 0xFF);
		size >>= 8;
	}
	for (int i = 0; i < 4; ++i) {
		res.insert(res.end(), nonce & 0xFF);
		nonce >>= 8;
	}

	return res;
}

/* ************************************************************************** */
BOOST_FIXTURE_TEST_CASE(check_auxpow, BasicTestingSetup) {
	const Consensus::Params& params = Params().GetConsensus();
	CAuxpowBuilder builder(5, 42);
	CAuxBlockHeader auxHeader;

	uint256 hashAux = ArithToUint256(arith_uint256(12345));
	const int32_t ourChainId = params.nAuxpowChainId;
	const unsigned height = 30;
	const int nonce = 7;
	int index;

	valtype auxRoot, data;
	CScript scr;

	/* Build a correct auxpow. The height is the maximally allowed one. */
	index = CAuxPow::getExpectedIndex(nonce, ourChainId, height);
	auxRoot = builder.buildAuxpowChain(hashAux, height, index);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	scr = (CScript () << 2809 << 2013);
	scr = (scr << OP_2 << data);
	builder.setCoinbase(scr);
	BOOST_CHECK(CAuxPow::check(builder.get(), hashAux, ourChainId, params));

	/* An auxpow without any inputs in the parent coinbase tx should be
	 handled gracefully (and be considered invalid).  */
	CMutableTransaction mtx(*builder.parentBlock.vtx[0]);
	mtx.vin.clear();
	builder.parentBlock.vtx.clear();
	builder.parentBlock.vtx.push_back(MakeTransactionRef(std::move (mtx)));
	builder.parentBlock.hashMerkleRoot = BlockMerkleRoot(builder.parentBlock);
	BOOST_CHECK(!CAuxPow::check(builder.get(), hashAux, ourChainId, params));

	/* Check that the auxpow is invalid if we change either the aux block's
	 hash or the chain ID.  */
	uint256 modifiedAux(hashAux);
	tamperWith(modifiedAux);
	BOOST_CHECK(!CAuxPow::check(builder.get(), modifiedAux, ourChainId, params));
	BOOST_CHECK(!CAuxPow::check(builder.get(), hashAux, ourChainId + 1, params));

	/* Non-coinbase parent tx should fail.  Note that we can't just copy
	 the coinbase literally, as we have to get a tx with different hash.  */
	const CTransactionRef oldCoinbase = builder.parentBlock.vtx[0];
	builder.setCoinbase(scr << 5);
	builder.parentBlock.vtx.push_back(oldCoinbase);
	builder.parentBlock.hashMerkleRoot = BlockMerkleRoot(builder.parentBlock);
	auxHeader = builder.get(builder.parentBlock.vtx[0]);
	BOOST_CHECK(CAuxPow::check(auxHeader, hashAux, ourChainId, params));
	auxHeader = builder.get(builder.parentBlock.vtx[1]);
	BOOST_CHECK(!CAuxPow::check(auxHeader, hashAux, ourChainId, params));

	/* The parent chain can't have the same chain ID.  */
	CAuxpowBuilder builder2(builder);
	builder2.parentBlock.SetChainId(100);
	BOOST_CHECK(CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
	builder2.parentBlock.SetChainId(ourChainId);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	/* Disallow too long merkle branches.  */
	builder2 = builder;
	index = CAuxPow::getExpectedIndex(nonce, ourChainId, height + 1);
	auxRoot = builder2.buildAuxpowChain(hashAux, height + 1, index);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height + 1, nonce);
	scr = (CScript () << 2809 << 2013);
	scr = (scr << OP_2 << data);
	builder2.setCoinbase(scr);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	/* Verify that we compare correctly to the parent block's merkle root.  */
	builder2 = builder;
	BOOST_CHECK(CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
	tamperWith(builder2.parentBlock.hashMerkleRoot);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	/* Build a non-header legacy version and check that it is also accepted.  */
	builder2 = builder;
	index = CAuxPow::getExpectedIndex(nonce, ourChainId, height);
	auxRoot = builder2.buildAuxpowChain(hashAux, height, index);
	data = CAuxpowBuilder::buildCoinbaseData(false, auxRoot, height, nonce);
	scr = (CScript () << 2809 << 2013);
	scr = (scr << OP_2 << data);
	builder2.setCoinbase(scr);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	/* However, various attempts at smuggling two roots in should be detected.  */
	const valtype wrongAuxRoot = builder2.buildAuxpowChain(modifiedAux, height, index);
	valtype data2 = CAuxpowBuilder::buildCoinbaseData(false, wrongAuxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data << data2);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
	builder2.setCoinbase(CScript() << data2 << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	data2 = CAuxpowBuilder::buildCoinbaseData(true, wrongAuxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data << data2);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
	builder2.setCoinbase(CScript() << data2 << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data << data2);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
	builder2.setCoinbase(CScript() << data2 << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	data2 = CAuxpowBuilder::buildCoinbaseData(false, wrongAuxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data << data2);
	BOOST_CHECK(CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
	builder2.setCoinbase(CScript() << data2 << data);
	BOOST_CHECK(CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	/* Verify that the appended nonce/size values are checked correctly.  */
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data);
	BOOST_CHECK(CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	data.pop_back();
	builder2.setCoinbase(CScript() << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height - 1, nonce);
	builder2.setCoinbase(CScript() << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce + 3);
	builder2.setCoinbase(CScript() << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	/* Put the aux hash in an invalid merkle tree position.  */
	auxRoot = builder.buildAuxpowChain(hashAux, height, index + 1);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data);
	BOOST_CHECK(!CAuxPow::check(builder2.get(), hashAux, ourChainId, params));

	auxRoot = builder.buildAuxpowChain(hashAux, height, index);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder2.setCoinbase(CScript() << data);
	BOOST_CHECK(CAuxPow::check(builder2.get(), hashAux, ourChainId, params));
}

/* ************************************************************************** */

/**
 * Mine a block (assuming minimal difficulty) that either matches
 * or doesn't match the difficulty target specified in the block header.
 * @param block The block to mine (by updating nonce).
 * @param ok Whether the block should be ok for PoW.
 * @param nBits Use this as difficulty if specified.
 */
static void mineBlock(CBlockHeader& block, bool ok, int nBits = -1) {
	if (nBits == -1)
		nBits = block.nBits;

	arith_uint256 target;
	target.SetCompact(nBits);

	block.nNonce = 0;
	while(true) {
		const bool nowOk = (UintToArith256(block.GetHash()) <= target);
		if ((ok && nowOk) || (!ok && !nowOk))
			break;

		++block.nNonce;
	}

	if (ok)
		BOOST_CHECK(CheckProofOfWork(block.GetHash(), nBits, Params().GetConsensus()));
	else
		BOOST_CHECK(!CheckProofOfWork(block.GetHash(), nBits, Params().GetConsensus()));
}

BOOST_FIXTURE_TEST_CASE(auxpow_pow, BasicTestingSetup) {
	/* Use regtest parameters to allow mining with easy difficulty.  */
	SelectParams(CBaseChainParams::REGTEST);
	const Consensus::Params& params = Params().GetConsensus();

	const arith_uint256 target = (~arith_uint256(0) >> 1);
	CBlockHeader block;
	block.nBits = target.GetCompact();
	const CBlockIndex blockIndex{block};

	/* Pre auxpow version blocks should pass just fine. */
	block.nVersion = ComputeBlockVersion(&blockIndex, params);
	mineBlock(block, true);
	BOOST_CHECK(CheckProofOfWork(block, params));

	/* No auxpow block version, so it is validated as pre auxpow */
	block.SetBaseVersion(2, params.nAuxpowChainId);
	mineBlock(block, true);
	BOOST_CHECK(CheckProofOfWork(block, params));

	/* Block does not have auxpow so wrong chain id doesnt matter */
	block.SetChainId(params.nAuxpowChainId + 1);
	mineBlock(block, true);
	BOOST_CHECK(CheckProofOfWork(block, params));

	/* Check the case when the block does have auxpow (this is true
	 right now).  */
	block.SetChainId(params.nAuxpowChainId);
	block.SetBlockHeaderVersion(true);
	mineBlock(block, true);
	BOOST_CHECK(!CheckProofOfWork(block, params));

	block.SetBlockHeaderVersion(false);
	mineBlock(block, true);
	BOOST_CHECK(CheckProofOfWork(block, params));
	mineBlock(block, false);
	BOOST_CHECK(!CheckProofOfWork(block, params));

	/* ****************************************** */
	/* Check the case that the block has auxpow.  */
	CAuxpowBuilder builder(5, 42);
	const int32_t ourChainId = params.nAuxpowChainId;
	const unsigned height = 3;
	const int nonce = 7;
	const int index = CAuxPow::getExpectedIndex(nonce, ourChainId, height);
	valtype auxRoot, data;

	/* Valid auxpow, PoW check of parent block.  */
	block.SetBlockHeaderVersion(true);
	auxRoot = builder.buildAuxpowChain(block.GetHash(), height, index);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder.setCoinbase(CScript() << data);
	mineBlock(builder.parentBlock, false, block.nBits);
	block.SetAuxBlockHeader(builder.getUnique());
	BOOST_CHECK(!CheckProofOfWork(block, params));
	mineBlock(builder.parentBlock, true, block.nBits);
	block.SetAuxBlockHeader(builder.getUnique());
	BOOST_CHECK(CheckProofOfWork(block, params));

	/* Mismatch between auxpow being present and block.nVersion. Note that
	 block.SetAuxpow sets also the version and that we want to ensure
	 that the block hash itself doesn't change due to version changes.
	 This requires some work arounds.  */
	block.SetBlockHeaderVersion(false);
	uint256 hashAux = block.GetHash();
	auxRoot = builder.buildAuxpowChain(hashAux, height, index);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder.setCoinbase(CScript () << data);
	mineBlock(builder.parentBlock, true, block.nBits);
	block.SetAuxBlockHeader(builder.getUnique());
	BOOST_CHECK(hashAux != block.GetHash());
	block.SetBlockHeaderVersion(false);
	BOOST_CHECK(hashAux == block.GetHash());
	BOOST_CHECK(!CheckProofOfWork(block, params));

	/* Modifying the block invalidates the PoW.  */
	block.SetBlockHeaderVersion(true);
	auxRoot = builder.buildAuxpowChain(block.GetHash(), height, index);
	data = CAuxpowBuilder::buildCoinbaseData(true, auxRoot, height, nonce);
	builder.setCoinbase(CScript() << data);
	mineBlock(builder.parentBlock, true, block.nBits);
	block.SetAuxBlockHeader(builder.getUnique());
	BOOST_CHECK(CheckProofOfWork(block, params));
	tamperWith(block.hashMerkleRoot);
	BOOST_CHECK(!CheckProofOfWork(block, params));
}

/* ************************************************************************** */

/**
 * Helper class that is friend to AuxpowMiner and makes the tested methods
 * accessible to the test code.
 */
class AuxpowMinerForTest : public AuxpowMiner {
public:
	using AuxpowMiner::cs;

	using AuxpowMiner::getCurrentBlock;
	using AuxpowMiner::lookupSavedBlock;

};

// BOOST_FIXTURE_TEST_CASE(auxpow_miner_blockRegeneration, TestChain100Setup) {
// 	CTxMemPool mempool;
// 	AuxpowMinerForTest miner;
// 	LOCK(miner.cs);

// 	/* We use mocktime so that we can control GetTime() as it is used in the
// 	 logic that determines whether or not to reconstruct a block.  The "base"
// 	 time is set such that the blocks we have from the fixture are fresh.  */
// 	const int64_t baseTime = ::ChainActive().Tip()->GetMedianTimePast() + 1;
// 	SetMockTime(baseTime);

// 	/* Construct a first block.  */
// 	CScript scriptPubKey;
// 	uint256 target;
// 	const CBlock* pblock1 = miner.getCurrentBlock(mempool, scriptPubKey, target);
// 	BOOST_CHECK(pblock1 != nullptr);
// 	const uint256 hash1 = pblock1->GetHash();

// 	/* Verify target computation.  */
// 	arith_uint256 expected;
// 	expected.SetCompact(pblock1->nBits);
// 	BOOST_CHECK(target == ArithToUint256(expected));

// 	/* Calling the method again should return the same, cached block a second
// 	 time (even if we advance the clock, since there are no new
// 	 transactions).  */
// 	SetMockTime(baseTime + 100);
// 	const CBlock* pblock = miner.getCurrentBlock(mempool, scriptPubKey, target);
// 	BOOST_CHECK(pblock == pblock1 && pblock->GetHash() == hash1);

// 	/* Mine a block, then we should get a new auxpow block constructed.  Note that
// 	 it can be the same *pointer* if the memory was reused after clearing it,
// 	 so we can only verify that the hash is different.  */
// 	CreateAndProcessBlock({}, scriptPubKey);
// 	const CBlock* pblock2 = miner.getCurrentBlock(mempool, scriptPubKey, target);
// 	BOOST_CHECK(pblock2 != nullptr);
// 	const uint256 hash2 = pblock2->GetHash();
// 	BOOST_CHECK(hash2 != hash1);

// 	/* Add a new transaction to the mempool.  */
// 	TestMemPoolEntryHelper entry;
// 	CMutableTransaction mtx;
// 	mtx.vout.emplace_back(1234, scriptPubKey);
// 	{
// 		LOCK (mempool.cs);
// 		mempool.addUnchecked(entry.FromTx(mtx));
// 	}

// 	/* We should still get back the cached block, for now.  */
// 	SetMockTime(baseTime + 160);
// 	pblock = miner.getCurrentBlock(mempool, scriptPubKey, target);
// 	BOOST_CHECK(pblock == pblock2 && pblock->GetHash() == hash2);

// 	/* With time advanced too far, we get a new block.  This time, we should also
// 	 definitely get a different pointer, as there is no clearing.  The old
// 	 blocks are freed only after a new tip is found.  */
// 	SetMockTime(baseTime + 161);
// 	const CBlock* pblock3 = miner.getCurrentBlock(mempool, scriptPubKey, target);
// 	BOOST_CHECK(pblock3 != pblock2 && pblock3->GetHash() != hash2);
// }

BOOST_FIXTURE_TEST_CASE(auxpow_miner_createAndLookupBlock, TestChain100Setup) {
	CTxMemPool mempool;
	AuxpowMinerForTest miner;
	LOCK(miner.cs);

	CScript scriptPubKey;
	uint256 target;
	const CBlock* pblock = miner.getCurrentBlock(mempool, scriptPubKey, target);
	BOOST_CHECK(pblock != nullptr);

	BOOST_CHECK(miner.lookupSavedBlock(pblock->GetHash().GetHex()) == pblock);
	BOOST_CHECK_THROW(miner.lookupSavedBlock("foobar"), UniValue);
}

/* ************************************************************************** */

BOOST_AUTO_TEST_SUITE_END()
