# SPI-PTY bridge

Tool allow running `pppd` using custom SPI streaming protocol.

## Enable SPI on RaspberryPi 5

```
sudo nano /boot/firmware/config.txt
```

```
[all]
dtparam=spi=on
```

```
sudo reboot
ls -l /dev/spidev*
```

## Send, build and run

```
rsync -r ../pty-spi emozdzen@192.168.100.2:/home/emozdzen/
```

```
mkdir /home/emozdzen/pty-spi/build
cmake ..
make
sudo pty-spi
```
