// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>

uint256 CPureBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

void CBlockHeader::SetAuxBlockHeader(std::unique_ptr<CAuxBlockHeader> auxBlockHeader)
{
	if (auxBlockHeader) {
		auxHeader.reset(auxBlockHeader.release());
		SetBlockHeaderVersion(true);
	} else {
		auxHeader.reset();
		SetBlockHeaderVersion(false);
	}
}

std::string CAuxBlockHeader::ToString() const {
	std::stringstream s;
	s << strprintf("CAuxBlockHeader(coinbase=%s, vMerkleBranchSize=%u, vChainMerkleBranchSize=%u, nChainId=%u, parentBlock=%s)\n",
		coinbaseTx->ToString(),
		vMerkleBranch.size(),
		vChainMerkleBranch.size(),
		nChainIndex,
		CBlock(parentBlock).ToString());
	return s.str();
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
