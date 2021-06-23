#ifndef ELCASH_STAKING_TRANSACTION_H
#define ELCASH_STAKING_TRANSACTION_H

#include <primitives/transaction.h>
#include <script/script.h>
#include <assert.h>
#include "stakingparams.h"

enum class StakingTransactionType {
    NONE,
    DEPOSIT,
    BURN
};

constexpr uint8_t STAKING_TX_HEADER = 0x53;
constexpr uint8_t STAKING_TX_DEPOSIT_SUBHEADER = 0x44;
constexpr uint8_t STAKING_TX_BURN_SUBHEADER = 0x42;

constexpr size_t STAKING_HEADER_SIZE = 1 + 1 + 2; // OP_RETURN + push_size HEADER + SUBHEADER
constexpr size_t STAKING_BURN_TX_HEADER_SIZE = STAKING_HEADER_SIZE + 8; // HEADER + UINT64


struct CStakingDepositTxMetadata {
    CStakingDepositTxMetadata() = default;
    CStakingDepositTxMetadata(uint64_t nOutputIndexIn, uint8_t nPeriodIn) : nOutputIndex(nOutputIndexIn), nPeriod(nPeriodIn) {}
    uint64_t nOutputIndex;
    uint8_t nPeriod;
};

struct CStakingBurnTxMetadata {
    CStakingBurnTxMetadata() = default;
    explicit CStakingBurnTxMetadata(CAmount nAmountIn) : nAmount(nAmountIn) {}
    CAmount nAmount;
};

union UStakingTxMetadata {
    CStakingDepositTxMetadata depositTxMetadata;
    CStakingBurnTxMetadata burnTxMetadata;
    ~UStakingTxMetadata() {}
};

class CStakingTransactionParser {
public:
    explicit CStakingTransactionParser(CTransactionRef txIn);
    StakingTransactionType GetStakingTxType() const;
    CStakingDepositTxMetadata GetStakingDepositTxMetadata() const;
    CStakingBurnTxMetadata GetStakingBurnTxMetadata() const;
private:
    CTransactionRef tx;
    UStakingTxMetadata stakingTxMetadata;
    StakingTransactionType txType;
    StakingTransactionType ProcessTransaction();
    StakingTransactionType ValidateStakingDepositTx();
    StakingTransactionType ValidateStakingBurnTx();
    static bool IsStakingTxHeader(const CScript& script);
    static bool IsStakingDepositTxHeader(const CScript& script);
    static bool IsStakingBurnTxHeader(const CScript& script);
    static bool IsStakingDepositTxSubHeader(const CScript& script);
    static bool IsStakingBurnTxSubHeader(const CScript& script);


};


#endif //ELCASH_STAKING_TRANSACTION_H
