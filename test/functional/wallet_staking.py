"""Test the staking API."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
)


def check_amount_equals_excluding_fee(amount, amount_with_fee):
    return amount_with_fee - 0.1 < amount < amount_with_fee


class StakingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        wallet_value = 2000
        stake1_value = 1900
        stake2_value = 50
        self.nodes[1].unloadwallet("")
        self.nodes[1].createwallet("w1")
        node1_addr = self.nodes[1].getnewaddress()
        self.nodes[1].unloadwallet("w1")
        self.nodes[1].createwallet("w_staking")
        node1_staking_addr = self.nodes[1].getnewaddress()
        assert self.nodes[1].getbalance() == 0
        _ = self.nodes[0].sendtoaddress(node1_staking_addr, wallet_value)
        self.nodes[0].generate(1)
        self.sync_all()

        stakeid1 = self.nodes[1].depositstake(stake1_value, 4320, node1_staking_addr)
        stakeid2 = self.nodes[1].depositstake(stake2_value, 4320, node1_staking_addr)
        assert len(stakeid1) > 0
        assert len(stakeid2) > 0
        self.nodes[1].generate(1)

        stake1info = self.nodes[1].getstakeinfo(stakeid1)
        assert stake1info is not None
        assert stake1info["staking_amount"] == stake1_value

        stake2info = self.nodes[1].getstakeinfo(stakeid2)
        assert stake2info is not None
        assert stake2info["staking_amount"] == stake2_value

        stakeinfowallet = self.nodes[1].getstakes()
        assert len(stakeinfowallet) == 2
        stake1infowallet = stakeinfowallet[0]
        assert stake1infowallet["address"] == node1_staking_addr

        plain_balance = self.nodes[1].getbalance()
        balance_with_stakes = self.nodes[1].getbalance("*", 0, False, False, True)

        assert check_amount_equals_excluding_fee(plain_balance, wallet_value - stake1_value - stake2_value), f"{plain_balance}"
        assert check_amount_equals_excluding_fee(balance_with_stakes, wallet_value - 0.03 * (
                stake1_value + stake2_value)), f"{balance_with_stakes}"

        # try spending an incomplete stake implicitly
        assert_raises_rpc_error(-6, "", self.nodes[1].sendtoaddress, node1_addr, 75)

        # try spending an incomplete stake explicitly
        txid = self.nodes[1].withdrawstake(stakeid2, node1_addr)
        assert txid is not None
        self.nodes[1].unloadwallet("w_staking")
        self.nodes[1].loadwallet("w1")
        self.nodes[1].generate(1)
        assert check_amount_equals_excluding_fee(self.nodes[1].getbalance(), stake2_value * 0.97), f"{self.nodes[1].getbalance()}"

        self.nodes[1].unloadwallet("w1")
        self.nodes[1].loadwallet("w_staking")

        # try spending a complete stake
        assert len(self.nodes[1].getstakes()) == 1, self.nodes[1].getstakes()
        self.nodes[1].generate(4320)
        assert self.nodes[1].getstakeinfo(stakeid1)["fulfilled"]
        txid = self.nodes[1].sendtoaddress(node1_addr, 1957)
        assert len(txid) > 0


if __name__ == '__main__':
    StakingTest().main()
