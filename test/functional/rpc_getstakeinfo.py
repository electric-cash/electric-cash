import math

from test_framework.messages import COIN
from test_framework.staking_utils import DepositStakingTransactionsMixin
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_raises_rpc_error


class GetStakeInfoTest(BitcoinTestFramework, DepositStakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2
        self.rpc_timeout = 120

    def run_test(self):
        self.getstakeinfo_rpc_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def getstakeinfo_rpc_test(self):
        month_stake_length_blocks = 30 * 144
        deposit_value = 300
        deposit_amount = deposit_value * COIN
        staking_percentage = 5.0
        expected_reward_per_block = math.floor(deposit_amount * staking_percentage / 100.0 / 360.0 / 144.0)

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr0 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr0)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, 'Difference in nodes height'

        stake_id = self.send_staking_deposit_tx(addr0, deposit_amount=deposit_amount, node_num=0)

        # raise error when transaction still in mempool
        assert_raises_rpc_error(-5, 'Stake not found', self.nodes[0].getstakeinfo, stake_id)

        self.nodes[0].generate(1)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * 1, 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == False
        assert stake_info['paid_out'] == False

        # mine blocks for the half of the staking period
        # only accumulated reward is changing
        blocks_to_mine = int(0.5 * month_stake_length_blocks)
        self.nodes[0].generate(blocks_to_mine)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * (1 + blocks_to_mine), 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == False
        assert stake_info['paid_out'] == False

        # mine blocks for the rest of the staking period
        # stake is fullfiled but not paid out yet
        blocks_to_mine = int(0.5 * month_stake_length_blocks)
        self.nodes[0].generate(blocks_to_mine)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * month_stake_length_blocks, 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == True
        assert stake_info['paid_out'] == False

        # withdraw staking transaction
        self.spend_stake(0, stake_id, addr0, expected_reward=expected_reward_per_block * month_stake_length_blocks)

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['paid_out'] == False

        self.nodes[0].generate(1)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * month_stake_length_blocks, 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == True
        assert stake_info['paid_out'] == True

        # test early withdraw of new staking transaction
        addr1 = self.nodes[0].getnewaddress()
        node0_height = self.nodes[0].getblockcount()
        stake_id = self.send_staking_deposit_tx(addr1, deposit_amount=deposit_amount, node_num=0)
        self.nodes[0].generate(1)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * 1, 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == False
        assert stake_info['paid_out'] == False

        self.nodes[0].generate(100)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)
        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * (1 + 100), 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == False
        assert stake_info['paid_out'] == False

        self.spend_stake(0, stake_id, addr1, early_withdrawal=True)
        self.nodes[0].generate(50)
        self.sync_all()

        stake_info = self.nodes[0].getstakeinfo(stake_id)

        assert stake_info['deposit_height'] == node0_height + 1, 'Wrong deposit height'
        assert stake_info['staking_period'] == month_stake_length_blocks, 'Wrong staking period'
        assert stake_info['staking_amount'] == deposit_value, 'Wrong staking amount'
        assert stake_info['accumulated_reward'] * COIN == expected_reward_per_block * (1 + 100), 'Wrong accumulated reward'
        assert stake_info['fulfilled'] == False
        assert stake_info['paid_out'] == True


if __name__ == "__main__":
    GetStakeInfoTest().main()
