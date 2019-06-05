#!/bin/sh

if [ -z $1  ]; then
	echo -e "Input error, need parameters!\nUsage:\narg1 : PC ipaddr"
	exit
fi

var=0
limit=100000
while [ $var -lt $limit   ]
do
	iperf -c $1 -i 1 -t 30 -w 2M
	iperf -c $1 -i 1 -t 30 -w 2M -d
	var=`expr $var + 1`
done
