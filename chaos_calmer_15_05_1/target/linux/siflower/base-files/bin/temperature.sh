#!/bin/ash
echo "~~~~~~~~~~start recording~~~~~~~~~~~" > temperature.txt
timer=1
interval=3
cooling_temp=`cat sys/kernel/debug/aetnensis/cooling_temp`
echo "cooling=$cooling_temp" >> temperature.txt
while [ $timer -gt 0 ]
do
	temp=`cat sys/kernel/debug/aetnensis/temperature`
	echo "$temp" >> temperature.txt
	cat /sys/kernel/debug/ieee80211/phy1/rwnx/txpower >> txpower.txt
	sleep 3
done
