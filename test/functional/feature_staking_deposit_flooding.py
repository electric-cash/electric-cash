import decimal

from test_framework.messages import COIN
from test_framework.staking_utils import DepositStakingTransactionsMixin, STAKING_PENALTY_PERCENTAGE
from test_framework.test_framework import BitcoinTestFramework

class StakingDepositWithdrawalTest(BitcoinTestFramework, DepositStakingTransactionsMixin):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.flooding_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
            return self.nodes[node_num].getstakinginfo()['staking_pool']

    def send_staking_deposit_tx(self, stake_address: str, deposit_value: decimal.Decimal, node_num: int,
                                change_address: str = None):
        txid = self.nodes[node_num].depositstake(deposit_value, 4320, stake_address)
        return txid

    def assure_same_height(self, height):
        last_node_height = self.nodes[0].getblockcount()
        for index, node in enumerate(self.nodes):
            node_height = node.getblockcount()
            assert node_height == last_node_height == height, f'Difference in nodes height at node index: {index}'
            last_node_height = node_height

    def assure_same_staking_balance(self):
        # assure that staking balance is the same on both nodes

        last_balance = self.get_staking_pool_balance(node_num=0)
        for index, _ in enumerate(self.nodes):
            balance = self.get_staking_pool_balance(node_num=index)
            assert last_balance == balance, f'Difference in nodes balance at index: {index}'
            last_balance = balance

    def flooding_test(self):
        starting_height = 200
        deposit_value = 6
        flood_amount = 1500

        self.assure_same_height(starting_height)

        # generate 10 blocks on node 0, then sync nodes and check height and staking balance
        generated_blocks = 1000
        addr1 = self.nodes[0].getnewaddress()
        self.nodes[0].generatetoaddress(generated_blocks, addr1)

        self.sync_all()
        self.assure_same_staking_balance()
        self.assure_same_height(starting_height+generated_blocks)

        # flood with stakes 
        for _ in range(flood_amount):
            self.send_staking_deposit_tx(addr1, deposit_value, node_num=0)

        # mine some blocks
        self.nodes[0].generate(3)
        self.sync_all()

if __name__ == '__main__':
    StakingDepositWithdrawalTest().main()
