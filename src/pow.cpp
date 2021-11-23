// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <staking/stakes_db.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, CStakesDBCache& stakes, uint32_t freeTxSizeBytes)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    if (params.fPowAllowMinDifficultyBlocks)
    {
    // Special difficulty rule for testnet:
    // If the new block's timestamp is more than 2* 10 minutes
    // then allow mining of a min-difficulty block.
        if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
            return nProofOfWorkLimit;
    }

    if (params.fPowNoRetargeting)
        return pindexLast->nBits;
    arith_uint256 target = LwmaCalculateNextBaseWorkRequired(pindexLast, params, stakes);
    target *= (params.freeTxMaxSizeInBlock * params.freeTxDifficultyCoefficient + freeTxSizeBytes);
    target /= (params.freeTxMaxSizeInBlock * params.freeTxDifficultyCoefficient);
    return target.GetCompact();
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}


// LWMA-1 for BTC/Zcash clones
// Algorithm by Zawy, a modification of WT-144 by Tom Harding
// https://github.com/zawy12/difficulty-algorithms/issues/3#issuecomment-442129791

arith_uint256 LwmaCalculateNextBaseWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params, CStakesDBCache& stakes)
{
    const int64_t T = params.nPowTargetSpacing;

   // For T=600, 300, 150 use approximately N=60, 90, 120
    const int64_t N = params.nLwmaAveragingWindow;  

    // Define a k that will be used to get a proper average after weighting the solvetimes.
    const int64_t k = N * (N + 1) * T / 2; 

    const int64_t height = pindexLast->nHeight;
    const arith_uint256 powLimit = UintToArith256(params.powLimit);

   // New coins should just give away first N blocks before using this algorithm.
    if (height < N) { return powLimit.GetCompact(); }

    arith_uint256 avgTarget, nextTarget;
    int64_t thisTimestamp, previousTimestamp;
    int64_t sumWeightedSolvetimes = 0, j = 0;

    const CBlockIndex* blockPreviousTimestamp = pindexLast->GetAncestor(height - N);
    previousTimestamp = blockPreviousTimestamp->GetBlockTime();

    // Loop through N most recent blocks. 
    for (int64_t i = height - N + 1; i <= height; i++) {
        const CBlockIndex* block = pindexLast->GetAncestor(i);

        // Prevent solvetimes from being negative in a safe way. It must be done like this. 
        // In particular, do not attempt anything like  if(solvetime < 0) {solvetime=0;}
        // The +1 ensures new coins do not calculate nextTarget = 0.
        thisTimestamp = (block->GetBlockTime() > previousTimestamp) ? 
                            block->GetBlockTime() : previousTimestamp + 1;

         int64_t solvetime = thisTimestamp - previousTimestamp; 

       // The following is part of "preventing negative solvetimes". 
        previousTimestamp = thisTimestamp;

       // Give linearly higher weight to more recent solvetimes.
        j++;
        sumWeightedSolvetimes += solvetime * j; 

        arith_uint256 target;
        target.SetCompact(block->nBits);
        // find base difficulty having the final difficulty and free transactions size
        target *= (params.freeTxMaxSizeInBlock * params.freeTxDifficultyCoefficient);
        target /= (params.freeTxMaxSizeInBlock * params.freeTxDifficultyCoefficient + stakes.getFreeTxSizeForBlock(block->GetBlockHash()));
        avgTarget += target / N / k; // Dividing by k here prevents an overflow below.
    }

   // Desired equation in next line was nextTarget = avgTarget * sumWeightSolvetimes / k
   // but 1/k was moved to line above to prevent overflow in new coins

    nextTarget = avgTarget * sumWeightedSolvetimes;

    if (nextTarget > powLimit) { nextTarget = powLimit; }

    return nextTarget;
} 
