#
# Copyright (C) 2008-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=sf_switch
PKG_RELEASE:=1

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/sf_switch
  SUBMENU:=Network Devices
  TITLE:=Siflower SFA18 switch support
  FILES:=$(PKG_BUILD_DIR)/sfax8eth.ko
  AUTOLOAD:=$(call AutoLoad,55,sfax8eth,1)
  KCONFIG:=
endef

define KernelPackage/sf_switch/config
	if PACKAGE_kmod-sf_switch

		config SFAX8_SWITCH_VLAN
			bool "vlan config enable"
			default "n"

		config SFAX8_SWITCH_DPS
			bool "detect port status"
			default "y"

	endif
endef

#DDONOT_SHUTDOWN_WAN
EXTRA_KCONFIG:= \
	CONFIG_NET_VENDOR_SIFLOWER=y \
	CONFIG_SFAX8_SWITCH=m \

NOSTDINC_FLAGS += -DSF_TX_SHUTDOWN
NOSTDINC_FLAGS += -DCONFIG_SFAX8_SWITCH_AGEING

ifdef CONFIG_TARGET_siflower_sf16a18_fullmask
NOSTDINC_FLAGS += -DCONFIG_SF16A18_FULL_MASK
endif

SFAX8_KO_NAME="sfax8eth_switch.ko"

ifdef CONFIG_SFAX8_SWITCH_VLAN
ifdef CONFIG_KERNEL_DEBUG_FS
SFAX8_KO_NAME="sfax8eth_vlan.ko"
else
SFAX8_KO_NAME="sfax8eth_vlan_nodebugfs.ko"
endif
endif

ifdef CONFIG_SFAX8_SWITCH_DPS
NOSTDINC_FLAGS += -DCONFIG_SFAX8_SWITCH_DPS
endif

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./Makefile $(PKG_BUILD_DIR)/
endef

define Build/Compile
$(CP) ./$(SFAX8_KO_NAME) $(PKG_BUILD_DIR)/sfax8eth.ko
endef

$(eval $(call KernelPackage,sf_switch))
