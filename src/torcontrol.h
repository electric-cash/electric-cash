// Copyright (c) 2015-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with Tor.
 */
#ifndef ELCASH_TORCONTROL_H
#define ELCASH_TORCONTROL_H


extern const std::string DEFAULT_TOR_CONTROL;
static const bool DEFAULT_LISTEN_ONION = true;

void StartTorControl();
void InterruptTorControl();
void StopTorControl();

#endif /* ELCASH_TORCONTROL_H */
