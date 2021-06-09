#include "transaction.h"
#include <streams.h>
#include <iostream>


CStakingTransactionHandler::CStakingTransactionHandler(CTransactionRef txIn) {
    tx = txIn;
    txType = ProcessTransaction();
}


StakingTransactionType CStakingTransactionHandler::ProcessTransaction() {
    if (tx->vout.empty()) {
        return StakingTransactionType::NONE;
    }
    const CScript& txHeaderScript = tx->vout[0].scriptPubKey;
    if (!IsStakingTxHeader(txHeaderScript)) {
        return StakingTransactionType::NONE;
    }
    if (IsStakingDepositTxSubHeader(txHeaderScript)) {
        return CStakingTransactionHandler::ValidateStakingDepositTx();
    } else if (IsStakingBurnTxSubHeader(txHeaderScript)) {
        return CStakingTransactionHandler::ValidateStakingBurnTx();
    } else {
        return StakingTransactionType::NONE;
    }
}

StakingTransactionType CStakingTransactionHandler::ValidateStakingDepositTx() {
    CDataStream ds(0,0);
    tx->vout[0].scriptPubKey.Serialize(ds); // TODO: doing memcpy instead of serializing would be more efficient.
    ser_readdata8(ds); // read the size
    const uint8_t op = ser_readdata8(ds);
    const uint8_t header = ser_readdata8(ds);
    const uint8_t subheader = ser_readdata8(ds);
    assert(op == OP_RETURN || header == STAKING_TX_HEADER || subheader == STAKING_TX_DEPOSIT_SUBHEADER);
    if (ds.size() < 2) {
        return StakingTransactionType::NONE;
    }

    // Read and validate staking vout index
    uint64_t outputIndex;
    try {
        outputIndex = ReadCompactSize(ds);
    } catch(...) {
        return StakingTransactionType::NONE;
    }
    if (outputIndex == 0 || outputIndex >= tx->vout.size() || tx->vout[outputIndex].nValue < MIN_STAKING_AMOUNT) {
        return StakingTransactionType::NONE;
    }

    // Read and validate staking period index
    uint8_t stakingPeriod;
    try {
        stakingPeriod = ser_readdata8(ds);
    } catch (...) {
        return StakingTransactionType::NONE;
    }
    if (stakingPeriod >= STAKING_PERIOD.size()) {
        return StakingTransactionType::NONE;
    }
    stakingTxMetadata.depositTxMetadata = CStakingDepositTxMetadata(outputIndex, stakingPeriod);
    return StakingTransactionType::DEPOSIT;
}

StakingTransactionType CStakingTransactionHandler::ValidateStakingBurnTx() {
    CDataStream ds(0,0);
    tx->vout[0].scriptPubKey.Serialize(ds); // TODO: doing memcpy instead of serializing would be more efficient.
    ser_readdata8(ds); // read the size
    const uint8_t op = ser_readdata8(ds);
    const uint8_t header = ser_readdata8(ds);
    const uint8_t subheader = ser_readdata8(ds);
    assert(op == OP_RETURN || header == STAKING_TX_HEADER || subheader == STAKING_TX_BURN_SUBHEADER);
    if (ds.size() < STAKING_BURN_TX_HEADER_SIZE - STAKING_HEADER_SIZE) {
        return StakingTransactionType::NONE;
    }
    uint64_t amount;
    try {
        amount = ser_readdata64(ds);
    } catch(...) {
        return StakingTransactionType::NONE;
    }

    stakingTxMetadata.burnTxMetadata = CStakingBurnTxMetadata(amount);
    return StakingTransactionType::BURN;
}

bool CStakingTransactionHandler::IsStakingTxHeader(const CScript &script) {
    return (script.size() >= STAKING_HEADER_SIZE && script[0] == OP_RETURN && script[1] == STAKING_TX_HEADER);
}

bool CStakingTransactionHandler::IsStakingDepositTxHeader(const CScript &script) {
    return (IsStakingTxHeader(script) && IsStakingDepositTxSubHeader(script));
}

bool CStakingTransactionHandler::IsStakingBurnTxHeader(const CScript &script) {
    return (IsStakingTxHeader(script) && IsStakingBurnTxSubHeader(script));
}

bool CStakingTransactionHandler::IsStakingDepositTxSubHeader(const CScript &script) {
    return (script.size() >= STAKING_HEADER_SIZE && script[2] == STAKING_TX_DEPOSIT_SUBHEADER);
}

bool CStakingTransactionHandler::IsStakingBurnTxSubHeader(const CScript &script) {
    return (script.size() >= STAKING_HEADER_SIZE && script[2] == STAKING_TX_BURN_SUBHEADER);
}

StakingTransactionType CStakingTransactionHandler::GetStakingTxType() const {
    return txType;
}

CStakingDepositTxMetadata CStakingTransactionHandler::GetStakingDepositTxMetadata() const {
    assert(txType == StakingTransactionType::DEPOSIT);
    return stakingTxMetadata.depositTxMetadata;
}

CStakingBurnTxMetadata CStakingTransactionHandler::GetStakingBurnTxMetadata() const {
    assert(txType == StakingTransactionType::BURN);
    return stakingTxMetadata.burnTxMetadata;
}
