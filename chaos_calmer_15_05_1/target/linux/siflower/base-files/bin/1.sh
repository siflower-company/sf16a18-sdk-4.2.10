#!/bin/sh
. /lib/functions.sh
. /usr/share/libubox/jshn.sh

cookie_path=/tmp/auto_cookie

auto_ota_logger(){
	logger "auto ota: $@"
}

get_stok() {
	auth=$(cat /etc/shadow | sed -n '1p' | awk -F ':' '{print $2}')
	if [ "x$auth" = "x" ];then
		curl -D $1 "http://127.0.0.1/cgi-bin/luci/api/sfsystem/get_stok_local" -d '{"version":"V10","luci_name":"admin"}'
	else
		curl -D $1 -H "Authorization: $auth" "http://127.0.0.1/cgi-bin/luci/api/sfsystem/get_stok_local" -d '{"version":"V10","luci_name":"admin"}'
	fi
	result=`awk -F: ' /path/ {split($2,myarry,"=")} END {print myarry[4]}' $1`
	if [ -n $result ]; then
		echo "$result"
		return 1
	else
		return 0
	fi
}

purl_check() {
	url="http://127.0.0.1/cgi-bin/luci/;stok="$1"/api/sfsystem/pctl_url_check"
	check_result=`curl -b $2 -H "Content-type:application/json" -X POST $url -d '{"version":"V10","luci_name":"admin"}'`
#	echo "get ++++++++++++++++++++result=$check_result" > /dev/ttyS0
	json_load "$check_result"
	json_get_vars code
	if [ "x$code" == "x0" ]; then
		echo "++++update url success!" > dev/ttyS0
		json_select content
		json_get_vars code2
		if [ "x$code" == "x0" ]; then
			json_select data
			json_get_vars action updateAt province
			echo "++++update action=$action! updateAt=$updateAt" > dev/ttyS0
			if [ "x$action" == "x1" -o "x$action" == "x2" ]; then
				echo "$check_result" > /tmp/updatelist.bin
				tscfg -u /tmp/updatelist.bin
				checksum=`md5sum /tmp/updatelist.bin | awk '{print $1}'`
				uci set tsconfig.tsrecord.checksum="$checksum"
				echo "++++++++++++update=$updateAt" > /dev/ttyS0
#				uci set tsconfig.tsrecord.updateAt="$updateAt"
#				uci set tsconfig.tsrecord.province="$province"
				uci commit tsconfig
			fi
			json_select ..
		fi
		json_select ..
	fi
}

cookie=`get_stok $cookie_path | tr -d "\r"`
if [ -z $cookie ]; then
	auto_ota_logger "get stok fail :$cookie"
	return 0
fi
purl_check $cookie $cookie_path
