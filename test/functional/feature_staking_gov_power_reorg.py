from test_framework.staking_utils import DepositStakingTransactionsMixin
from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes,
    disconnect_nodes,
)


class StakingGovPowerReorgTest(BitcoinTestFramework, DepositStakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.reorg_recalculate_reward()

    def get_gp(self, address: str, node_num: int):
        return self.nodes[node_num].getgovpower(address)

    def reorg_recalculate_reward(self):
        deposit_amount = 300 * COIN

        addr1 = self.nodes[0].getnewaddress()

        stake_txid = self.send_staking_deposit_tx(addr1, deposit_amount, 0)
        # generate 15 blocks on node 0, then sync nodes and check height and staking balance
        self.nodes[0].generate(15)
        self.sync_all()
        # disconnect nodes
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.nodes[0].generate(10)
        self.nodes[1].generate(15)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node1_height - node0_height == 5, 'Wrong diff in nodes height'

        node0_gp = self.get_gp(addr1, 0)
        node1_gp = self.get_gp(addr1, 1)

        assert node0_gp < node1_gp, f'Invalid GP distribution: f{node0_gp, node1_gp}'

        # reconnecting nodes
        self.log.info('Reconnect nodes')
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_blocks()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == target_height, f'Nodes heights different than {target_height}'

        node0_gp = self.get_gp(addr1, 0)
        node1_gp = self.get_gp(addr1, 1)

        assert node0_gp == node1_gp, f'Invalid GP distribution: f{node0_gp, node1_gp}'


if __name__ == '__main__':
    StakingGovPowerReorgTest().main()