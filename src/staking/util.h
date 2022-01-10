#ifndef ELECTRIC_CASH_STAKING_UTIL_H
#define ELECTRIC_CASH_STAKING_UTIL_H

#include <stdint.h>
#include <string>
#include <script/script.h>
#include <primitives/transaction.h>

std::string scriptToStr(const CScript& script);
CScript scriptFromStr(const std::string& script_str);
uint32_t getTransactionVSize(const CTransaction& tx);

#endif //ELECTRIC_CASH_STAKING_UTIL_H
