#ifndef ELECTRIC_CASH_CFREETXINFO_H
#define ELECTRIC_CASH_CFREETXINFO_H

#include <utility>
#include <vector>
#include <boost/serialization/access.hpp>
#include <uint256.h>

class CFreeTxInfo {
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive& archive, const unsigned int version) {
        archive & m_used_limit_unconfirmed;
        archive & m_used_limit_confirmed;
        archive & m_limit;
        archive & m_current_window_start_height;
        archive & m_active_stake_ids;
        archive & m_unconfirmed_transactions;
        archive & m_valid;
    }
    uint32_t m_used_limit_unconfirmed;
    uint32_t m_used_limit_confirmed;
    uint32_t m_limit;
    uint32_t m_current_window_start_height;
    std::set<uint256> m_active_stake_ids;
    std::map<uint256, uint32_t> m_unconfirmed_transactions;
    bool m_valid = false;
public:
    CFreeTxInfo() = default;
    CFreeTxInfo(const uint32_t limit_in, const uint32_t current_window_start_height_in, std::set<uint256> active_stake_ids_in):
            m_used_limit_unconfirmed(0),
            m_used_limit_confirmed(0),
            m_limit(limit_in),
            m_current_window_start_height(current_window_start_height_in),
            m_active_stake_ids(std::move(active_stake_ids_in)),
            m_unconfirmed_transactions({}),
            m_valid(true) {
    }
    bool isValid() const { return m_valid; }
    uint32_t getUsedUnconfirmedLimit() const { return m_used_limit_confirmed; }
    uint32_t getUsedConfirmedLimit() const { return m_used_limit_confirmed; }
    uint32_t getLimit() const { return m_limit; }
    uint32_t getCurrentWindowStartHeight() const { return m_current_window_start_height; }
    const std::set<uint256>& getActiveStakeIds() const { return m_active_stake_ids; }

    void setLimit(const uint32_t limit) { m_limit = limit; }
    void setUsedConfirmedLimit(const uint32_t used_limit) { m_used_limit_confirmed = used_limit; }
    void setUsedUnconfirmedLimit(const uint32_t used_limit) { m_used_limit_confirmed = used_limit; }
    void setActiveStakeIds(std::set<uint256> active_stake_ids) { m_active_stake_ids = std::move(active_stake_ids); }
    void setCurrentWindowStartHeight(const uint32_t current_window_start_height_in) { m_current_window_start_height = current_window_start_height_in; }

    bool increaseUsedConfirmedLimit(const uint32_t size) {
        if (m_used_limit_confirmed + size > m_limit) return false;
        m_used_limit_confirmed += size;
        return true;
    }

    bool decreaseUsedConfirmedLimit(const uint32_t size) {
        if (size > m_used_limit_confirmed) return false;
        m_used_limit_confirmed -= size;
        return true;
    }

    bool addUnconfirmedTxId(const uint256& txid, const uint32_t size) {
        if (!m_unconfirmed_transactions.count(txid)) {
            if (m_used_limit_unconfirmed + size > m_limit) return false;
            m_unconfirmed_transactions.insert(std::make_pair(txid, size));
            m_used_limit_unconfirmed += size;
            return true;
        }
        return true;
    }

    void removeUnconfirmedTxId(const uint256& txid) {
        if (m_unconfirmed_transactions.count(txid)) {
            m_used_limit_confirmed -= m_unconfirmed_transactions[txid];
            m_unconfirmed_transactions.erase(txid);
        }
    }


};


#endif //ELECTRIC_CASH_CFREETXINFO_H
