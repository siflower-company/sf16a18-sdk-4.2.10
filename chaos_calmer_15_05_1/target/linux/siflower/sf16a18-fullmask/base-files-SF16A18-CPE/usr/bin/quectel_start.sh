#!/bin/ash

# wait for usb connection
sleep 20

echo "EC20 start now!" > /dev/ttyS0
result=`ifconfig eth1 | grep inet addr`
while [ "$result" == "" ]
do
	quectel-CM &
	sleep 10
	result=`ifconfig eth1 | grep ”inet addr“`
done
echo "EC20 ends!" > /dev/ttyS0
