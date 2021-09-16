from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_raises_rpc_error
from feature_staking_deposit_withdrawal import StakingTransactionsMixin


class GetStakesForAddressTest(BitcoinTestFramework, StakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2
        self.rpc_timeout = 120

    def run_test(self):
        self.getstakesforaddress_rpc_test()
        self.getstakesforaddress_rpc_invalid_address_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']
    
    def getstakesforaddress_rpc_test(self):
        month_stake_length_blocks = 30 * 144

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

        txids_set = set()

        txid1 = self.send_staking_deposit_tx(addr0, deposit_amount=50 * COIN, node_num=0)
        txids_set.add(txid1)
        self.nodes[0].generate(int(0.2 * month_stake_length_blocks))
        self.sync_all()
        assert txids_set == set(self.nodes[0].getstakesforaddress(addr0))

        txid2 = self.send_staking_deposit_tx(addr0, deposit_amount=50 * COIN, node_num=0)
        txids_set.add(txid2)
        self.nodes[0].generate(int(0.2 * month_stake_length_blocks))
        self.sync_all()
        assert txids_set == set(self.nodes[0].getstakesforaddress(addr0))

        txid3 = self.send_staking_deposit_tx(addr0, deposit_amount=50 * COIN, node_num=0)
        txids_set.add(txid3)
        self.nodes[0].generate(int(0.2 * month_stake_length_blocks))
        self.sync_all()
        assert txids_set == set(self.nodes[0].getstakesforaddress(addr0))

        addr1 = self.nodes[1].getnewaddress()
        assert set(self.nodes[0].getstakesforaddress(addr1)) == set()

        self.nodes[0].generate(int(0.5 * month_stake_length_blocks))
        self.sync_all()
        txids_set.remove(txid1)
        assert txids_set == set(self.nodes[0].getstakesforaddress(addr0))

        self.nodes[0].generate(int(0.2 * month_stake_length_blocks))
        self.sync_all()
        txids_set.remove(txid2)
        assert txids_set == set(self.nodes[0].getstakesforaddress(addr0))

        self.nodes[0].generate(int(0.2 * month_stake_length_blocks))
        self.sync_all()
        txids_set.remove(txid3)
        assert txids_set == set(self.nodes[0].getstakesforaddress(addr0))

    def getstakesforaddress_rpc_invalid_address_test(self):
        assert_raises_rpc_error(-5, 'Invalid ELCASH address', self.nodes[0].getstakesforaddress, 'invalid-address')


if __name__ == "__main__":
    GetStakesForAddressTest().main()
