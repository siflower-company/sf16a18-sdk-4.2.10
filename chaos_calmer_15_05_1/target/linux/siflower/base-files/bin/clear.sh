#!/bin/sh
./etc/init.d/subservice stop
./etc/init.d/syncservice stop
./etc/init.d/uhttpd stop
rmmod sf_ts
ifconfig eth0 down
devmem 0x19e04040 32 0
