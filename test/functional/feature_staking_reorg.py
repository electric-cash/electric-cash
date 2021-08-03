from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes,
    disconnect_nodes,
    assert_raises_rpc_error,
)

import decimal
import math

STAKING_TX_HEADER = 0x53
STAKING_TX_BURN_SUBHEADER = 0x42
STAKING_TX_DEPOSIT_SUBHEADER = 0x44
STAKING_PENALTY_PERCENTAGE = 3.0


class StakingReorgTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.reorg_cancelled_deposit_tx()

    def get_staking_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    @staticmethod
    def create_tx_inputs_and_outputs(stake_address: str, change_address: str, deposit_amount: decimal.Decimal,
                                     utxo: dict):
        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }
        # tx outputs
        tx_output_staking_deposit_header = {
            "data": bytes([STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, 0x00]).hex()
        }

        tx_output_stake = {
            stake_address: deposit_amount / COIN
        }

        change_amount = (utxo["amount"] * COIN - deposit_amount - fee) / COIN
        if change_amount > 0.0001:
            tx_output_change = {
                change_address: change_amount
            }
            return [tx_input], [tx_output_staking_deposit_header, tx_output_stake, tx_output_change]
        else:
            return [tx_input], [tx_output_staking_deposit_header, tx_output_stake]

    def send_staking_deposit_tx(self, stake_address: str, deposit_amount: decimal.Decimal, node_num: int,
                                change_address: str = None):
        unspent = self.nodes[node_num].listunspent()[0]
        assert unspent[
                   "amount"] * COIN > deposit_amount, f"Amount too big ({deposit_amount}) to be sent from this UTXO " \
                                                      f"({unspent['amount'] * COIN})"
        if not change_address:
            change_address = self.nodes[node_num].getnewaddress()
        tx_inputs, tx_outputs = self.create_tx_inputs_and_outputs(stake_address, change_address, deposit_amount,
                                                                  unspent)
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[node_num].createrawtransaction(tx_inputs, tx_outputs)
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        txid = self.nodes[0].sendrawtransaction(signed_raw_tx['hex'])
        return txid

    def spend_stake(self, node_num: int, stake_txid: str, stake_address: str, early_withdrawal: bool = False,
                    expected_reward: int = 0, expected_rpc_err: int = None):
        unspent = [u for u in self.nodes[node_num].listunspent() if u["txid"] == stake_txid and u["address"] == stake_address]
        assert len(unspent) == 1
        utxo = unspent[0]
        staking_penalty = int(utxo["amount"] * COIN * decimal.Decimal(STAKING_PENALTY_PERCENTAGE / 100.0)) if early_withdrawal else 0
        reward = expected_reward if not early_withdrawal else 0
        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }
        tx_output = {
            stake_address: (utxo["amount"] * COIN - fee - staking_penalty + reward) / COIN
        }
        raw_tx = self.nodes[node_num].createrawtransaction([tx_input], [tx_output])
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        if expected_rpc_err is not None:
            assert_raises_rpc_error(expected_rpc_err, None,
                                    self.nodes[0].sendrawtransaction, signed_raw_tx["hex"])
        else:
            return self.nodes[node_num].sendrawtransaction(signed_raw_tx["hex"])

    def reorg_cancelled_deposit_tx(self):
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward
        deposit_amount = 300 * COIN
        staking_percentage = 5.0
        stake_length_months = 1
        stake_length_blocks = stake_length_months * 30 * 144
        expected_reward = math.floor(deposit_amount * staking_percentage / 100.0 * stake_length_months / 12.0)
        starting_height = 200
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward

        addr1 = self.nodes[0].getnewaddress()
        # assure that nodes start with the same height
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than {starting_height}'

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == starting_staking_balance, f'Starting staking balance different than {starting_staking_balance}'

        # generate 15 blocks on node 0, then sync nodes and check height and staking balance
        self.nodes[0].generate(15)
        self.sync_all()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == (starting_height + 15), 'Difference in nodes height'

        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == (starting_staking_balance + 15 * staking_reward), 'Difference in nodes staking pool balance'

        # disconnect nodes
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.send_staking_deposit_tx(addr1, deposit_amount, 0)
        self.nodes[0].generate(1122)
        self.nodes[1].generate(1219)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node1_height - node0_height == 97, 'Wrong diff in nodes height'

        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        # staking pool balance after nodes reconnecting
        target_staking_balance = max(node0_staking_balance, node1_staking_balance)
        assert node0_staking_balance < node1_staking_balance, 'Staking pool balance wrong inequality'

        # reconnecting nodes
        self.log.info('Reconnect nodes')
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_blocks()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == target_height, f'Nodes heights different than {target_height}'

        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == target_staking_balance, f'Nodes staking balance ({node0_staking_balance, node1_staking_balance}) different than {target_staking_balance}'


if __name__ == '__main__':
    StakingReorgTest().main()