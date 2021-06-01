#ifndef ELCASH_STAKING_TRANSACTION_H
#define ELCASH_STAKING_TRANSACTION_H

#include <primitives/transaction.h>

enum class StakingTransactionType {
    NONE,
    DEPOSIT,
    WITHDRAWAL,
    BURN
};

const uint8_t STAKING_TX_HEADER = 0x53;
const uint8_t STAKING_TX_DEPOSIT_SUBHEADER = 0x44;
const uint8_t STAKING_TX_WITHDRAWAL_SUBHEADER = 0x57;
const uint8_t STAKING_TX_BURN_SUBHEADER = 0x42;


class CStakingTransactionHandler {
    static StakingTransactionType GetStakingTxType(const CTransaction& tx);
    static StakingTransactionType ValidateStakingDepositTx(const CTransaction& tx);
    static StakingTransactionType ValidateStakingWithdrawalTx(const CTransaction& tx);
    static StakingTransactionType ValidateStakingBurnTx(const CTransaction& tx);




};


#endif //ELCASH_STAKING_TRANSACTION_H
