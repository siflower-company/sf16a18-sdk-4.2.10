#!/bin/sh

enable=`cat /etc/watchdog-ping/watchdog_ping.conf |grep enable |awk '{print $2}'`
ipaddr=`cat /etc/watchdog-ping/watchdog_ping.conf |grep ipaddr |awk '{print $2}'`
period=`cat /etc/watchdog-ping/watchdog_ping.conf |grep period |awk '{print $2}'`
delay=`cat /etc/watchdog-ping/watchdog_ping.conf |grep delay |awk '{print $2}'`
maxdrop=`cat /etc/watchdog-ping/watchdog_ping.conf |grep maxdrop |awk '{print $2}'`
running_time=`cat /proc/uptime |awk '{print $1}' |cut -d '.' -f1`
dropcount=0

if [ "$enable" -eq "1" ]; then
	echo "Restart watchdog ping" > /dev/ttyS0
	if [ $running_time -lt 30 ]; then
		echo "Found system reboot, delay $delay second to enable watchdog ping" > /dev/ttyS0
		sleep $delay
	fi

	while true
	do
		if !(ping -c 1 $ipaddr > /dev/null); then
			dropcount=$(($dropcount + 1))
			if [ $dropcount -ge $maxdrop ]; then
				dropcount=0
				echo "Ping $ipaddr reach maxdrop $maxdrop, restart AP" > /dev/ttyS0
				/sbin/reboot
			fi
		fi
		sleep $period
	done
fi

exit
