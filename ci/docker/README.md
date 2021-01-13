Elcash
============================

### ENV's

| name | default value | notes |
|------|-------|-------|
| `BVAULTD_RPC_PASSWORD` | `xxxx` | RPC password for bvaultd 
| `BVAULTD_RPC_USER` | `root` | RPC username for bvaultd 
| `BVAULTD_RPC_ALLOW_IP_FIRST` | `127.0.0.0/24` | `rpcallowip` parameter first iteration 
| `BVAULTD_RPC_ALLOW_IP_SECOND` | `10.0.0.0/8` | `rpcallowip` parameter second iteration 
| `BVAULTD_NET_BAN_SCORE` | `100000` | `banscore` parameter, to disable, set to empty string 
| `BVAULTD_NET_BAN_TIME` | `60` | `bantime` parameter, to disable, set to empty string 
| `BVAULTD_TESTNET_ENABLED` | undefined | Allowed: `1`, so set if you want to run in testnet
| `BVAULTD_SERVER` | `1` | Allowed: `1` `server` parameter
| `BVAULTD_TXINDEX` | undefined | Allowed: `1` `txindex` parameter


For more information how config is generated see template: `templates/elcashd.conf.tmpl`


### BlockChain volume

> Important 

Blockchain data is stored in volume defined for path `/elcash/blockchain`. So when you run this image, make sur to bind proper volume 
Depending on `ELCASHD_NET` env
