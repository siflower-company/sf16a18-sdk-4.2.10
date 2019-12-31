#
# Copyright (C) 2008-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=realtek_switch
PKG_RELEASE:=1

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/realtek_switch
  SUBMENU:=Network Devices
  TITLE:=Siflower SFA18 gigabit switch support
  FILES:=$(PKG_BUILD_DIR)/gswitch.ko
  AUTOLOAD:=$(call AutoLoad,61,gswitch,1)
  KCONFIG:=
endef

EXTRA_KCONFIG:= \
	CONFIG_NET_VENDOR_SIFLOWER=y \
	CONFIG_REALTEK_SWITCH=m \

NOSTDINC_FLAGS = \
	-I$(PKG_BUILD_DIR)

NOSTDINC_FLAGS += -DMDC_MDIO_OPERATION

ifdef CONFIG_TARGET_siflower_sf16a18_fullmask
NOSTDINC_FLAGS += -DCONFIG_SF16A18_FULL_MASK
endif

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

$(eval $(call KernelPackage,realtek_switch))
