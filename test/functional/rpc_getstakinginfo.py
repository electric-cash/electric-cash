from test_framework.messages import COIN
from test_framework.staking_utils import DepositStakingTransactionsMixin
from test_framework.test_framework import BitcoinTestFramework


class GetStakingInfoTest(BitcoinTestFramework, DepositStakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2
        self.rpc_timeout = 120

    def run_test(self):
        self.gestakinginfo_rpc_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

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
    GetStakingInfoTest().main()
