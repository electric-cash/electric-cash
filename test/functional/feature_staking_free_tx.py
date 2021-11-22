import decimal

from test_framework.messages import COIN
from test_framework.staking_utils import FreeTransactionMixin
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_raises_rpc_error


class StakingBurnTest(BitcoinTestFramework, FreeTransactionMixin):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [
            ["-txindex"],
            ["-txindex"]
        ]

    def run_test(self):
        self.free_tx_simple_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def send_staking_deposit_tx(self, stake_address: str, deposit_value: float, node_num: int,
                                change_address: str = None):
        txid = self.nodes[node_num].depositstake(deposit_value, 4320, stake_address)
        return txid

    def free_tx_simple_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        deposit_value = 6
        deposit_amount = deposit_value * COIN
        free_tx_base_value = 10
        free_tx_base_amount = free_tx_base_value * COIN
        free_tx_value = 9
        free_tx_amount = free_tx_value * COIN

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than ' \
                                                                f'{starting_height, node0_height, node1_height}'

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        addr2 = self.nodes[0].getnewaddress()
        dummy_addresses = [self.nodes[1].getnewaddress() for _ in range(60)]
        self.nodes[0].generate(10)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == (starting_height + 10), 'Difference in nodes height'

        # send staking burn transaction
        stake_id = self.send_staking_deposit_tx(addr2, deposit_value, 0)
        free_tx_base_id = self.nodes[0].sendtoaddress(addr2, free_tx_base_value)
        invalid_free_tx_base_id = self.nodes[0].sendtoaddress(addr1, free_tx_base_value)

        self.nodes[0].generate(1)
        self.sync_all()
        # check if the transaction over the hard-coded limit of 1kB will be rejected
        assert_raises_rpc_error(-26, "invalid-free-transaction", self.send_free_tx, dummy_addresses, free_tx_amount, 0, addr2)

        # check if the transaction that has no underlying stake will be rejected
        assert_raises_rpc_error(-26, "invalid-free-transaction", self.send_free_tx, [self.nodes[1].getnewaddress()], free_tx_amount, 0, addr1)

        # check if a correct transaction will be accepted
        free_tx_id = self.send_free_tx(dummy_addresses[:2], free_tx_amount, 0, addr2)
        assert free_tx_id is not None

        self.nodes[0].generate(1)
        free_tx = self.nodes[0].getrawtransaction(free_tx_id, True)
        print(free_tx)
        assert free_tx["confirmations"] == 1

if __name__ == '__main__':
    StakingBurnTest().main()
