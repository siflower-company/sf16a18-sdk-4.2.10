#!/bin/ash

sfwifi remove
sleep 1

rf_misc.sh $1
sleep 1

if [ $1 == "lb" ]
then
ifconfig wlan0 up
fi

if [ $1 == "hb" ]
then
ifconfig wlan0 up
fi

if [ -z "$1" ]
then
ifconfig wlan0 up
ifconfig wlan1 up
fi

sleep 1

cmd="ate_cmd"
echo $cmd
eval $cmd
