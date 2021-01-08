#!/usr/bin/env python3
# Copyright (c) 2019 Daniel Kraft
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Tests what happens if we submit a valid block with invalid auxpow.  Obviously
# the block should not be accepted, but it should also not be marked as
# permanently invalid.  So resubmitting the same block with a valid auxpow
# should then work fine.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.blocktools import (
  create_block,
  create_coinbase,
)
from test_framework.messages import (
  CAuxPow,
  CBlock,
  CInv,
  msg_getdata,
  uint256_from_compact,
)
from test_framework.mininode import P2PDataStore, P2PInterface
from test_framework.util import (
  assert_equal,
  hex_str_to_bytes,
  wait_until
)

from test_framework.auxpow_testing import computeAuxpow

import codecs
from io import BytesIO

class P2PBlockGetter (P2PInterface):
  """
  P2P connection class that allows requesting blocks from a node's P2P
  connection to verify how they are sent.
  """

  def on_block (self, msg):
    self.block = msg.block

  def getBlock (self, blkHash):
    self.block = None
    inv = CInv (t=2, h=int (blkHash, 16))
    self.send_message (msg_getdata (inv=[inv]))
    wait_until (lambda: self.block is not None)
    return self.block

class AuxpowInvalidPoWTest (BitcoinTestFramework):

  def set_test_params (self):
    self.num_nodes = 1
    self.setup_clean_chain = True
    self.extra_args = [["-whitelist=127.0.0.1"]]

  def run_test (self):
    node = self.nodes[0]
    node.add_p2p_connection (P2PDataStore ())
    p2pGetter = node.add_p2p_connection (P2PBlockGetter ())

    self.log.info ("Submitting block with invalid auxpow...")
    tip = node.getbestblockhash ()
    blk, blkHash = self.createBlock ()
    blk = self.addAuxpow (blk, blkHash, False)
    assert_equal (node.submitblock (blk.serialize ().hex ()), "high-hash")
    assert_equal (node.getbestblockhash (), tip)

    self.log.info ("Submitting block with valid auxpow...")
    blk = self.addAuxpow (blk, blkHash, True)
    assert_equal (node.submitblock (blk.serialize ().hex ()), None)
    assert_equal (node.getbestblockhash (), blkHash)
    
    self.log.info ("Retrieving block through RPC...")
    gotHex = node.getblock(blkHash, 0)
    gotBlk = CBlock ()
    gotBlk.deserialize (BytesIO (hex_str_to_bytes (gotHex)))
    gotBlk.rehash()
    assert_equal ("%064x" % int(gotBlk.hash, 16), blkHash)
    
    self.log.info ("Retrieving block through P2P...")
    gotBlk = p2pGetter.getBlock (blkHash)
    gotBlk.rehash()
    assert_equal ("%064x" % int(gotBlk.hash, 16), blkHash)

    self.log.info ("Sending block with invalid auxpow over P2P...")
    tip = node.getbestblockhash ()
    blk, blkHash = self.createBlock ()
    blk = self.addAuxpow (blk, blkHash, False)
    node.p2p.send_blocks_and_test ([blk], node, force_send=True,
                                   success=False, reject_reason="high-hash")
    assert_equal (node.getbestblockhash (), tip)

    self.log.info ("Sending the same block with valid auxpow...")
    blk = self.addAuxpow (blk, blkHash, True)
    node.p2p.send_blocks_and_test ([blk], node, success=True)
    assert_equal (node.getbestblockhash (), blkHash)
    
    self.log.info ("Retrieving block through RPC...")
    gotHex = node.getblock(blkHash, 0)
    gotBlk = CBlock ()
    gotBlk.deserialize (BytesIO (hex_str_to_bytes (gotHex)))
    gotBlk.rehash()
    assert_equal ("%064x" % int(gotBlk.hash, 16), blkHash)
    
    self.log.info ("Retrieving block through P2P...")
    gotBlk = p2pGetter.getBlock (blkHash)
    gotBlk.rehash()
    assert_equal ("%064x" % int(gotBlk.hash, 16), blkHash)

  def createBlock (self):
    """
    Creates a new block that is valid for the current tip.  It is marked as
    auxpow, but the auxpow is not yet filled in.
    """

    bestHash = self.nodes[0].getbestblockhash ()
    bestBlock = self.nodes[0].getblock (bestHash)
    tip = int (bestHash, 16)
    height = bestBlock["height"] + 1
    time = bestBlock["time"] + 1

    block = create_block (tip, create_coinbase (height), time)
    block.set_base_version(4)
    block.mark_auxpow ()
    block.rehash ()
    newHash = "%064x" % block.sha256

    return block, newHash


  def addAuxpow (self, block, blkHash, ok):
    """
    Fills in the auxpow for the given block message.  It is either
    chosen to be valid (ok = True) or invalid (ok = False).
    """

    target = b"%064x" % uint256_from_compact (block.nBits)
    auxpowHex = computeAuxpow (blkHash, target, ok)
    block.auxpow = CAuxPow ()
    block.auxpow.deserialize (BytesIO (hex_str_to_bytes (auxpowHex)))

    return block

if __name__ == '__main__':
  AuxpowInvalidPoWTest ().main ()
