from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes,
    disconnect_nodes,
)

STAKING_TX_HEADER = 0x53
STAKING_TX_BURN_SUBHEADER = 0x42


class StakingBurnTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.staking_burn_simple_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def staking_burn_simple_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward
        amount_to_burn = 400 * COIN

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than {starting_height, node0_height, node1_height}'

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

        # gather data for staking burn transaction generation
        # tx input (expected to be 450 ELC)
        unspent = self.nodes[0].listunspent()[0]
        tx_input = {
            "txid": unspent["txid"],
            "vout": unspent["vout"]
        }

        # tx outputs
        amount_to_burn_bin = int(amount_to_burn).to_bytes(8, "little")
        tx_output_staking_burn_header = {
            "data": (bytes([STAKING_TX_HEADER, STAKING_TX_BURN_SUBHEADER]) + amount_to_burn_bin).hex()
        }
        tx_output_change = {
            addr1: 49.99
        }
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[0].createrawtransaction([tx_input], [tx_output_staking_burn_header, tx_output_change])
        signed_raw_tx = self.nodes[0].signrawtransactionwithwallet(raw_tx)
        self.nodes[0].sendrawtransaction(signed_raw_tx['hex'])

        # check if staking pool balance wasn't increased yet
        assert self.get_staking_pool_balance(node_num=0) == node0_staking_balance

        # mine a block and check if staking pool balance increased by a proper value
        self.nodes[0].generate(1)
        self.sync_all()
        assert self.get_staking_pool_balance(node_num=0) == self.get_staking_pool_balance(node_num=1) == \
               node0_staking_balance + amount_to_burn + staking_reward

    def staking_burn_reorg_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward
        amount_to_burn = 300 * COIN

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than {starting_height, node0_height, node1_height}'

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == starting_staking_balance, f'Starting staking balance different than {starting_staking_balance}'

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr1)
        self.sync_all()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == (starting_height + 10), 'Difference in nodes height'

        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        # gather data for staking burn transaction generation
        # tx input (expected to be 450 ELC)
        unspent = self.nodes[0].listunspent()[0]
        tx_input = {
            "txid": unspent["txid"],
            "vout": unspent["vout"]
        }

        # tx outputs
        amount_to_burn_bin = int(amount_to_burn).to_bytes(8, "little")
        tx_output_staking_burn_header = {
            "data": (bytes([STAKING_TX_HEADER, STAKING_TX_BURN_SUBHEADER]) + amount_to_burn_bin).hex()
        }
        tx_output_change = {
            addr1: 149.99
        }
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[0].createrawtransaction([tx_input], [tx_output_staking_burn_header, tx_output_change])
        signed_raw_tx = self.nodes[0].signrawtransactionwithwallet(raw_tx)
        self.nodes[0].sendrawtransaction(signed_raw_tx['hex'])

        # check if staking pool balance wasn't increased yet
        assert self.get_staking_pool_balance(node_num=0) == node0_staking_balance

        # mine some blocks, more on node 1 than 0
        self.nodes[0].generate(1)
        self.nodes[1].generate(2)

        # check if staking pool balance on node 0 has increased by a burned amount
        assert self.get_staking_pool_balance(node_num=0) == node0_staking_balance + amount_to_burn + staking_reward

        # check if staking pool balance on node 1 hasn't increased by a burned amount
        assert self.get_staking_pool_balance(node_num=1) == node0_staking_balance + 2 * staking_reward

        # connect nodes
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_all()

        # check if staking pool balance on node 1 hasn't increased by a burned amount
        assert self.get_staking_pool_balance(node_num=0) == self.get_staking_pool_balance(node_num=1) == node0_staking_balance + 2 * staking_reward


if __name__ == '__main__':
    StakingBurnTest().main()
