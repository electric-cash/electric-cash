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
        archive & m_used_limit;
        archive & m_limit;
        archive & m_current_window_start_height;
        archive & m_active_stake_ids;
    }
    uint32_t m_used_limit;
    uint32_t m_limit;
    uint32_t m_current_window_start_height;
    std::set<uint256> m_active_stake_ids;
    bool m_valid = false;
public:
    CFreeTxInfo() = default;
    CFreeTxInfo(uint32_t used_limit_in, uint32_t limit_in, uint32_t current_window_start_height_in, std::set<uint256>  active_stake_ids_in):
        m_used_limit(used_limit_in), m_limit(limit_in), m_current_window_start_height(current_window_start_height_in), m_active_stake_ids(std::move(active_stake_ids_in)) {
    m_valid = true;
    }

    bool isValid() const { return m_valid; }
};


#endif //ELECTRIC_CASH_CFREETXINFO_H
