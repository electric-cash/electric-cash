from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes,
    disconnect_nodes,
)


class StakingPoolTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.reorg_test()
    
    def get_staking_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    def reorg_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward

        # assure that nodes start with the same height
        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == starting_height, f'Starting nodes height different than {starting_height}'

        # assure that staking balance is the same on both nodes
        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == starting_staking_balance, f'Starting staking balance different than {starting_staking_balance}'
        
        # generate 15 blocks on node 0, then sync nodes and check height and staking balance
        self.nodes[0].generate(15)
        self.sync_all()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == (starting_height + 15), 'Difference in nodes height'

        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == (starting_staking_balance + 15 * staking_reward), 'Difference in nodes staking pool balance'

        # disconnect nodes 
        self.log.info('Disconnect nodes')
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 2)

        self.nodes[0].generate(11)
        self.nodes[1].generate(19)

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        # height of nodes after nodes reconnecting
        target_height = max(node0_height, node1_height)
        assert node1_height - node0_height == 8, 'Wrong diff in nodes height'
        
        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        # staking pool balance after nodes reconnecting
        target_staking_balance = max(node0_staking_balance, node1_staking_balance)
        assert node0_staking_balance < node1_staking_balance, 'Staking pool balance wrong inequality'

        # reconnecting nodes
        self.log.info('Reconnect nodes')
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        self.sync_all()

        node0_height = self.nodes[0].getblockcount()
        node1_height = self.nodes[1].getblockcount()
        assert node0_height == node1_height == target_height, f'Nodes heights different than {target_height}'

        node0_staking_balance = self.get_staking_balance(node_num=0)
        node1_staking_balance = self.get_staking_balance(node_num=1)
        assert node0_staking_balance == node1_staking_balance == target_staking_balance, f'Nodes staking balance different than {target_staking_balance}'
        assert target_staking_balance == 35 * staking_reward, 'Wrong staking balance'


if __name__ == '__main__':
    StakingPoolTest().main()
