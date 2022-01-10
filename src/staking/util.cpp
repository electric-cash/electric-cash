#include <staking/util.h>
#include <consensus/validation.h>
#include <key_io.h>
#include <streams.h>
#include <clientversion.h>

std::string scriptToStr(const CScript& script) {
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << script;
    return ds.str();
}

CScript scriptFromStr(const std::string& script_str) {
    CDataStream ds(script_str.c_str(), script_str.c_str() + script_str.size(), SER_DISK, CLIENT_VERSION);
    CScript res;
    ds >> res;
    return res;
}

uint32_t getTransactionVSize(const CTransaction& tx) {
    return (GetTransactionWeight(tx) + WITNESS_SCALE_FACTOR - 1) / WITNESS_SCALE_FACTOR;
}
