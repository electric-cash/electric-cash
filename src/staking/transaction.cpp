#include "transaction.h"



StakingTransactionType CStakingTransactionHandler::GetStakingTxType(const CTransaction &tx) {
    const CScript& txHeaderScript = tx.vout[0].scriptPubKey;
    if(txHeaderScript[0] != OP_RETURN || txHeaderScript[1] != STAKING_TX_HEADER) {
        return StakingTransactionType::NONE;
    }

    switch (txHeaderScript[2]) {
        case STAKING_TX_DEPOSIT_SUBHEADER:
            return CStakingTransactionHandler::ValidateStakingDepositTx(tx);
        case STAKING_TX_WITHDRAWAL_SUBHEADER:
            return CStakingTransactionHandler::ValidateStakingWithdrawalTx(tx);
        case STAKING_TX_BURN_SUBHEADER:
            return CStakingTransactionHandler::ValidateStakingBurnTx(tx);
        default:
            return StakingTransactionType::NONE;
    }
}

StakingTransactionType CStakingTransactionHandler::ValidateStakingDepositTx(const CTransaction &tx) {
    return StakingTransactionType::DEPOSIT;
}

StakingTransactionType CStakingTransactionHandler::ValidateStakingWithdrawalTx(const CTransaction &tx) {
    return StakingTransactionType::WITHDRAWAL;
}

StakingTransactionType CStakingTransactionHandler::ValidateStakingBurnTx(const CTransaction &tx) {
    return StakingTransactionType::BURN;
}
