import decimal

from test_framework.messages import COIN
from test_framework.util import assert_raises_rpc_error


STAKING_TX_HEADER = 0x53
STAKING_TX_BURN_SUBHEADER = 0x42
STAKING_TX_DEPOSIT_SUBHEADER = 0x44
STAKING_PENALTY_PERCENTAGE = 3.0


class SpendStakeMixin:
    def spend_stake(self, node_num: int, stake_txid: str, stake_address: str, early_withdrawal: bool = False,
                    expected_reward: int = 0, expected_rpc_err: int = None):
        unspent = [u for u in self.nodes[node_num].listunspent() if
                   u["txid"] == stake_txid and u["address"] == stake_address]
        assert len(unspent) == 1
        utxo = unspent[0]
        staking_penalty = int(
            utxo["amount"] * COIN * decimal.Decimal(STAKING_PENALTY_PERCENTAGE / 100.0)) if early_withdrawal else 0
        reward = expected_reward if not early_withdrawal else 0
        fee = COIN // 1000
        tx_input = {
            "txid": utxo["txid"],
            "vout": utxo["vout"]
        }
        tx_output = {
            stake_address: (utxo["amount"] * COIN - fee - staking_penalty + reward) / COIN
        }
        raw_tx = self.nodes[node_num].createrawtransaction([tx_input], [tx_output])
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        if expected_rpc_err is not None:
            assert_raises_rpc_error(expected_rpc_err, None,
                                    self.nodes[0].sendrawtransaction, signed_raw_tx["hex"])
        else:
            return self.nodes[node_num].sendrawtransaction(signed_raw_tx["hex"])


class DepositStakingTransactionsMixin(SpendStakeMixin):
    @staticmethod
    def create_tx_inputs_and_outputs(stake_address: str, change_address: str, deposit_amount: decimal.Decimal,
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
        i = 0
        while True:
            unspent = self.nodes[node_num].listunspent()[i]
            if unspent["amount"] * COIN >= deposit_amount:
                break
            i = i + 1
        if not change_address:
            change_address = self.nodes[node_num].getnewaddress()
        tx_inputs, tx_outputs = self.create_tx_inputs_and_outputs(stake_address, change_address, deposit_amount,
                                                                  unspent)
        # create, sign and send staking burn transaction
        raw_tx = self.nodes[node_num].createrawtransaction(tx_inputs, tx_outputs)
        signed_raw_tx = self.nodes[node_num].signrawtransactionwithwallet(raw_tx)
        txid = self.nodes[node_num].sendrawtransaction(signed_raw_tx['hex'])
        return txid


class BurnStakingTransactionsMixin(SpendStakeMixin):
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
        return self.nodes[node_num].sendrawtransaction(signed_raw_tx['hex'])
