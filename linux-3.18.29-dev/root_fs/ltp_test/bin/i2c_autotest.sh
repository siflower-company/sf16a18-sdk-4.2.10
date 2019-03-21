#!/bin/ash



######################################################################
## mode: test type: tt (transmission test), et(exception test)
## num: transmission times( num * num)
## success: transmission successful times
## fail: 	transmission failed times
## msg_num: each transmission contains how many msgs
## msg_len: each msgs contains how many bytes data
#####################################################################
mode=tt
num=20
success=0
fail=0
msg_num=8
msg_len=4096

IsSubString(){
	local x=1
	case "$2" in 
	*$1*) x=0;;
	esac
	return $x
}

for args in $@
do
	IsSubString "num=" "$args"
	if [ "$?" == "0" ]; then
		num=${args##*num=}
	else
		IsSubString "msg_num=" "$args"
		if [ "$?" == "0" ]; then
			msg_num=${args##*msg_num=}
		else
			IsSubString "msg_len="	 "$args"
			if [ "$?" == "0" ]; then
				msg_len=${args##*msg_len=}
			else
				IsSubString "mode=" "$args"
				if [ "$?" == "0" ] ; then
					mode=${args##*mode=}
				fi
			fi
		fi
	fi
done

num=$((num))
msg_num=$((msg_num))
msg_len=$((msg_len))
avarge_speed=0

cat /proc/modules | grep "i2c_test" > /dev/null
if [ "0" -ne "$?" ]; then
	insmod /lib/modules/3.18.29/i2c-test.ko
fi

cat /proc/modules | grep "i2c_dev" > /dev/null
if [ "0" -ne "$?" ]; then
	insmod /lib/modules/3.18.29/i2c-dev.ko
fi

if [ ! -c '/dev/i2c-dev' ]; then
	mknod /dev/i2c-dev c 256 0
fi

if [ "$mode" == "tt" ] ; then
	
	for j in `seq $num`
	do
		#get the uptime to caculate transmission speed
		start=`cat /proc/uptime | awk -F '.' '{printf"%d\n",$1 *100 + $2}'`
		for i in `seq $num`
		do
			/bin/i2c_test $msg_num $msg_len
			if [ "$?" == "0" ] ;then
				let success=success+1
			else
				let fail=fail+1
			fi
			echo "" | awk -v num=$num -v i=$i -v j=$j -v success=$success -v fail=$fail '{printf"%f%% completed, %d success, %d fail\n", ((j -1) * num + i ) / (num * num ) * 100, success, fail}' 
		done

		end=`cat /proc/uptime | awk -F '.' '{printf"%d\n",$1 * 100 +  $2}'`
		let size=2*msg_num*msg_len*num
		echo -n "Transmit data size is ${size} bytes in total in $(((end - start)*10)) ms;"
		let "speed=size * 100 / (end - start)"
		echo  "Transmit speed is $speed B/s."
		let "avarge_speed=avarge_speed + speed"
	done
	echo "Transmit avarge speed is $((avarge_speed/num)) B/s."
elif [ "$mode" == "et" ]; then
	echo "msgs_point_to_null" > /dev/i2c-dev
	echo "msgs_num_negative" > /dev/i2c-dev
	echo "msgs_num_unmatch_msgs_size" > /dev/i2c-dev
	echo "adapter_point_to_null" > /dev/i2c-dev
fi 
