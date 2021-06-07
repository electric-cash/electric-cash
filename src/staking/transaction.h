#ifndef ELCASH_STAKING_TRANSACTION_H
#define ELCASH_STAKING_TRANSACTION_H

#include <primitives/transaction.h>
#include <script/script.h>
#include <assert.h>
#include "stakingparams.h"

enum class StakingTransactionType {
    NONE,
    DEPOSIT,
    WITHDRAWAL,
    BURN
};

constexpr uint8_t STAKING_TX_HEADER = 0x53;
constexpr uint8_t STAKING_TX_DEPOSIT_SUBHEADER = 0x44;
constexpr uint8_t STAKING_TX_BURN_SUBHEADER = 0x42;

constexpr size_t STAKING_HEADER_SIZE = 1 + 2; // OP_RETURN + HEADER + SUBHEADER
constexpr size_t STAKING_DEPOSIT_TX_HEADER_MIN_SIZE = STAKING_HEADER_SIZE + 1 + 1; // HEADER + VARINT + UINT8
constexpr size_t STAKING_BURN_TX_HEADER_SIZE = STAKING_HEADER_SIZE + 8; // HEADER + UINT64


class CStakingTransactionHandler {
public:
    static StakingTransactionType GetStakingTxType(const CTransaction& tx);
    static bool IsStakingTxHeader(const CScript& script);
    static bool IsStakingDepositTxHeader(const CScript& script);
    static bool IsStakingBurnTxHeader(const CScript& script);
    static bool IsStakingDepositTxSubHeader(const CScript& script);
    static bool IsStakingBurnTxSubHeader(const CScript& script);
private:
    static StakingTransactionType ValidateStakingDepositTx(const CTransaction& tx);
    static StakingTransactionType ValidateStakingBurnTx(const CTransaction& tx);





};


#endif //ELCASH_STAKING_TRANSACTION_H
