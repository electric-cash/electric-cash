from test_framework.messages import COIN
from test_framework.staking_utils import FreeTransactionMixin
from test_framework.test_framework import BitcoinTestFramework

from time import sleep

class FreeTxPreferenceTest(BitcoinTestFramework, FreeTransactionMixin):
    def set_test_params(self):
        self.free_tx_testing_proportion = 30 / 640000
        self.blockmaxweight = 640000
        self.maxFreeTxWeight = 20000
        self.num_nodes = 2
        self.extra_args = [
            ["-txindex"],
            ["-txindex"]
        ]

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def send_staking_deposit_tx(self, stake_address: str, deposit_value: float, node_num: int):
        txid = self.nodes[node_num].depositstake(deposit_value, 4320, stake_address)
        return txid

    def run_test(self):
        self.free_tx_trasaction_preference()

    def assert_block_tx_preference_only_free(self, free_tx_list, tested_range = 1000):
        last_block = self.nodes[0].getblock(self.nodes[0].getbestblockhash())

        free_tx_size = 0
        nonfree_tx_size = 0

        count = 0
        block_weight = last_block["weight"]
        block_size = last_block["size"]

        # by definition size to weight proportion is constatnt and depends on COIN settings
        size_weight_proportion = block_weight/block_size

        for tx in last_block["tx"]:
            tx_size = len(
                bytes.fromhex(
                    self.nodes[0].gettransaction(tx)['hex']
                    )
                )

            if tx in free_tx_list:
                free_tx_size = free_tx_size + tx_size
                count = count + 1
                print(tx_size*size_weight_proportion)

            else:
                nonfree_tx_size = nonfree_tx_size + tx_size
                print("P ", tx_size*size_weight_proportion)

        free_tx_weight = (free_tx_size*size_weight_proportion)

        print(len(last_block["tx"]))
        print (f"---------> Free tx weight {free_tx_size*size_weight_proportion}, non-free tx weight {nonfree_tx_size*size_weight_proportion} Block weight {block_weight}")
        print(free_tx_weight < self.maxFreeTxWeight)
        print(free_tx_weight > (self.maxFreeTxWeight - tested_range))
        assert free_tx_weight < self.maxFreeTxWeight and free_tx_weight > (self.maxFreeTxWeight - tested_range)
        return len(last_block["tx"])

    def assert_mempool_leftovers(self, excpected_txs_number):
        last_mempool_txs = self.nodes[0].getrawmempool()
        assert len(last_mempool_txs) == excpected_txs_number

    def generate_inputs_for_free_txs(self, amount_of_tested_free_tx, addr):
        for i in range(amount_of_tested_free_tx):
            self.send_staking_deposit_tx(addr, self.deposit_value, 0)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr, self.free_tx_base_value)
            self.nodes[0].generate(1)
            print(i)


    def free_tx_trasaction_preference(self):
        starting_height = 200
        self.deposit_value = 6
        self.free_tx_base_value = 10
        free_tx_value = 9
        free_tx_amount = free_tx_value * COIN
        amount_of_tested_free_tx = int(self.free_tx_testing_proportion * self.blockmaxweight)
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than ' \
                                                                f'{starting_height, node0_height, node1_height}'

        # generate blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[1].getnewaddress()
        addr2 = self.nodes[0].getnewaddress()
        dummy_addresses = [self.nodes[1].getnewaddress() for _ in range(60)]

        self.nodes[0].generatetoaddress(amount_of_tested_free_tx*2, addr2)
        self.nodes[0].generatetoaddress(amount_of_tested_free_tx*2, addr1)
        self.sync_all()

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, 'Difference in nodes height'

        ##############################################
        #                                            #
        #            Test only free tx               #
        #                                            #
        ##############################################


        self.generate_inputs_for_free_txs(amount_of_tested_free_tx, addr2)

        self.assert_mempool_leftovers(0)
        # load free txs into mempool
        free_tx_list = []
        for _ in range(amount_of_tested_free_tx):
            free_tx_id = self.send_free_tx(dummy_addresses[:5], free_tx_amount, 0, addr2)
            print(len(free_tx_list))
            print(self.nodes[0].gettransaction(free_tx_id))
            assert free_tx_id is not None
            free_tx_list.append(free_tx_id)

        self.nodes[0].generate(1)
        # Assert test case with only free transactions
        tx_in_block = self.assert_block_tx_preference_only_free(free_tx_list)

        tx_in_block_without_coinbase_tx = tx_in_block -1
        self.assert_mempool_leftovers(amount_of_tested_free_tx - tx_in_block_without_coinbase_tx)

        ##############################################
        #                                            #
        #           Test free and paid tx            #
        #                                            #
        ##############################################

        self.generate_inputs_for_free_txs(amount_of_tested_free_tx, addr2)

        self.assert_mempool_leftovers(0)
        # load free txs into mempool
        free_tx_list = []
        for _ in range(amount_of_tested_free_tx):
            free_tx_id = self.send_free_tx(dummy_addresses[:5], free_tx_amount, 0, addr2)
            assert free_tx_id is not None
            print(self.nodes[0].gettransaction(free_tx_id))
            free_tx_list.append(free_tx_id)

        for i in range(amount_of_tested_free_tx):
            print("Paid ", i)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            sleep(1)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            sleep(1)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            sleep(1)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value) ## TODO ancestors issue
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value) ## TODO fix on main branch the eeror with resetix free tx size
            self.nodes[0].sendtoaddress(addr2, self.free_tx_base_value)
            #self.nodes[1].sendtoaddress(dummy_addresses[4], self.free_tx_base_value)

        self.nodes[0].generate(1)
        # Assert test case with free and paid transactions
        tx_in_block = self.assert_block_tx_preference_only_free(free_tx_list, 20000)

        tx_in_block_without_coinbase_tx = tx_in_block -1
        self.assert_mempool_leftovers(amount_of_tested_free_tx*32 + amount_of_tested_free_tx - tx_in_block_without_coinbase_tx)


if __name__ == '__main__':
    FreeTxPreferenceTest().main()