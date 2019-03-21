#!/bin/sh
set -x
ipaddr="192.168.5.251"
netmask="255.255.255.0"
dns=""
standbyDns=""
connectmode=0
dhcp_enable="1"
connectmode=`uci get network.lan.connectmode`
if [  $? -eq 1 ]; then
    connectmode=0
    uci set network.lan.connectmode="0"
fi
dhcp_enable=`uci get network.lan.dhcp_enable`
if [  $? -eq 1 ]; then
    dhcp_enable=1
    uci set network.lan.dhcp_enable="1"
fi
uci commit
if [ $connectmode == 0 ];then                          #auto mode
	echo "connectmode=$connectmode"
		echo "----------------------------------------------auto" >> /dev/ttyS0
	ap=`uci get network.lan.dhcp_enable`
	if [ "$ap" == "1" ]; then                          #dhcp server is open
		echo "----------------------------------------------ap is open" >> /dev/ttyS0
		n=`awk '{print NR}' /tmp/sf_log.txt|tail -n1`
		uci set network.lan.proto='dhcp'
		uci commit network
		ubus call network reload
		result=`ps -w |grep udhcpc`
		sleep 3
		cat /tmp/sf_log.txt
		for i in `seq 5`
		do
			m=`awk '{print NR}' /tmp/sf_log.txt|tail -n1`
			sleep 1
		done
		result=`awk 'NF{a=$0}END{print a}' /tmp/sf_log.txt`
		if [ $m -gt $n ]; then                               #ac is open
			echo "------------------------------------------------------ac is open" >> /dev/ttyS0
			ipaddr=`echo $result |awk '{print $10}' |awk -F ':' '{print $2}'`
			netmask=`echo $result |awk '{print $11}' |awk -F ':' '{print $2}'`
			gateway=`echo $result |awk '{print $12}' |awk -F ':' '{print $2}'`
			dns=`echo $result |awk '{print $13}' |awk -F ':' '{print $2}'`
			echo "ipaddr=$ipaddr"
			echo "netmask=$netmask"
			echo "router=$router"
			echo "dns=$dns"
			echo "-------------------------------ipaddr=$ipaddr"  >> /dev/ttyS0
			echo "-------------------------------netmask=$netmask"  >> /dev/ttyS0

			uci set network.lan.ipaddr="$ipaddr"
			uci set network.lan.netmask="$netmask"
			uci set network.lan.gateway="$gateway"
			uci set network.lan.dns="$dns"
			uci commit network
			result=`uci set dhcp.lan.ignore='1'`            #close ap lan
			result=`uci commit dhcp`                        #save lan
			/etc/init.d/dnsmasq restart
		else
			echo "------------------------------------------------------ac is close" >> /dev/ttyS0
			#status=`uci get network.lan.proto`
			#echo "status=$status" >> /dev/ttyS0
			#if [ $status == "dhcp" ]; then
			#	echo "status=$status" >> /dev/ttyS0
			#	uci set network.lan.proto='static'
			#	uci commit network
			#	ubus call network reload
			#fi
			echo "-------------------------------ipaddr='192.168.5.251'"  >> /dev/ttyS0
			echo "-------------------------------netmask='255.255.255.0'"  >> /dev/ttyS0
			ifconfig br-lan $ipaddr                      #set br-lan ip
			uci set network.lan.ipaddr="$ipaddr"
			uci set network.lan.netmask="$netmask"
			uci set network.lan.gateway=""
			uci set network.lan.dns=""
			uci set network.lan.standbyDns=""
			uci commit network
			#result=`uci set dhcp.lan.ignore=''`            #close ap lan
			#result=`uci commit dhcp`                        #save lan
			#/etc/init.d/dnsmasq restart
			echo "-------------------------------uci set dhcp.lan.ignore=''"  >> /dev/ttyS0
		fi
	elif [ "$ap" == "0" ] ; then                         #dhcp server is close
		echo "--------------------------------------------------,ap is close" >> /dev/ttyS0
		status=`uci get network.lan.proto`
		if [ $status == "dhcp" ]; then
			uci set network.lan.proto='static'
			uci commit network
			ubus call network reload
		fi
		#uci set network.lan.proto='static'
		#uci commit network
		#ubus call network reload
		echo "-------------------------------ipaddr='192.168.5.251'"  >> /dev/ttyS0
		echo "-------------------------------netmask='255.255.255.0'"  >> /dev/ttyS0
		ifconfig br-lan $ipaddr                      #set br-lan ip
		uci set network.lan.ipaddr=$ipaddr
		uci set network.lan.netmask=$netmask
		uci set network.lan.gateway=""
		uci set network.lan.dns=""
		uci set network.lan.standbyDns=""
		uci commit network
		result=`uci set dhcp.lan.ignore='1'`            #close ap lan
		result=`uci commit dhcp`                        #save lan
		/etc/init.d/dnsmasq restart
		echo "-------------------------------uci set dhcp.lan.ignore='1'"  >> /dev/ttyS0
	fi
else
	echo "connectmode=$connectmode"                          #static mode
	echo "----------------------------------------------static" >> /dev/ttyS0
	status=`uci get network.lan.proto`
	if [ $status == "dhcp" ]; then
		uci set network.lan.proto='static'
		uci commit network
		ubus call network reload
	fi
	ip=`uci get network.lan.ipaddr`
	ifconfig br-lan $ip
	echo "-------------------------------ipaddr=$ip"  >> /dev/ttyS0
	#uci set network.lan.proto='static'
	#uci commit network
	#ubus call network reload
fi
