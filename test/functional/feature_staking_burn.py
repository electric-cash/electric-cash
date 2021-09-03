import decimal

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
        self.staking_burn_reorg_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    @staticmethod
    def create_staking_burn_tx_input(change_address: str, amount_to_burn: decimal.Decimal, utxo: dict):
        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }
        # tx outputs
        amount_to_burn_bin = int(amount_to_burn).to_bytes(8, "little")
        tx_output_staking_burn_header = {
            "data": (bytes([STAKING_TX_HEADER, STAKING_TX_BURN_SUBHEADER]) + amount_to_burn_bin).hex()
        }
        change_amount = (utxo["amount"] * COIN - amount_to_burn - fee) / COIN
        if change_amount > 0.0001:
            tx_output_change = {
                change_address: change_amount
            }
            return [tx_input], [tx_output_staking_burn_header, tx_output_change]
        else:
            return [tx_input], [tx_output_staking_burn_header]

    def send_staking_burn_tx(self, change_address: str, amount_to_burn: decimal.Decimal, node_num: int):

        i = 0
        while True:
            unspent = self.nodes[node_num].listunspent()[i]
            if unspent["amount"] * COIN >= amount_to_burn:
                break
            i = i + 1
        tx_inputs, tx_outputs = self.create_staking_burn_tx_input(change_address, amount_to_burn, unspent)
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[node_num].createrawtransaction(tx_inputs, tx_outputs)
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        self.nodes[0].sendrawtransaction(signed_raw_tx['hex'])

    def staking_burn_simple_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        amount_to_burn = 400 * COIN

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

        # send staking burn transaction
        self.send_staking_burn_tx(addr1, amount_to_burn, node_num=0)

        # check if staking pool balance wasn't increased yet
        assert self.get_staking_pool_balance(
            node_num=0) == node0_staking_balance, f"Staking pool balance increased before a block has been mined"

        # mine a block and check if staking pool balance increased by a proper value
        self.nodes[0].generate(1)
        self.sync_all()
        node0_expected_staking_pool_balance = node0_staking_balance + amount_to_burn + staking_reward
        assert self.get_staking_pool_balance(node_num=0) == self.get_staking_pool_balance(node_num=1) == \
               node0_expected_staking_pool_balance, f"Node 0 staking pool balance " \
                                                    f"({self.get_staking_pool_balance(node_num=0)}) " \
                                                    f"did not increase to expected value: " \
                                                    f"{node0_expected_staking_pool_balance}"

    def staking_burn_reorg_test(self):
        staking_reward = 50 * COIN
        amount_to_burn = 300 * COIN

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, f'Starting nodes height different than ' \
                                                                f'{node0_height, node1_height}'

        starting_height = node0_height
        # assure that staking balance is the same on both nodes

        node0_staking_balance = self.get_staking_pool_balance(node_num=0)
        node1_staking_balance = self.get_staking_pool_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance, \
            f'Starting staking balance difference between nodes {node0_staking_balance, node1_staking_balance}'

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(10, addr1)
        self.sync_all()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == (starting_height + 10), 'Difference in nodes height'

        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        node0_staking_balance = self.get_staking_pool_balance(node_num=0)

        # send staking burn transaction at node 0
        self.send_staking_burn_tx(addr1, amount_to_burn, node_num=0)

        # check if staking pool balance hasn't increased yet
        assert self.get_staking_pool_balance(
            node_num=0) == node0_staking_balance, f"Staking pool balance increased before a block has been mined"

        # mine some blocks, more on node 1 than 0
        self.nodes[0].generate(1)
        self.nodes[1].generate(2)

        # check if staking pool balance on node 0 has increased by a burned amount
        node0_expected_staking_pool_balance = node0_staking_balance + amount_to_burn + 1 * staking_reward
        assert self.get_staking_pool_balance(
            node_num=0) == node0_expected_staking_pool_balance, f"Node 0 staking pool balance " \
                                                                f"({self.get_staking_pool_balance(node_num=0)}) " \
                                                                f"did not increase to expected value: " \
                                                                f"{node0_expected_staking_pool_balance}"

        node1_expected_staking_pool_balance = node0_staking_balance + 2 * staking_reward
        # check if staking pool balance on node 1 hasn't increased by a burned amount
        assert self.get_staking_pool_balance(
            node_num=1) == node1_expected_staking_pool_balance, f"Node 1 staking pool balance " \
                                                                f"({self.get_staking_pool_balance(node_num=1)}) " \
                                                                f"did not increase to expected value: " \
                                                                f"{node1_expected_staking_pool_balance}"


        # connect nodes
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)

        # check if after reorg pool balance on node 0 hasn't increased by a burned amount
        assert self.get_staking_pool_balance(node_num=0) == self.get_staking_pool_balance(
            node_num=1) == node1_expected_staking_pool_balance, f"Staking pool balances do not match: " \
                                                                f"node 0: {self.get_staking_pool_balance(node_num=0)}, " \
                                                                f"node 1: {self.get_staking_pool_balance(node_num=1)}, " \
                                                                f"expected: {node1_expected_staking_pool_balance} "

        # mine one more block and see if the burn is applied again.
        self.nodes[0].generate(1)
        self.sync_all()
        expected_staking_pool_balance = node1_expected_staking_pool_balance + amount_to_burn + 1 * staking_reward
        assert self.get_staking_pool_balance(node_num=0) == self.get_staking_pool_balance(
            node_num=1) == expected_staking_pool_balance, f"Staking pool balances do not match: " \
                                                                f"node 0: {self.get_staking_pool_balance(node_num=0)}, " \
                                                                f"node 1: {self.get_staking_pool_balance(node_num=1)}, " \
                                                                f"expected: {node1_expected_staking_pool_balance} "



if __name__ == '__main__':
    StakingBurnTest().main()
