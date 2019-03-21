#!/bin/ash
#echo "~$1~$2~$3~" > /dev/ttyS0
if [[ "$1" == "sfi0" || "$1" == "rai0" ]]; then
	trig=phy0
else
	trig=phy1
fi
path_led="/sys/class/leds/siwifi-"

del_clients() {
	local cnt
	local device
	local clients

	# eth devices
	if [[ "$1" == "0" || "$1" == "2" ]]; then
		ifconfig eth0 down
		sleep 1
		ifconfig eth0 up
	fi

	# wifi devices
	if [[ "$1" == "1" || "$1" == "2" ]]; then
		cnt=`ubus list | grep -c hostapd`
		while [ $cnt -gt 0 ]
		do
			local ccnt
			#echo "cnt is $cnt" > /dev/ttyS0
			device=`ubus list | grep hostapd | awk 'NR=="'$cnt'" {print $1}'`
			#echo "device is $device" > /dev/ttyS0
			#clients=`ubus call $device get_clients | awk '/:..:/ {sub(/.$/,"",$1); print $1}'`
			clients=`ubus call $device get_clients | awk '/:..:/ {sub(/\":/,"",$1);sub(/\"/,"",$1); print $1}'`
			#echo "clients is $clients" > /dev/ttyS0
			ccnt=`echo "$clients" | grep -c ""`
			while [ $ccnt -gt 0 ]
			do
				local cli
				cli=`echo "$clients" | awk 'NR=="'$ccnt'" {print $1}'`
				#echo "cli is $cli" > /dev/ttyS0
				[ -n "$cli" ] && ubus call $device del_client '{"addr":"'$cli'", "reason":3, "deauth": True}'
				let ccnt=$ccnt-1
			done
			let cnt=$cnt-1
		done
	fi
}

if [[ "$1" == "sfi0" || "$1" == "sfi1" || "$1" == "rai0" || "$1" == "rai1" ]]; then

	if [ "$2" == "CONNECTED" ]; then
		#echo "wpa_cli_evt: connected" > /dev/ttyS0
		local busy=`cat /tmp/wds_sta_status`
		while [ "$busy" == "b" ]
		do
			busy=`cat /tmp/wds_sta_status`
		done
		echo "b" > /tmp/wds_sta_status

		# disable lan dhcp server
		uci set dhcp.lan.ignore=1
		uci commit dhcp
		/etc/init.d/dnsmasq reload

		if [ -d "$path_led""$trig""::tx" ]; then
			echo "$trig""tx" > "$path_led""$trig""::tx"/trigger
		fi
		if [ -d "$path_led""$trig""::rx" ]; then
			echo "$trig""rx" > "$path_led""$trig""::rx"/trigger
		fi
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
		uci set dhcp.lan.ignore=0
		uci commit dhcp
		/etc/init.d/dnsmasq reload

		# kick out devices, 0 for eth devices, 1 for wifi, 2 for all
		del_clients 2

		local is_enabled=`ifconfig | grep $1`
		if [ "$is_enabled" == "" ]; then
			echo "1" > /tmp/wds_sta_status
			exit 0
		fi

		if [ -d "$path_led""$trig""::tx" ]; then
			echo "timer" > "$path_led""$trig""::tx"/trigger
			echo "100" > "$path_led""$trig""::tx"/delay_on
			echo "100" > "$path_led""$trig""::tx"/delay_off
		fi
		if [ -d "$path_led""$trig""::rx" ]; then
			echo "timer" > "$path_led""$trig""::tx"/trigger
			echo "100" > "$path_led""$trig""::tx"/delay_on
			echo "100" > "$path_led""$trig""::tx"/delay_off
		fi

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
	fi

fi
#echo "wpa_cli_event done!" > /dev/ttyS0

