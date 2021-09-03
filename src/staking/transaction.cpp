#include "transaction.h"
#include <streams.h>
#include <iostream>


CStakingTransactionParser::CStakingTransactionParser(CTransactionRef txIn) {
    tx = txIn;
    txType = ProcessTransaction();
}


StakingTransactionType CStakingTransactionParser::ProcessTransaction() {
    if (tx->vout.empty()) {
        return StakingTransactionType::NONE;
    }
    const CScript& txHeaderScript = tx->vout[0].scriptPubKey;
    if (!IsStakingTxHeader(txHeaderScript)) {
        return StakingTransactionType::NONE;
    }
    if (IsStakingDepositTxSubHeader(txHeaderScript)) {
        return CStakingTransactionParser::ValidateStakingDepositTx();
    } else if (IsStakingBurnTxSubHeader(txHeaderScript)) {
        return CStakingTransactionParser::ValidateStakingBurnTx();
    } else {
        return StakingTransactionType::NONE;
    }
}

StakingTransactionType CStakingTransactionParser::ValidateStakingDepositTx() {
    CDataStream ds(0,0);
    tx->vout[0].scriptPubKey.Serialize(ds); // TODO: doing memcpy instead of serializing would be more efficient.
    ser_readdata8(ds); // read the size
    const uint8_t op = ser_readdata8(ds);
    const uint8_t push_size = ser_readdata8(ds);
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
    if (outputIndex == 0 || outputIndex >= tx->vout.size() || tx->vout[outputIndex].nValue < stakingParams::MIN_STAKING_AMOUNT) {
        return StakingTransactionType::NONE;
    }

    // Read and validate staking period index
    uint8_t stakingPeriod;
    try {
        stakingPeriod = ser_readdata8(ds);
    } catch (...) {
        return StakingTransactionType::NONE;
    }
    if (stakingPeriod >= stakingParams::NUM_STAKING_PERIODS) {
        return StakingTransactionType::NONE;
    }
    stakingTxMetadata.depositTxMetadata = CStakingDepositTxMetadata(outputIndex, stakingPeriod);
    return StakingTransactionType::DEPOSIT;
}


/* NOTE: The following method has no access to the value of transaction inputs, so it only validates the syntax.
 * Validation of values must be done separately
 */
StakingTransactionType CStakingTransactionParser::ValidateStakingBurnTx() {
    CDataStream ds(0,0);
    tx->vout[0].scriptPubKey.Serialize(ds); // TODO: doing memcpy instead of serializing would be more efficient.
    ser_readdata8(ds); // read the size
    const uint8_t op = ser_readdata8(ds);
    const uint8_t push_size = ser_readdata8(ds);
    const uint8_t header = ser_readdata8(ds);
    const uint8_t subheader = ser_readdata8(ds);
    assert(op == OP_RETURN || header == STAKING_TX_HEADER || subheader == STAKING_TX_BURN_SUBHEADER);
    if (ds.size() < STAKING_BURN_TX_HEADER_SIZE - STAKING_HEADER_SIZE) {
        return StakingTransactionType::NONE;
    }
    CAmount amount;
    try {
        amount = static_cast<CAmount>(ser_readdata64(ds));
    } catch(...) {
        return StakingTransactionType::NONE;
    }

    if (!MoneyRange(amount)) {
        return StakingTransactionType::NONE;
    }

    stakingTxMetadata.burnTxMetadata = CStakingBurnTxMetadata(amount);
    return StakingTransactionType::BURN;
}

bool CStakingTransactionParser::IsStakingTxHeader(const CScript &script) {
    return (script.size() >= STAKING_HEADER_SIZE && script[0] == OP_RETURN && script[2] == STAKING_TX_HEADER);
}

bool CStakingTransactionParser::IsStakingDepositTxHeader(const CScript &script) {
    return (IsStakingTxHeader(script) && IsStakingDepositTxSubHeader(script));
}

bool CStakingTransactionParser::IsStakingBurnTxHeader(const CScript &script) {
    return (IsStakingTxHeader(script) && IsStakingBurnTxSubHeader(script));
}

bool CStakingTransactionParser::IsStakingDepositTxSubHeader(const CScript &script) {
    return (script.size() >= STAKING_HEADER_SIZE && script[3] == STAKING_TX_DEPOSIT_SUBHEADER);
}

bool CStakingTransactionParser::IsStakingBurnTxSubHeader(const CScript &script) {
    return (script.size() >= STAKING_HEADER_SIZE && script[3] == STAKING_TX_BURN_SUBHEADER);
}

StakingTransactionType CStakingTransactionParser::GetStakingTxType() const {
    return txType;
}

CStakingDepositTxMetadata CStakingTransactionParser::GetStakingDepositTxMetadata() const {
    assert(txType == StakingTransactionType::DEPOSIT);
    return stakingTxMetadata.depositTxMetadata;
}

CStakingBurnTxMetadata CStakingTransactionParser::GetStakingBurnTxMetadata() const {
    assert(txType == StakingTransactionType::BURN);
    return stakingTxMetadata.burnTxMetadata;
}
