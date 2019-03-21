#
# Copyright (C) 2009 OpenWrt.org
#

SUBTARGET:=sf16a18-mpw1
BOARDNAME:=sf16a18 mpw1 based boards
ARCH_PACKAGES:=mips_siflower
FEATURES+=usb
CPU_TYPE:=mips-interAptiv


#DEFAULT_PACKAGES += kmod-sf_smac

define Target/Description
	Build firmware images for siflower sf16a18 mpw1 based boards.
endef
