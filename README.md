# embedded-networking

### PPP

```
sudo pppd /dev/ttyUSB0 921600 192.168.7.1:192.168.7.2 local noauth debug nodetach nocrtscts
```

``` iperf
iperf -c 192.168.7.2 -i 1
iperf -s -i 1
```
