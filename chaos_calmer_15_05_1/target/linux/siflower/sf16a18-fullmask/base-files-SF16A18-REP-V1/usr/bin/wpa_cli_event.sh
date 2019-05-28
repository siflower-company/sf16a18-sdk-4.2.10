#!/bin/ash
#echo "~$1~$2~$3~" > /dev/ttyS0
. /lib/functions.sh

wds_enabled=0
wds_if=$1
check_wds_connection () {
	local cfg="$1"

	config_get ifname "$cfg" ifname

	if [[ "$ifname" == "$wds_if" ]]; then
		config_get disable "$cfg" disabled
		if [[ "$disable" == "0" ]]; then
			wds_enabled="1"
		fi
	fi
	return 0
}

get_channel() {
	chan=`iwinfo $wds_if info | grep Chan|awk -F ' ' '{print $4}'`
	case $wds_if in
		sfi0)
			num=0
			;;
		sfi1)
			num=1
			;;
	esac
}

set_channel() {
	get_channel

	[ "$chan" -gt 0  ] && {
		uci set wireless.radio${num}.channel="$chan"
		# set sfix which is not in use disabled = 1,and set 5g htmode
		if [ $num = 1 ]; then
			uci set wireless.@wifi-iface[2].disabled='1'
			uci set wireless.radio1.htmode="VHT80"
			[ "$chan" = "165"  ] && uci set wireless.radio1.htmode="VHT20"
		else
			uci set wireless.@wifi-iface[3].disabled='1'
		fi
		uci commit wireless
		wifi reload
	}
}


if [[ "$wds_if" == "sfi0" || "$wds_if" == "sfi1" ]]; then
	wds_enabled="0"
	config_load wireless
	config_foreach check_wds_connection wifi-iface
	if [[ "$wds_enabled" != "1" ]]; then
		exit 0
	fi

	if [ "$2" == "CONNECTED" ]; then
		#echo "wpa_cli_evt: connected" > /dev/ttyS0
		local busy=`cat /tmp/wds_sta_status`
		while [ "$busy" == "b" ]
		do
			busy=`cat /tmp/wds_sta_status`
		done
		echo "b" > /tmp/wds_sta_status

		/bin/led-button -l 18
		sta_status=`uci get network.stabridge.disabled`
		echo "1" > /tmp/wds_connected
		# sta_status use to judge whether rep has been configured, 0 mean has been configured
		[ "$sta_status" = "0"  ] && {
			/etc/init.d/dnsmasq restart
			set_channel
		}

		echo "0" > /tmp/wds_sta_status
	fi

	if [ "$2" == "DISCONNECTED" ]; then
		#echo "wpa_cli_evt: disconnected" > /dev/ttyS0
		local busy=`cat /tmp/wds_sta_status`
		while [ "$busy" == "b" ]
		do
			busy=`cat /tmp/wds_sta_status`
		done
		echo "b" > /tmp/wds_sta_status
		# enable lan dhcp server
		echo "0" > /tmp/wds_connected
		/etc/init.d/dnsmasq restart

		local dns_status=`ps|grep dnsmasq|grep -vc grep`
		while [ "$dns_status" != "2" ]
		do
			sleep 1
			dns_status=`ps|grep dnsmasq|grep -vc grep`
		done
		# if wds disconnected ,reset disabled for scan normal
		uci set wireless.@wifi-iface[2].disabled='0'
		uci set wireless.@wifi-iface[3].disabled='0'
		uci commit

		# kick out devices
		ubus call lepton.network net_restart

		/bin/led-button -l 17

		echo "1" > /tmp/wds_sta_status

	fi

	# This is called by wds_sta_is_disconnected() in wirelessnew.lua
	if [ "$2" == "RECONNECT" ]; then
		local busy=`cat /tmp/wds_sta_status`
		while [ "$busy" == "b" ]
		do
			busy=`cat /tmp/wds_sta_status`
		done
		echo "b" > /tmp/wds_sta_status

		local result=""
		wpa_cli reconfigure
		wpa_cli scan
		result=`wpa_cli scan_result | grep $3 -i`
		if [ "$result""x" != "x" ]; then
			# host exists, try again
			echo "1" > /tmp/wds_sta_status
		else
			# no host
			echo "2" > /tmp/wds_sta_status
		fi
		# don't exit immediately, wait sta reconnect
		sleep 4
	fi

fi
#echo "wpa_cli_event done!" > /dev/ttyS0
