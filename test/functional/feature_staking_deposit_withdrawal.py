import decimal

from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
)

STAKING_TX_HEADER = 0x53
STAKING_TX_BURN_SUBHEADER = 0x42
STAKING_TX_DEPOSIT_SUBHEADER = 0x44


class StakingBurnTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        self.early_withdrawal_test()

    def get_staking_pool_balance(self, node_num: int) -> int:
        return self.nodes[node_num].getstakinginfo()['staking_pool']

    @staticmethod
    def create_staking_burn_tx_input(stake_address: str, change_address: str, deposit_amount: decimal.Decimal,
                                     utxo: dict):
        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }
        # tx outputs
        tx_output_staking_deposit_header = {
            "data": bytes([STAKING_TX_HEADER, STAKING_TX_DEPOSIT_SUBHEADER, 0x01, 0x00]).hex()
        }

        tx_output_stake = {
            stake_address: deposit_amount / COIN
        }

        change_amount = (utxo["amount"] * COIN - deposit_amount - fee) / COIN
        if change_amount > 0.0001:
            tx_output_change = {
                change_address: change_amount
            }
            return [tx_input], [tx_output_staking_deposit_header, tx_output_stake, tx_output_change]
        else:
            return [tx_input], [tx_output_staking_deposit_header, tx_output_stake]

    def send_staking_deposit_tx(self, stake_address: str, deposit_amount: decimal.Decimal, node_num: int,
                                change_address: str = None):
        unspent = self.nodes[node_num].listunspent()[0]
        assert unspent[
                   "amount"] * COIN > deposit_amount, f"Amount too big ({deposit_amount}) to be sent from this UTXO " \
                                                      f"({unspent['amount'] * COIN})"
        if not change_address:
            change_address = self.nodes[node_num].getnewaddress()
        tx_inputs, tx_outputs = self.create_staking_burn_tx_input(stake_address, change_address, deposit_amount,
                                                                  unspent)
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[node_num].createrawtransaction(tx_inputs, tx_outputs)
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        txid = self.nodes[0].sendrawtransaction(signed_raw_tx['hex'])
        return txid

    def early_withdrawal_test(self):
        starting_height = 200
        staking_reward = 50 * COIN
        starting_staking_balance = 1 * staking_reward
        deposit_amount = 400 * COIN

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

        # send staking deposit transaction
        stake_id = self.send_staking_deposit_tx(addr1, deposit_amount, node_num=0)
        # mine some blocks
        self.nodes[0].generate(3)
        self.sync_all()

        # try to spend the stake

        unspent = [u for u in self.nodes[0].listunspent() if u["txid"] == stake_id and u["address"] == addr1]
        assert len(unspent) == 1
        utxo = unspent[0]

        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }

        tx_output = {
            addr1: (utxo["amount"] * COIN - fee) / COIN
        }
        raw_tx = self.nodes[0].createrawtransaction([tx_input], [tx_output])
        signed_raw_tx = self.nodes[0].signrawtransactionwithwallet(raw_tx)

        # verify that the stake cannot be spent without paying the penalty
        assert_raises_rpc_error(-26, "bad-txns-in-belowout, value in (388.00) < value out (399.999)",
                                self.nodes[0].sendrawtransaction, signed_raw_tx["hex"])

        # try to spend the stake paying the staking penalty
        tx_output = {
            addr1: (utxo["amount"] * COIN - fee - int(deposit_amount * 0.03)) / COIN
        }
        raw_tx = self.nodes[0].createrawtransaction([tx_input], [tx_output])
        signed_raw_tx = self.nodes[0].signrawtransactionwithwallet(raw_tx)
        txid = self.nodes[0].sendrawtransaction(signed_raw_tx["hex"])
        assert txid is not None


if __name__ == '__main__':
    StakingBurnTest().main()
