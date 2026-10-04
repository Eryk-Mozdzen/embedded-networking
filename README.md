# embedded-networking

### PPP

```
sudo pppd /dev/ttyUSB0 921600 192.168.7.1:192.168.7.2 local noauth debug nodetach nocrtscts
```

```
sudo ip route add 192.168.11.0/24 via 192.168.0.129
```

``` iperf
iperf -c 192.168.7.2 -i 1
iperf -s -i 1
```
