# sockperf_cli

DPDK based [sockperf](https://github.com/Mellanox/sockperf) client implementation
should work on linux/windows (yes windows :grin:, see [releases](https://github.com/serge-klim/dpdk-tools/releases)) and could be run against original sockperf in server mode(sr) as well as testpmd.

original sockperf:

```
(win)sockperf sr -i 192.168.1.2
sockperf_cli 192.168.1.2:11111 --detailed-stats
```

expect missing packets in bandwidth tests, [sockperf](https://github.com/Mellanox/sockperf) won't able to handle all generated traffic

testpmd:

```
dpdk-testpmd -a 0000:01:00.1 -n 2 -l 6,8 -- --forward-mode=5tswap
sockperf_cli --destination-mac=EC:0D:9A:XX:XX:XX 192.168.1.2:11111 --eal.a=0000:01:00.0
```

eal specific parameter can be passed via: `eal.` prefix.
most of part of [rte_eth_dev_info](https://doc.dpdk.org/api/structrte__eth__dev__info.html) can be configured via [config file](configs/sockperf-mx.config), and used like so:
```
sockperf_cli -c configs/sockperf-mx.config
```

per nic:
```
[nic.EC:0D:9A:XX:XX:XX] # mac address should match nic address
info.default_rxportconf.burst_size=1
info.default_txportconf.burst_size=1
```
or nic driver:
```
[mlx5_pci]
info.default_rxportconf.burst_size=1
info.default_txportconf.burst_size=1
```

rx/tx offloads can be configured by setting:
```
 effective_offload.rx
 effective_offload.tx
```
accordingly (preferred way). The other way it to set offloads in info(rte_eth_dev_info) parameter see above.

to confirm that parameter took effect - `-v` parameter can be used.

