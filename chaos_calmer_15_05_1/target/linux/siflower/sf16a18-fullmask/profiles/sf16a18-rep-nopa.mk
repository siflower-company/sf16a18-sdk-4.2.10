#
# Copyright (C) 2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/SF16A18-REP-NOPA
 NAME:= SF16A18 REP NOPA
 PACKAGES:=\
	vsftpd vsftpd-tls\
	netdiscover iwinfo luci-lib-json kmod-button-hotplug kmod-reset-button
endef

define Profile/SF16A18-REP-NOPA/Description
 Support for siflower rep boards nopa.0
endef

define Profile/SF16A18-REP-NOPA/Config
select BUSYBOX_DEFAULT_FEATURE_TOP_SMP_CPU
select BUSYBOX_DEFAULT_FEATURE_TOP_DECIMALS
select BUSYBOX_DEFAULT_FEATURE_TOP_SMP_PROCESS
select BUSYBOX_DEFAULT_FEATURE_TOPMEM
select BUSYBOX_DEFAULT_CKSUM
select TARGET_ROOTFS_SQUASHFS
endef

$(eval $(call Profile,SF16A18-REP-NOPA))
