// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ELCASH_POW_H
#define ELCASH_POW_H

#include <consensus/params.h>

#include <stdint.h>
#include <arith_uint256.h>

class CBlockHeader;
class CBlockIndex;
class uint256;
class CStakesDBCache;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&, CStakesDBCache& stakes, const uint32_t freeTxSizeBytes = 0);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);
arith_uint256 LwmaCalculateNextBaseWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params, CStakesDBCache& stakes);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);

#endif // ELCASH_POW_H
