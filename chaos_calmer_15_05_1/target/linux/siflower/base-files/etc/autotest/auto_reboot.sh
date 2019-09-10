#!/bin/sh

time=0
timeout=60

echo "test enter ------------------------------------" > /dev/ttyS0

if [ "$1" = "-h" ]; then
    echo "auto test for auto reboot"
    echo "args is test times"
    exit
else
    if [ ! -f "/etc/config/cycle" ]; then
        if [ ! $1 ]; then
            echo "miss args, please use '-h' for help"
            exit
        fi
        touch /etc/config/cycle
        uci set cycle.times=cycle
        uci set cycle.times.total=$1
        uci commit cycle
    fi
fi

cycle=`uci get cycle.times.total`
index=`uci get cycle.times.index`
if [ -n "$index" ]; then
    index=`expr $index + 1`
    uci set cycle.times.index=$index
    uci commit cycle
else
    index=1
    uci set cycle.times.index=$index
    uci commit cycle
fi

#!/bin/sh
timer=100000
while [ $timer -gt 0 ]; do
	timer=$(($timer - 1))
	if ifconfig | grep "wlan1"; then
		echo "wlan1 up" > /dev/ttyS0
		sleep 3
		break
	fi
	sleep 2
done

if dmesg | grep "ieee80211_request_scan"; then
    echo "========cache hw scan++++++++++++++++++++++++++++++++++++++++++++++++++++========"
    exit
fi

if dmesg | grep "virtual address 00000001"; then
    echo "========cache virtual address++++++++++++++++++++++++++++++++++++++++++++++++++++========"
    exit
fi

#if dmesg | grep "RIU_IRQMACCCATIMEOUTMASKED_BIT"; then
#    echo "========cache the CCA TIMEOUT++++++++++++++++++++++++++++++++++++++++++++++++++++========"
#    exit
#fi

if [ $index -le $cycle ]; then
    echo "======test cycle is $index======" > /dev/ttyS0
    echo `date -I'seconds' |awk -F "[ +|T ]" '{print $1 "[" $2"]"}' > /dev/ttyS0`
    echo "===============autoreboot start!!!====================" > /dev/ttyS0
    /sbin/reboot
else
    echo "===============autoreboot end!!!====================" > /dev/ttyS0
    rm /etc/config/cycle
fi
