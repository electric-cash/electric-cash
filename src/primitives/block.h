// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ELCASH_PRIMITIVES_BLOCK_H
#define ELCASH_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

struct CPureBlockHeader {
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
    	READWRITE(nVersion);
    	READWRITE(hashPrevBlock);
    	READWRITE(hashMerkleRoot);
    	READWRITE(nTime);
    	READWRITE(nBits);
    	READWRITE(nNonce);
    }

    uint256 GetHash() const;
};

class CAuxBlockHeader;

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader : public CPureBlockHeader
{
public:
	std::shared_ptr<CAuxBlockHeader> auxHeader;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CPureBlockHeader, *this);

        if (IsAuxPow()) {
        	if (auxHeader == nullptr)
        		auxHeader = std::make_shared<CAuxBlockHeader>();
        	assert(auxHeader != nullptr);
        	READWRITE(*auxHeader);
        } else {
        	auxHeader.reset();
        }
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        auxHeader.reset();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    /* Below are methods to interpret the version with respect to
     * auxpow data and chain ID. */

    /** Extract the base version (without modifiers and chain ID).
     * @return The base version.
     */
    int32_t GetBaseVersion() const {
    	return GetBaseVersion(nVersion);
    }
    static int32_t GetBaseVersion(int32_t ver) {
    	return ver % VERSION_AUXPOW;
    }

    /** Set the base version (apart from chain ID and auxpow flag) to
     * the on given. This should only be called when auxpow is not yet
     * set, to initialise a block!
     * @param nBaseVersion The base version.
     * @param nChainId The auxpow chain ID.
     */
    void SetBaseVersion(int32_t nBaseVersion, int32_t nChainId) {
    	assert(nBaseVersion >= 1 && nBaseVersion < VERSION_AUXPOW);
    	assert(!IsAuxPow());
    	nVersion = nBaseVersion | (nChainId * VERSION_CHAIN_START);
    }

    /** Extract the chain ID.
     * @return The chain ID encoded in the version.
     */
    int32_t GetChainId() const {
    	return nVersion / VERSION_CHAIN_START;
    }

    /** Set the chain ID. This is used for the test suite.
     * @param nChainId The chain ID to set.
     */
    void SetChainId(int32_t nChainId) {
    	nVersion %= VERSION_CHAIN_START;
    	nVersion |= nChainId * VERSION_CHAIN_START;
    }

    void SetAuxBlockHeader(std::unique_ptr<CAuxBlockHeader> auxHeader);

    /** Check if the auxpow flag is set in the version.
     * @return True if this block version is marked as auxpow.
     */
    bool IsAuxPow() const {
    	return nVersion & VERSION_AUXPOW;
    }

    /** Set the auxpow flag. This is used for testing.
     * @param isAuxHeader Whether to mark auxpow as true.
     */
    void SetBlockHeaderVersion(const bool isAuxHeader) {
    	if (isAuxHeader)
    		nVersion |= VERSION_AUXPOW;
    	else
    		nVersion &= ~VERSION_AUXPOW;
    }

private:
    /** Aux version modifier. */
    static const int32_t VERSION_AUXPOW = (1 << 8);

    /** Bits above are reserved for the auxpow chain ID. */
    static const int32_t VERSION_CHAIN_START = (1 << 16);
};

class CAuxBlockHeader
{
public:
	/** The parent block's coinbase transaction. */
	CTransactionRef coinbaseTx;

	/** The Merkle branch of the coinbase tx to the parent block's root. */
	std::vector<uint256> vMerkleBranch;

	/** The Merkle branch connecting the aux block to our coinbase. */
	std::vector<uint256> vChainMerkleBranch;

	/** Merkle tree index of the aux block header in the coinbase. */
	int nChainIndex;

	/** Parent block header (on which the real PoW is done). */
	CBlockHeader parentBlock;

	/* Prevent accidental conversion. */
	explicit CAuxBlockHeader(CTransactionRef&& txIn)
		: coinbaseTx(std::move(txIn)) {};
	CAuxBlockHeader() = default;

	ADD_SERIALIZE_METHODS;

	template <typename Stream, typename Operation>
	inline void SerializationOp(Stream& s, Operation ser_action) {
		/* The coinbase Merkle tx' hashBlock field is never actually verified
		 * or used in the code for an auxpow (and never was). The parent block
		 * is known anyway directly, so this is also redundant. By setting the
		 * value to zero (for serialising), we make sure that the format is
		 * backwards compatible but the data can be compressed. */
		uint256 hashBlock;

		/* The index of the parent coinbase tx is always zero. */
		int nIndex = 0;

		/* Data from the coinbase transaction as Merkle tx. */
		if (coinbaseTx == nullptr)
			coinbaseTx = std::make_shared<CTransaction>();
		READWRITE(coinbaseTx, hashBlock, vMerkleBranch, nIndex);

		/* Additional data for the auxpow itself. */
		READWRITE(vChainMerkleBranch, nChainIndex, parentBlock);
	}

	/**
	 * Returns the parent block hash. This is used to validate the PoW.
	 */
	uint256 getParentBlockHash() const {
		return parentBlock.GetHash();
	}

	std::string ToString() const;
};

class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CBlockHeader, *this);
        READWRITE(vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.auxHeader		 = auxHeader;
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // ELCASH_PRIMITIVES_BLOCK_H
