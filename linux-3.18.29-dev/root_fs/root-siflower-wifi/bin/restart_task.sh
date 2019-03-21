#!/bin/ash

echo 1 > /sys/kernel/debug/ieee80211/phy0/rwnx/diags/force_trigger_type
echo 1 > /sys/kernel/debug/ieee80211/phy1/rwnx/diags/force_trigger_type

cat /sys/kernel/debug/ieee80211/phy0/rwnx/diags/mactrace
cat /sys/kernel/debug/ieee80211/phy1/rwnx/diags/mactrace
