scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/linux-siflower_sf16a18-fullmask/sf_smac/smac/sf16a18_smac.ko /lib/modules/3.18.29/
scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/linux-siflower_sf16a18-fullmask/sf_smac/smac/sf16a18_smac.ko /lib/modules/3.18.29/
scp robert@192.168.1.10:/home/robert/sf1688_clean/clean/sf16A18/bare_metal/fullmask/core1_wifi_fw/sf1688.bin /lib/firmware/
scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/linux-siflower_sf16a18-fullmask/compat-wireless-2016-01-10/net/mac80211/*.ko lib/modules/3.18.29/
echo 288 > /sys/kernel/debug/ieee80211/phy1/rc/fixed_rate_idx
echo 22 > /sys/kernel/debug/ieee80211/phy1/rwnx/txpower
iperf -u -c 192.168.5.125 -i 1 -b 600M -t 100 -l 32k -P 2
set_pwrmgmt dcdc0 1100000
echo 1000000000 > /sys/kernel/debug/cpu-freq
iperf -u -s -l 32k -i 1
echo 8 > /proc/sys/kernel/printk

ifconfig eth1 hw ether 10:16:88:B2:67:82
ifconfig eth1 up
brctl addif br-lan eth1

scp robert@192.168.1.10:~/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/linux-siflower_sf16a18-fullmask/sf_smac/startcore/*.ko /lib/modules/3.18.29/
scp robert@192.168.1.10:~/cc_clean/wireless-sw-sfax8/lmac/bin/* /lib/firmware/

scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/alsa-lib-1.0.28/src/.libs/libasound.so.2.0.0 /usr/lib/
arecord -f dat -d 1 -t wav /tmp/test_lu.wav &
arecord -Dhw:1,0 -f dat -d 1 -t wav /tmp/test_lu.wav

scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/netifd-2015-12-16/netifd /tmp/
scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/build_dir/target-mipsel_mips-interAptiv_uClibc-0.9.33.2/hostapd-mini/hostapd-2015-03-25/hostapd/hostapd /usr/sbin/hostapd
ifconfig br-lan 192.168.12.24
route add default gw 192.168.12.1
scp robert@192.168.1.10:/home/robert/cc_clean/latest/chaos_calmer_15_05_1/package/siflower/bin/sf-mm/files/ext/functions/* /usr/share/sf-mm/functions/
ubus call hostapd.wlan0 show_sta_whitelist
