#!/bin/sh

sfwifi

times=10000
while [ $times -gt 0 ]; do
	echo "---wifi_iq_te_test time left $times----"
	times=$(($times - 1))
	echo 0x1 > /sys/kernel/debug/aetnensis/iq_te/sel_bb
	echo 0x1 > /sys/kernel/debug/aetnensis/iq_te/rp_mode
	echo 0x0 > /sys/kernel/debug/aetnensis/iq_te/iq_offset
	echo 0x6000 > /sys/kernel/debug/aetnensis/iq_te/iq_length
	echo 1 > /sys/kernel/debug/aetnensis/iq_te/iq_trigger
	cat /sys/kernel/debug/aetnensis/iq_te/iq_in_iram > /iq_player_data_check_${times}.bin

	diff /iq_player_data_check_${times}.bin /lib/firmware/iq_player_data.bin
	if [ $? -eq 0 ]; then
    	echo "player data check OK!"
	else
    	echo "error player data has been changed !!, times $times"
	fi

	rm -f /iq_player_data_check_${times}.bin

	echo 0x0 > /sys/kernel/debug/aetnensis/iq_te/sel_bb
	echo 0x0 > /sys/kernel/debug/aetnensis/iq_te/rp_mode
	echo 0x4 > /sys/kernel/debug/aetnensis/iq_te/iq_offset
	echo 0x6000 > /sys/kernel/debug/aetnensis/iq_te/iq_length
	echo 1 > /sys/kernel/debug/aetnensis/iq_te/iq_trigger
	#cat /sys/kernel/debug/aetnensis/iq_te/iq_in_iram > /iq_record_data_${times}.bin

	echo "/lib/firmware/iq_player_data.bin" > /sys/kernel/debug/aetnensis/iq_te/iq_in_iram
	cat /sys/kernel/debug/aetnensis/iq_te/iq_in_iram > /iq_write_data_check_${times}.bin

	diff /iq_write_data_check_${times}.bin /lib/firmware/iq_player_data.bin
	if [ $? -eq 0 ]; then
    	echo "write data check OK!"
	else
    	echo "error write data has been changed !!, times $times"
	fi
	rm -f /iq_write_data_check_${times}.bin
done
