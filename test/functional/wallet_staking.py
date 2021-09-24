"""Test the staking API."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
)


class StakingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.nodes[1].unloadwallet("")
        self.nodes[1].createwallet("w1")
        node1_addr = self.nodes[1].getnewaddress()
        assert self.nodes[1].getbalance() == 0
        txid = self.nodes[0].sendtoaddress(node1_addr, 2000)
        self.nodes[0].generate(1)
        self.sync_all()
        stakeid = self.nodes[1].depositstake(1900.0, 4320, node1_addr)
        assert len(stakeid) > 0
        self.nodes[1].generate(1)
        stakeinfo = self.nodes[1].getstakeinfo(stakeid)
        assert stakeinfo is not None
        assert stakeinfo["staking_amount"] == 1900

        # try spending an incomplete stake
        node0_addr = self.nodes[0].getnewaddress()
        assert_raises_rpc_error(-6, "", self.nodes[1].sendtoaddress, node0_addr, 150)
        self.nodes[1].generate(4320)
        txid = self.nodes[1].sendtoaddress(node0_addr, 2007)
        assert len(txid) > 0


if __name__ == '__main__':
    StakingTest().main()
