Elcash
============================

### ENV's

| name | default value | notes |
|------|-------|-------|
| `ELCASHD_RPC_PASSWORD` | `xxxx` | RPC password for bvaultd 
| `ELCASHD_RPC_USER` | `root` | RPC username for bvaultd 
| `ELCASHD_RPC_ALLOW_IP_FIRST` | `127.0.0.0/24` | `rpcallowip` parameter first iteration 
| `ELCASHD_RPC_ALLOW_IP_SECOND` | `10.0.0.0/8` | `rpcallowip` parameter second iteration 
| `ELCASHD_NET_BAN_SCORE` | `100000` | `banscore` parameter, to disable, set to empty string 
| `ELCASHD_NET_BAN_TIME` | `60` | `bantime` parameter, to disable, set to empty string 
| `ELCASHD_TESTNET_ENABLED` | undefined | Allowed: `1`, so set if you want to run in testnet
| `ELCASHD_REGTEST_ENABLED` | undefined | Allowed: `1`, set if you want to run regtest
| `ELCASHD_SERVER` | `1` | Allowed: `1` `server` parameter
| `ELCASHD_TXINDEX` | undefined | Allowed: `1` `txindex` parameter


For more information how config is generated see template: `templates/elcashd.conf.tmpl`


### BlockChain volume

> Important 

Blockchain data is stored in volume defined for path `/elcash/blockchain`. So when you run this image, make sur to bind proper volume 
Depending on `ELCASHD_NET` env
