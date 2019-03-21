#!/bin/ash
# Copyright (C) 2006 OpenWrt.org

echo "------------------------clear dmesg---------------------------"
dmesg -c > log2
echo "-------------------------test3 start---------------------------" 
echo "~~~~~~~~~~~~~~~~~~~~~key log~~~~~~~~~~~~~~~~~~~~~~~~" > log1
if [ -z "$1" ]
then
	timer=1
else
	timer=$1
fi

while [ $timer -gt 0 ];
do
	timer=$(($timer - 1))
	ate_init.sh hb
	sleep 1
	ate_cmd fastconfig -n 40000 -l 1024 -f 5785 -c 5785 -w 3 -u 3 -m 4 -i 6 -g 1 -p 25 -t
	sleep 50
	dmesg -c >log2
	grep -q "IRQ_HK" log2
	if [ "$?" -eq "0" ]
	then
			grep -n "IRQ_HK" log2 >> log1
			grep -n -B 5 "aet_app_check_status" log2 >> log1
			echo "-----timer:$timer HB----" >>log1
	fi

	ate_init.sh lb
	sleep 1
	ate_cmd fastconfig -n 40000 -l 1024 -f 2412 -c 2412 -w 2 -u 2 -m 2 -i 7 -g 1 -p 25 -t
	sleep 50
	dmesg -c > log2
	grep -q "IRQ_HK" log2
	if [ "$?" -eq "0" ]
	then
			grep -n "IRQ_HK" log2 >> log1
			grep -n -B 5 "aet_app_check_status" log2 >> log1
			echo "-----timer:$timer LB----" >>log1
	fi
	sleep 1
	echo "-------------timer is $timer -----------------------------"
done

grep -q "IRQ_HK" log1
if [ "$?" -eq "0" ]
then
	echo "----------test fail! Please read log1( cat log1 )---------------"
else
	echo "----------------- test success -----------------"
fi
