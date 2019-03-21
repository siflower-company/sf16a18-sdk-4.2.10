#!/bin/ash

sfwifi reset

#wait all ready
sleep 50
if [ $3 == "down" ]; then
    ifconfig wlan0 down
    ifconfig wlan1 down
fi

echo 100 > /sys/kernel/debug/aetnensis/recalibrate

min=0
max=100000

while [ $min -le $max ]
do
min=`expr ${min} + 1`

SEED=`(tr -cd 0-9 </dev/urandom | head -c 8;)2>/dev/null`
RND_NUM=`echo $SEED $1 $2|awk '{srand($1);printf "%d",rand()*10000%($3-$2)+$2}'`
echo $RND_NUM

echo $RND_NUM > /sys/kernel/debug/aetnensis/recalibrate
sleep 3
done
