from test_framework.messages import COIN
from test_framework.staking_utils import FreeTransactionMixin
from test_framework.test_framework import BitcoinTestFramework

from time import sleep

class FreeTxPreferenceTest(BitcoinTestFramework, FreeTransactionMixin):
    def set_test_params(self):
        self.blockmaxweight = 128000
        self.num_nodes = 2
        self.extra_args = [
            ["-txindex", f'-blockmaxweight={self.blockmaxweight}'],
            ["-txindex", f'-blockmaxweight={self.blockmaxweight}']
        ]

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def send_staking_deposit_tx(self, stake_address: str, deposit_value: float, node_num: int):
        txid = self.nodes[node_num].depositstake(deposit_value, 4320, stake_address)
        return txid

    def run_test(self):
        self.free_tx_trasaction_preference()

    def assert_block_tx_preference(self, free_tx_list):
        self.nodes[0].generate(1)
        last_block = self.nodes[0].getblock(self.nodes[0].getbestblockhash())

        free_tx_size = 0
        nonfree_tx_size = 0
        for tx in last_block["tx"]:
            tx_size = len(
                bytes.fromhex(
                    self.nodes[0].gettransaction(tx)['hex']
                    )
                )

            if tx in free_tx_list:
                free_tx_size = free_tx_size + tx_size
            else:
                nonfree_tx_size = nonfree_tx_size + tx_size

        free_tx_percentage = free_tx_size / self.blockmaxweight
        assert free_tx_percentage < .28 and free_tx_percentage > .24 # TODO figure out how to make it more precise

    def generate_inputs_for_free_txs(self, amount_if_tested_free_tx, addr):
        for i in range(amount_if_tested_free_tx):
            self.send_staking_deposit_tx(addr, self.deposit_value, 0)
            tx_id = self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            tx_id = self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            tx_id = self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            tx_id = self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].generate(1)

        sleep(60) # it is too tx much so sync_all() times out without this
        self.sync_all()

    def free_tx_trasaction_preference(self):
        starting_height = 200
        self.deposit_value = 6
        self.free_tx_base_value = 10
        free_tx_value = 9
        free_tx_amount = free_tx_value * COIN
        amount_if_tested_free_tx = 100
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than ' \
                                                                f'{starting_height, node0_height, node1_height}'

        # generate blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        addr2 = self.nodes[0].getnewaddress()
        dummy_addresses = [self.nodes[1].getnewaddress() for _ in range(60)]

        self.nodes[0].generatetoaddress(170, addr2)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, 'Difference in nodes height'

        self.generate_inputs_for_free_txs(amount_if_tested_free_tx, addr2)

        # load free txs into mempool
        free_tx_list = []
        for _ in range(amount_if_tested_free_tx):
            free_tx_id = self.send_free_tx(dummy_addresses[:5], free_tx_amount, 0, addr2)
            assert free_tx_id is not None
            free_tx_list.append(free_tx_id)

        self.assert_block_tx_preference(free_tx_list)




if __name__ == '__main__':
    FreeTxPreferenceTest().main()