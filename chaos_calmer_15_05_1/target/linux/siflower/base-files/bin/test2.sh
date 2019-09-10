#!/bin/sh

timer=100000000
while [ $timer -gt 0 ]; do
	timer=$(($timer - 1))

	teststr=`ifconfig |grep wlan0`
	if [ "x$teststr" != "x" ]; then
		echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX-----------------1" > /dev/ttyS0
		break
	fi
	sleep 10
done
reboot

echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX-----------------2" > /dev/ttyS0
