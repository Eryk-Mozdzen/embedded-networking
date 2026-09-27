# embedded-networking

### PPP

```
sudo pppd /dev/ttyUSB0 921600 192.168.7.1:192.168.7.2 local noauth debug nodetach nocrtscts
```

``` iperf
iperf -c 192.168.7.2 -e -i 1 -M 5000 -l 8192
```
