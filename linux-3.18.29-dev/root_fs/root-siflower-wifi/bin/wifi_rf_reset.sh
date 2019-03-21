#!/bin/sh

sfwifi

times=10000
while [ $times -gt 0 ]; do
	echo "---wifi_rf_reset test time left $times----"
	times=$(($times - 1))
	sfwifi reset
	sleep 1s
done
