from test_framework.staking_utils import FreeTransactionMixin
from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes,
    disconnect_nodes,
    assert_raises_rpc_error,
)

STAKING_TX_HEADER = 0x53
STAKING_TX_BURN_SUBHEADER = 0x42
STAKING_TX_DEPOSIT_SUBHEADER = 0x44
STAKING_PENALTY_PERCENTAGE = 3.0


class StakingReorgTest(BitcoinTestFramework, FreeTransactionMixin):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [
            ["-txindex"],
            ["-txindex"]
        ]

    def run_test(self):
        self.reorg_recalculate_free_tx_info()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def get_free_tx_info(self, address: str, node_num: int):
        return self.nodes[node_num].getfreetxinfo(address)

    def send_staking_deposit_tx(self, stake_address: str, deposit_value: float, node_num: int):
        txid = self.nodes[node_num].depositstake(deposit_value, 4320, stake_address)
        return txid

    def reorg_recalculate_free_tx_info(self):
        deposit_value = 6
        deposit_amount = deposit_value * COIN

        addr1 = self.nodes[0].getnewaddress()
        addr2 = self.nodes[0].getnewaddress()
        # assure that nodes start with the same height
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height, f'Starting nodes height difference: {node0_height, node1_height}'

        ### SCENARIO 1 - branch without free transaction is best
        stake_txid = self.send_staking_deposit_tx(addr1, deposit_value, node_num=0)
        self.nodes[0].sendtoaddress(addr1, 10)
        # generate 15 blocks on node 0, then sync nodes and check height and staking balance
        self.nodes[0].generate(15)
        self.sync_all()
        # disconnect nodes
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.nodes[0].generate(9)
        self.nodes[1].generate(15)

        self.send_free_tx([addr2], 8 * COIN, 0, addr1)
        self.nodes[0].generate(1)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node1_height - node0_height == 5, 'Wrong diff in nodes height'
        node0_free_tx_info = self.get_free_tx_info(addr1, node_num=0)
        node1_free_tx_info = self.get_free_tx_info(addr1, node_num=1)

        assert node0_free_tx_info["limit"] == node1_free_tx_info[
            "limit"], f"difference in calculated free tx limit: {node0_free_tx_info}, {node1_free_tx_info}"
        assert node0_free_tx_info[
                   "used_blockchain_limit"] > 0, f"Wrong used blockchain limit at node 0: {node0_free_tx_info}"
        assert node1_free_tx_info[
                   "used_blockchain_limit"] == 0, f"Wrong used blockchain limit at node 1: {node1_free_tx_info}"

        # reconnecting nodes
        self.log.info('Reconnect nodes')
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_blocks()
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == target_height, f'Nodes heights different than {target_height}'

        node0_free_tx_info = self.get_free_tx_info(addr1, node_num=0)
        node1_free_tx_info = self.get_free_tx_info(addr1, node_num=1)
        for key in node0_free_tx_info:
            assert node0_free_tx_info[key] == node1_free_tx_info[
                key], f'Difference in nodes free tx limits at {key} :({node0_free_tx_info, node1_free_tx_info})'

        self.nodes[0].generate(1)
        self.sync_blocks()
        node0_free_tx_info = self.get_free_tx_info(addr1, node_num=0)
        node1_free_tx_info = self.get_free_tx_info(addr1, node_num=1)
        assert node1_free_tx_info["used_blockchain_limit"] > 0, f"Wrong used blockchain limit at node 1: {node1_free_tx_info}"
        for key in node0_free_tx_info:
            assert node0_free_tx_info[key] == node1_free_tx_info[
                key], f'Difference in nodes free tx limits at {key} :({node0_free_tx_info, node1_free_tx_info})'

        ### SCENARIO 2 - branch with free transaction is best
        self.nodes[0].sendtoaddress(addr1, 10)
        self.nodes[0].generate(144)
        self.sync_blocks()

        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.nodes[0].generate(14)
        self.nodes[1].generate(10)
        self.send_free_tx([addr2], 8 * COIN, 0, addr1)
        self.nodes[0].generate(1)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node0_height - node1_height == 5, 'Wrong diff in nodes height'
        node0_free_tx_info = self.get_free_tx_info(addr1, node_num=0)
        node1_free_tx_info = self.get_free_tx_info(addr1, node_num=1)

        assert node0_free_tx_info["limit"] == node1_free_tx_info[
            "limit"], f"difference in calculated free tx limit: {node0_free_tx_info}, {node1_free_tx_info}"
        assert node0_free_tx_info[
               "used_blockchain_limit"] > 0, f"Wrong used blockchain limit at node 0: {node0_free_tx_info}"
        assert node1_free_tx_info[
               "used_blockchain_limit"] == 0, f"Wrong used blockchain limit at node 1: {node1_free_tx_info}"

        # reconnecting nodes
        self.log.info('Reconnect nodes')
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_blocks()
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == target_height, f'Nodes heights different than {target_height}'

        node0_free_tx_info = self.get_free_tx_info(addr1, node_num=0)
        node1_free_tx_info = self.get_free_tx_info(addr1, node_num=1)
        assert node0_free_tx_info["used_blockchain_limit"] > 0, f"Wrong used blockchain limit at node 1: {node1_free_tx_info}"
        for key in node0_free_tx_info:
            assert node0_free_tx_info[key] == node1_free_tx_info[
                key], f'Difference in nodes free tx limits at {key} :({node0_free_tx_info, node1_free_tx_info})'


        ### SCENARIO 3 - fork during the ongoing window, one branch has one more free transaction, window ends before the tip
        self.nodes[0].sendtoaddress(addr1, 10)
        self.nodes[0].generate(144)
        self.send_free_tx([addr2], 8 * COIN, 0, addr1)
        self.nodes[0].generate(130)
        self.sync_blocks()

        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)
        self.nodes[0].sendtoaddress(addr1, 10)
        self.nodes[0].generate(1)
        ftx2_id = self.send_free_tx([addr2], 8 * COIN, 0, addr1)
        self.nodes[1].generate(15)
        self.nodes[0].generate(19)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node0_height - node1_height == 5, 'Wrong diff in nodes height'
        free_tx = self.nodes[0].getrawtransaction(ftx2_id, True)
        assert free_tx["confirmations"] > 0

        # reconnecting nodes
        self.log.info('Reconnect nodes')
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_blocks()
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == target_height, f'Nodes heights different than {target_height}'

        node0_free_tx_info = self.get_free_tx_info(addr1, node_num=0)
        node1_free_tx_info = self.get_free_tx_info(addr1, node_num=1)
        free_tx = self.nodes[1].getrawtransaction(ftx2_id, True)
        assert free_tx["confirmations"] > 0
        for key in node0_free_tx_info:
            assert node0_free_tx_info[key] == node1_free_tx_info[
                key], f'Difference in nodes free tx limits at {key} :({node0_free_tx_info, node1_free_tx_info})'


if __name__ == '__main__':
    StakingReorgTest().main()
