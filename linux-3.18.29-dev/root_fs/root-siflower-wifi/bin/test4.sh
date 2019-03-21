#!/bin/ash
# Copyright (C) 2006 OpenWrt.org

echo "------------------------clear dmesg---------------------------"
dmesg -c > log3

ID=$(ps | grep "rf_hb_test" | grep -v "$0" | grep -v "grep" | awk '{print $1}')
if [ -n "$ID" ];then
	kill $ID
fi
ID=$(ps | grep "rf_lb_test" | grep -v "$0" | grep -v "grep" | awk '{print $1}')
if [ -n "$ID" ];then
	kill $ID
fi

echo "-------------------------test4 start---------------------------" 
sfwifi remove > log3

if [ -z "$1" ]
then 
	max=8640
	echo "------------------it will last 1440 minute-----------------------"
else
	max=$(($1 * 6))
	echo "------------------it will last $1 minute-------------------------"
fi

rf_misc.sh all

rf_hb_test > log1 &
rf_lb_test > log2 &

timer=0
while [ $timer -lt $max ]
do	
	timer=$(($timer + 1))
	grep -q -E "rf_set_channel fail|aet_trx_init fail" log1
	if [ "$?" -eq "0" ]
	then
		echo "hb_test test file,please check log1!"
		break
	fi

	grep -q -E "rf_set_channel fail|aet_trx_init fail" log2
	if [ "$?" -eq "0" ]
	then
		echo "lb_test test file,please check log2!"
		break
	fi

	grep -q "test end" log1
	if [ "$?" -eq "0" ]
	then
		echo "hb_test end!"
		break
	fi

	grep -q "test end" log2
	if [ "$?" -eq "0" ]
	then
		echo "lb_test end!"
		break
	fi

	time=$(($timer * 10))
	echo "--------------running $time sec-------------- "
	sleep 10
	ret=$(($timer % 100))
	if [ $ret = 0 ];then
		echo "" > log1
		echo "" > log2
	fi
done

ID=$(ps | grep "rf_hb_test" | grep -v "$0" | grep -v "grep" | awk '{print $1}')
if [ -n "$ID" ];then
	kill $ID
fi
ID=$(ps | grep "rf_lb_test" | grep -v "$0" | grep -v "grep" | awk '{print $1}')
if [ -n "$ID" ];then
	kill $ID
fi
exit
