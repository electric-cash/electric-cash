import decimal

import math

from test_framework.messages import COIN
from test_framework.staking_utils import DepositStakingTransactionsMixin, STAKING_PENALTY_PERCENTAGE
from test_framework.test_framework import BitcoinTestFramework

class StakingDepositWithdrawalTest(BitcoinTestFramework, DepositStakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.early_withdrawal_test()
        self.normal_withdrawal_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
            return self.nodes[node_num].getstakinginfo()['staking_pool']

    def get_stake_reward(self, txid: str, node_num: int) -> float:
        return self.nodes[node_num].getstakeinfo(txid)['accumulated_reward']

    def get_stake_amount(self, txid: str, node_num: int) -> float:
        return self.nodes[node_num].getstakeinfo(txid)['staking_amount']

    def send_staking_deposit_tx(self, stake_address: str, deposit_value: decimal.Decimal, node_num: int,
                                change_address: str = None):
        txid = self.nodes[node_num].depositstake(deposit_value, 4320, stake_address)
        return txid

    def assure_same_height(self, height):
        last_node_height = self.nodes[0].getblockcount()
        for index, node in enumerate(self.nodes):
            node_height = node.getblockcount()
            assert node_height == last_node_height == height, f'Difference in nodes height at node index: {index}'
            last_node_height = node_height

    def assure_same_staking_balance(self):
        # assure that staking balance is the same on both nodes

        last_balance = self.get_staking_pool_balance(node_num=0)
        for index, _ in enumerate(self.nodes):
            balance = self.get_staking_pool_balance(node_num=index)
            assert last_balance == balance, f'Difference in nodes balance at index: {index}'
            last_balance = balance

    def early_withdrawal_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        deposit_value = 400
        deposit_amount = deposit_value * COIN
        staking_penalty = int(deposit_amount * STAKING_PENALTY_PERCENTAGE / 100.0)

        self.assure_same_height(starting_height)

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        generated_blocks = 100
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(generated_blocks, addr1)

        self.sync_all()
        self.assure_same_staking_balance()
        self.assure_same_height(starting_height+generated_blocks)

        staking_balance_before = self.get_staking_pool_balance(node_num=0)

        # send staking deposit transaction
        stake_id = self.send_staking_deposit_tx(addr1, deposit_value, node_num=0)
        for _ in range(150):
            self.send_staking_deposit_tx(addr1, 6, node_num=0)
        # mine some blocks
        self.nodes[0].generate(3)
        self.sync_all()
        assert self.get_stake_amount(stake_id, 1) == deposit_value, f"Stake amount different than expected: {self.get_stake_amount(stake_id, 1)}, {deposit_value}"

        # try to spend the stake
        # verify that the stake cannot be spent without paying the penalty
        self.spend_stake(0, stake_id, addr1, early_withdrawal=False, expected_rpc_err=-26)

        # try to spend the stake paying the staking penalty
        txid = self.spend_stake(0, stake_id, addr1, early_withdrawal=True)
        assert txid is not None

        # check if staking pool balance increased by the penalty
        self.nodes[0].generate(1)

        staking_balance_after = self.get_staking_pool_balance(node_num=0)
        expected_staking_pool_balance = staking_balance_before + 4 * staking_reward + staking_penalty
        assert staking_balance_after == expected_staking_pool_balance, f"Staking pool balance ({staking_balance_after}) is not equal to expected value ({expected_staking_pool_balance})"

    def normal_withdrawal_test(self):
        staking_reward = 50 * COIN
        deposit_value = 300
        deposit_amount = deposit_value * COIN
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
        stake_id = self.send_staking_deposit_tx(addr1, deposit_value, node_num=0)
        # mine some blocks
        num_div = 54
        for i in range(num_div):
            self.nodes[0].generate(stake_length_blocks // num_div)
            self.sync_all()
        self.nodes[0].generate(1)
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


if __name__ == '__main__':
    StakingDepositWithdrawalTest().main()
