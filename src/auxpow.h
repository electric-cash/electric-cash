// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ELCASH_AUXPOW_H
#define ELCASH_AUXPOW_H

#include <consensus/params.h>
#include <serialize.h>
#include <uint256.h>

#include <memory>

class CAuxBlockHeader;
class CBlockHeader;

/** Header for merge-mining data in the coinbase. */
static const unsigned char pchMergedMiningHeader[] = { 0xfa, 0xbe, 'm', 'm'};

class CAuxPow {
public:
	/** Check if all merkle branches for parent block are valid and has our chain ID. */
	static bool check(const CAuxBlockHeader& auxHeader, const uint256& hashAuxBlock,
			const int nChainId, const Consensus::Params& params);

	/** Calculate expected index in merkle tree. */
	static int getExpectedIndex(uint32_t nNonce, int nChainId, unsigned h);

	/** Create minimal CAuxBlockHeader object. */
	static std::unique_ptr<CAuxBlockHeader> createAuxBlockHeader(CBlockHeader& header);

	/** Initialises CAuxBlockHeader of the given CBlockHeader. */
	static void initBlockHeader(CBlockHeader& header);

	static uint256 checkMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex);
};


#endif /* ELCASH_AUXPOW_H */
