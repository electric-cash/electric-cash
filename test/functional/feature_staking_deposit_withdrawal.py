import decimal
import math

from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
)

STAKING_TX_HEADER = 0x53
STAKING_TX_BURN_SUBHEADER = 0x42
STAKING_TX_DEPOSIT_SUBHEADER = 0x44
STAKING_PENALTY_PERCENTAGE = 3.0


class StakingDepositWithdrawalTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.early_withdrawal_test()
        self.normal_withdrawal_test()
        self.gestakinginfo_rpc_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def get_stake_reward(self, txid: str, node_num: int) -> float:
        return self.nodes[node_num].getstakeinfo(txid)['accumulated_reward']

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
        i = 0
        while True:
            unspent = self.nodes[node_num].listunspent()[i]
            if unspent["amount"] * COIN >= deposit_amount:
                break
            i = i + 1
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
                                    self.nodes[0].sendrawtransaction, signed_raw_tx["hex"])
        else:
            return self.nodes[node_num].sendrawtransaction(signed_raw_tx["hex"])

    def early_withdrawal_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        deposit_amount = 400 * COIN
        staking_penalty = int(deposit_amount * STAKING_PENALTY_PERCENTAGE / 100.0)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than ' \
                                                                f'{starting_height, node0_height, node1_height}'

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr1)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == (starting_height + 10), 'Difference in nodes height'

        # send staking deposit transaction
        stake_id = self.send_staking_deposit_tx(addr1, deposit_amount, node_num=0)
        # mine some blocks
        self.nodes[0].generate(3)
        self.sync_all()

        # try to spend the stake
        # verify that the stake cannot be spent without paying the penalty
        self.spend_stake(0, stake_id, addr1, early_withdrawal=False, expected_rpc_err=-26)

        # try to spend the stake paying the staking penalty
        txid = self.spend_stake(0, stake_id, addr1, early_withdrawal=True)
        assert txid is not None

        # check if staking pool balance increased by the penalty
        self.nodes[0].generate(1)
        expected_staking_pool_balance = node0_staking_balance + 4 * staking_reward + staking_penalty
        assert self.get_staking_pool_balance(
            node_num=0) == expected_staking_pool_balance, f"Staking pool balance ({self.get_staking_pool_balance(node_num=0)}) is not equal to expected value ({expected_staking_pool_balance})"

    def normal_withdrawal_test(self):
        staking_reward = 50 * COIN
        deposit_amount = 300 * COIN
        staking_percentage = 5.0
        stake_length_months = 1
        stake_length_blocks = stake_length_months * 30 * 144
        expected_reward_per_block = math.floor(deposit_amount * staking_percentage / 100.0 / 360.0 / 144.0)
        expected_reward = expected_reward_per_block * stake_length_blocks

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, f'Starting heights differ ' \
                                             f'{node0_height, node1_height}'

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr1)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, 'Difference in nodes height'

        # send staking deposit transaction
        stake_id = self.send_staking_deposit_tx(addr1, deposit_amount, node_num=0)
        # mine some blocks
        self.nodes[0].generate(stake_length_blocks + 1)
        self.sync_all()
        # try to spend the stake

        assert self.get_stake_reward(stake_id, 0) * COIN == expected_reward
        txid = self.spend_stake(0, stake_id, addr1, expected_reward=expected_reward)
        assert txid is not None

        self.nodes[0].generate(1)
        node0_height = self.nodes[0].getblockcount()

        blocks_after_reward_reduction = max(0, node0_height - 4199)
        blocks_before_reward_reduction = (stake_length_blocks + 2) - blocks_after_reward_reduction
        expected_staking_pool_balance = node0_staking_balance + blocks_after_reward_reduction * 7.5 * COIN + \
                                        blocks_before_reward_reduction * staking_reward - expected_reward

        assert self.get_staking_pool_balance(
            node_num=0) == expected_staking_pool_balance, f"Staking pool balance ({self.get_staking_pool_balance(node_num=0)}) differs from expected value ({expected_staking_pool_balance})"

    def gestakinginfo_rpc_test(self):
        month_stake_length_blocks = 30 * 144

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr0 = self.nodes[0].getnewaddress()
        addr1 = self.nodes[1].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr0)
        self.nodes[1].generatetoaddress(20, addr1)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, 'Difference in nodes height'

        self.send_staking_deposit_tx(addr0, deposit_amount=300 * COIN, node_num=0)
        self.send_staking_deposit_tx(addr1, deposit_amount=200 * COIN, node_num=1)
        self.nodes[0].generate(int(0.4 * month_stake_length_blocks))
        self.nodes[1].generate(int(0.6 * month_stake_length_blocks))
        self.sync_all()
        staking_info_0 = self.nodes[0].getstakinginfo()
        staking_info_1 = self.nodes[1].getstakinginfo()

        assert staking_info_0['staking_pool'] == staking_info_1['staking_pool']
        assert staking_info_0['num_stakes'] == staking_info_1['num_stakes'] == 2
        assert staking_info_0['total_staked'] == staking_info_1['total_staked'] == 500 * COIN

        self.nodes[1].generate(int(0.5 * month_stake_length_blocks))
        self.sync_all()
        staking_info_0 = self.nodes[0].getstakinginfo()
        staking_info_1 = self.nodes[1].getstakinginfo()

        assert staking_info_0['staking_pool'] == staking_info_1['staking_pool']
        assert staking_info_0['num_stakes'] == staking_info_1['num_stakes'] == 0
        assert staking_info_0['total_staked'] == staking_info_1['total_staked'] == 0


if __name__ == '__main__':
    StakingDepositWithdrawalTest().main()
