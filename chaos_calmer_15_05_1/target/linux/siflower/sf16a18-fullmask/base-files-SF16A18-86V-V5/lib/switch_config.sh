#!/bin/sh

. /lib/functions.sh

switch_to_fit() {
	last_config=`grep mode /etc/config/system`
	if [ "$last_config" == "" ]; then
		echo "switch_config switch to fit" > /dev/ttyS0
		eval "tar -zcvf  /etc/config_fat.tar /etc/config/*"
		if [ -f /etc/config_fit.tar ]; then
			rm /etc/config/* -rf
			eval "tar -zxvf  /etc/config_fit.tar  -C /"
		fi
	else
		echo "switch_config to fit nothing to do" > /dev/ttyS0
	fi
}

switch_to_fat()
{
	last_config=`grep mode /etc/config/system`
	if [ "$last_config" != "" ]; then
		echo "switch_config switch to fat $last_config" > /dev/ttyS0
		eval "tar -zcvf  /etc/config_fit.tar /etc/config/*"
		if [ -f /etc/config_fat.tar ]; then
			echo "switch_config untart fat" > /dev/ttyS0
			rm /etc/config/* -rf
			eval "tar -zxvf  /etc/config_fat.tar  -C /"
		fi
	else
		echo "switch_config to fat nothing to do" > /dev/ttyS0
	fi
}
