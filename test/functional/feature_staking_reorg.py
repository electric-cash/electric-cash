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
        self.reorg_recalculate_reward()
        self.reorg_spend_stake()

    def get_staking_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def get_stake_reward(self, txid: str, node_num: int) -> float:
        return self.nodes[node_num].getstakeinfo(txid)['accumulated_reward']

    @staticmethod
    def create_tx_inputs_and_outputs(stake_address: str, change_address: str, deposit_amount: decimal.Decimal, deposit_idx: int,
                                     utxo: dict):
        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }
        # tx outputs
        tx_output_staking_deposit_header = {
            "data": bytes([STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, deposit_idx]).hex()
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

    def send_staking_deposit_tx(self, stake_address: str, deposit_amount: decimal.Decimal, period_idx: int, node_num: int,
                                change_address: str = None):
        unspent = self.nodes[node_num].listunspent()[0]
        assert unspent[
                   "amount"] * COIN > deposit_amount, f"Amount too big ({deposit_amount}) to be sent from this UTXO " \
                                                      f"({unspent['amount'] * COIN})"
        if not change_address:
            change_address = self.nodes[node_num].getnewaddress()
        tx_inputs, tx_outputs = self.create_tx_inputs_and_outputs(stake_address, change_address, deposit_amount, period_idx,
                                                                  unspent)
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[node_num].createrawtransaction(tx_inputs, tx_outputs)
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        txid = self.nodes[0].sendrawtransaction(signed_raw_tx['hex'])
        return txid

    def spend_stake(self, node_num: int, stake_txid: str, stake_address: str, early_withdrawal: bool = False,
                    expected_reward: int = 0, expected_rpc_err: int = None):
        unspent = [u for u in self.nodes[node_num].listunspent() if
                   u["txid"] == stake_txid and u["address"] == stake_address]
        assert len(unspent) == 1
        utxo = unspent[0]
        staking_penalty = int(
            utxo["amount"] * COIN * decimal.Decimal(STAKING_PENALTY_PERCENTAGE / 100.0)) if early_withdrawal else 0
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
                                    self.nodes[node_num].sendrawtransaction, signed_raw_tx["hex"])
        else:
            return self.nodes[node_num].sendrawtransaction(signed_raw_tx["hex"])

    def reorg_recalculate_reward(self):
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward
        deposit_amount = 300 * COIN
        staking_percentage = 5.0
        stake_length_months = 1
        stake_length_blocks = stake_length_months * 30 * 144
        expected_reward_per_block = math.floor(deposit_amount * staking_percentage / 100.0 / 360.0 / 144.0)
        starting_height = 200
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward

        addr1 = self.nodes[0].getnewaddress()
        # assure that nodes start with the same height
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, f'Starting nodes height difference: {node0_height, node1_height}'

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance, f'Starting staking balance difference: {node0_staking_balance, node1_staking_balance}'
        stake_txid = self.send_staking_deposit_tx(addr1, deposit_amount, 0, 0)
        # generate 15 blocks on node 0, then sync nodes and check height and staking balance
        self.nodes[0].generate(15)
        self.sync_all()
        # disconnect nodes
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.nodes[0].generate(100)
        self.nodes[1].generate(150)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node1_height - node0_height == 50, 'Wrong diff in nodes height'

        node0_expected_reward = (100 + 15) * expected_reward_per_block
        node1_expected_reward = (150 + 15) * expected_reward_per_block

        node0_reward = self.get_stake_reward(stake_txid, 0) * COIN
        node1_reward = self.get_stake_reward(stake_txid, 1) * COIN

        assert node0_reward == node0_expected_reward, f"Reward on node0 different than expected: {node0_reward, node0_expected_reward}"
        assert node1_reward == node1_expected_reward, f"Reward on node1 different than expected: {node1_reward, node1_expected_reward}"

        node0_expected_staking_balance = node0_staking_balance + (100 + 15) * staking_reward - 2 * node0_expected_reward # stake from previous test is there
        node1_expected_staking_balance = node1_staking_balance + (150 + 15) * staking_reward - 2 * node1_expected_reward # stake from previous test is there
        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node0_expected_staking_balance, f'Staking balance on node0 different than expected: f{node0_staking_balance, node0_expected_staking_balance}'
        assert node1_staking_balance == node1_expected_staking_balance, f'Staking balance on node1 different than expected: f{node1_staking_balance, node1_expected_staking_balance}'

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
        assert node0_staking_balance == node1_staking_balance, f'Difference in nodes staking balance ({node0_staking_balance, node1_staking_balance})'

        node0_reward = self.get_stake_reward(stake_txid, 0) * COIN
        node1_reward = self.get_stake_reward(stake_txid, 1) * COIN

        assert node0_reward == node1_reward == node1_expected_reward, f'Difference in staking rewards: {node0_reward, node1_reward, node1_expected_reward}'

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
        assert node0_staking_balance == node1_staking_balance == (
                starting_staking_balance + 15 * staking_reward), 'Difference in nodes staking pool balance'

        # disconnect nodes
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.send_staking_deposit_tx(addr1, deposit_amount, 0, 0)
        self.nodes[0].generate(19)
        self.nodes[1].generate(22)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node1_height - node0_height == 3, 'Wrong diff in nodes height'

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

    def reorg_spend_stake(self):
        staking_reward = 50 * COIN
        deposit_amount = 300 * COIN
        staking_percentage = 5.0
        stake_length_months = 1
        stake_length_blocks = stake_length_months * 30 * 144
        expected_reward_per_block = math.floor(deposit_amount * staking_percentage / 100.0 / 360.0 / 144.0)
        staking_reward = 50 * COIN

        addr1 = self.nodes[0].getnewaddress()

        # share private key so the second node can also spend the stake.
        pk = self.nodes[0].dumpprivkey(addr1)
        self.nodes[1].importprivkey(pk)

        # assure that nodes start with the same height
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, f'Starting nodes height difference: {node0_height, node1_height}'

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance, f'Starting staking balance difference: {node0_staking_balance, node1_staking_balance}'
        stake_txid = self.send_staking_deposit_tx(addr1, deposit_amount, 0, 0)
        # generate 15 blocks on node 0, then sync nodes and check height and staking balance
        self.nodes[0].generate(stake_length_blocks // 4 - 10)
        self.nodes[0].generate(stake_length_blocks // 4)
        self.nodes[0].generate(stake_length_blocks // 4)
        self.nodes[0].generate(stake_length_blocks // 4)
        self.sync_all(timeout=180)

        # disconnect nodes
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.nodes[0].generate(5)
        self.nodes[1].generate(15)

        node0_expected_reward = (stake_length_blocks - 5) * expected_reward_per_block
        node1_expected_reward = stake_length_blocks * expected_reward_per_block

        node0_reward = self.get_stake_reward(stake_txid, 0) * COIN
        node1_reward = self.get_stake_reward(stake_txid, 1) * COIN

        assert node0_reward == node0_expected_reward, f"Reward on node0 different than expected: {node0_reward, node0_expected_reward}"
        assert node1_reward == node1_expected_reward, f"Reward on node1 different than expected: {node1_reward, node1_expected_reward}"

        self.spend_stake(0, stake_txid, addr1, True)
        self.spend_stake(1, stake_txid, addr1, False, self.get_stake_reward(stake_txid, 1) * COIN)

        self.nodes[0].generate(1)
        self.nodes[1].generate(1)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node1_height - node0_height == 10, 'Wrong diff in nodes height'
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)

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
        assert node0_staking_balance == node1_staking_balance, f'Difference in nodes staking balance ({node0_staking_balance, node1_staking_balance})'

        node0_reward = self.get_stake_reward(stake_txid, 0) * COIN
        node1_reward = self.get_stake_reward(stake_txid, 1) * COIN

        assert node0_reward == node1_reward == node1_expected_reward, f'Difference in staking rewards: {node0_reward, node1_reward, node1_expected_reward}'



if __name__ == '__main__':
    StakingReorgTest().main()
