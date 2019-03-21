#!/bin/ash
# Copyright (C) 2006 OpenWrt.org


firmware_remove(){
	#stop wifi first
	#/sbin/wifi down
	sleep 1
	#unregist umac
	rmmod sf16a18_lb_smac
	rmmod sf16a18_hb_smac
	rmmod sf16a18_rf
	sleep 1
	rmmod startcore
}

firmware_reload(){	
	rf_misc.sh $2
}

echo "---------------------clear dmesg---------------------"
dmesg -c > log2
sfwifi remove > log2
echo "---------------------test1 start----------------------"
echo "---------------------key log--------------------------" > log1
if [ -z "$1" ]
then 
	timer=1
else
	timer=$1
fi

while [ $timer -gt 0 ];
do
	timer=$(($timer - 1))
	firmware_reload
	sleep 1
	firmware_remove
	dmesg | grep -n "the app status is" >> log1
	dmesg -c > log2
	grep -q "failed" log2
	if [ "$?" -eq "0" ]
	then
		echo "timer:$timer" >> log1		
	fi
	sleep 1
	echo "-------------timer is $timer -----------------------------"
done
#dmesg | grep "the app status is" > log1
grep -q "failed" log1
if [ "$?" -eq "0" ]
then
	echo "----------------- test fail! (cat log1)----------------"
else
	echo "----------------- test success -----------------"
fi
