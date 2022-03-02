import math

from test_framework.messages import COIN
from test_framework.staking_utils import DepositStakingTransactionsMixin, STAKING_PENALTY_PERCENTAGE
from test_framework.test_framework import BitcoinTestFramework


class StakingGovPowerBasicTest(BitcoinTestFramework, DepositStakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.early_withdrawal_test()
        self.normal_withdrawal_test()

    def get_gp(self, address: str, node_num: int):
        return self.nodes[node_num].getgovpower(address)

    def early_withdrawal_test(self):
        deposit_value = 5
        deposit_amount = deposit_value * COIN

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr1)
        self.sync_all()

        # send staking deposit transaction
        stake_id = self.send_staking_deposit_tx(addr1, deposit_amount, node_num=0)
        assert self.get_gp(addr1, 0) == 0

        # mine some blocks
        self.nodes[0].generate(3)
        self.sync_all()
        assert self.get_gp(addr1, 0) > 0

        # try to spend the stake paying the staking penalty
        txid = self.spend_stake(0, stake_id, addr1, early_withdrawal=True)
        assert txid is not None

        # check if staking pool balance increased by the penalty
        self.nodes[0].generate(1)
        assert self.get_gp(addr1, 0) > 0

    def normal_withdrawal_test(self):
        deposit_value = 5
        deposit_amount = deposit_value * COIN
        staking_percentage = 5.0
        stake_length_months = 1
        stake_length_blocks = stake_length_months * 30 * 144
        expected_reward_per_block = math.floor(deposit_amount * staking_percentage / 100.0 / 360.0 / 144.0)
        expected_reward = expected_reward_per_block * stake_length_blocks

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr1)
        self.sync_all()
        assert self.get_gp(addr1, 0) == 0

        # send staking deposit transaction
        stake_id = self.send_staking_deposit_tx(addr1, deposit_amount, node_num=0)
        # mine some blocks
        num_div = 54
        for i in range(num_div):
            self.nodes[0].generate(stake_length_blocks // num_div)
            self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        # try to spend the stake
        assert 1.01e8 > self.get_gp(addr1, 0) > 0.99e8, f"Invalid gp balance: {self.get_gp(addr1, 0)}"

        txid = self.spend_stake(0, stake_id, addr1, expected_reward=expected_reward)
        assert txid is not None

        self.nodes[0].generate(1)
        assert 1.01e8 > self.get_gp(addr1, 0) > 0.99e8, f"Invalid gp balance: {self.get_gp(addr1, 0)}"


if __name__ == '__main__':
    StakingGovPowerBasicTest().main()
