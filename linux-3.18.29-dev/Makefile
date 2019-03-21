#this file is for build linux and initramfs

$(info build kernel start--$(CURDIR)--makelevel=$(MAKELEVEL))


LINUX_DIR=$(CURDIR)/linux-3.18.29
#export env necessary
CROSS_COMPILE:=$(CURDIR)/toolchain/toolchain-mipsel_mips32_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-
ARCH:=mips
STAGING_DIR:=$(CURDIR)/include
STAGING_DIR_HOST:=$(CURDIR)/host
KDIR:=$(CURDIR)
LINUX_VERSION:=3.18.29
IMAGE_DIR:=$(CURDIR)/include/image
SIGN:=$(CURDIR)/sign_image

INITRAMFS_SOURCE_BUSYBOX=$(CURDIR)/root_fs/busybox
INITRAMFS_SOURCE_OPENWRT=$(CURDIR)/root_fs/chaos_calmer
INITRAMFS_SOURCE_LTPTEST=$(CURDIR)/root_fs/ltp_test
INITRAMFS_SOURCE_OPENWRT_WIFI=$(CURDIR)/root_fs/root-siflower-wifi

ATE_CMD_TEST_APP_DIR=$(CURDIR)/test/ate_cmd

ifeq ($(p10), 1)
	PRJ:=p10
$(info make p10)
else
ifeq ($(fpga), 1)
	PRJ:=fpga
$(info make fpga)
else
	PRJ:=p20
$(info make p20)
endif
endif

ifeq ($(mpw1), 1)
	VER:=mpw1
else ifeq ($(mpw0), 1)
	VER:=mpw0
else
	VER:=fullmask
endif

ifeq ($(security_boot), 1)
	BOOT_FLAG:=SECURITY_BOOT
else
	BOOT_FLAG:=
endif


BOARD=sf16a18_$(VER)_$(PRJ)

ifeq ($(ltp), 1)
INITRAMFS_SOURCE=$(INITRAMFS_SOURCE_LTPTEST)
$(info make ltp test)
else ifeq ($(busybox), 1)
INITRAMFS_SOURCE=$(INITRAMFS_SOURCE_BUSYBOX)
$(info make busybox)
else
INITRAMFS_SOURCE=$(INITRAMFS_SOURCE_OPENWRT_WIFI)
$(info make wifi)
endif

$(info board $(BOARD))
TARGET_DIR=$(INITRAMFS_SOURCE)
BUSYBOX_DIR=$(CURDIR)/busybox/busybox-1.23.2
OBJCOPY_STRIP = -R .reginfo -R .notes -R .note -R .comment -R .mdebug -R .note.gnu.build-id
export CROSS_COMPILE ARCH STAGING_DIR STAGING_DIR_HOST KDIR LINUX_VERSION TARGET_DIR SIGN BOOT_FLAG

define Kernel/CopyImage
	$(CROSS_COMPILE)objcopy -O binary $(OBJCOPY_STRIP) -S $(LINUX_DIR)/vmlinux $(CURDIR)/vmlinux$(1)
	$(CROSS_COMPILE)objcopy $(OBJCOPY_STRIP) -S $(LINUX_DIR)/vmlinux $(CURDIR)/vmlinux$(1).elf
endef

define Kernel/Dasm
	$(CROSS_COMPILE)objdump -d -S -l $(LINUX_DIR)/vmlinux > $(CURDIR)/vmlinux-dasm$(1)
endef

define Kernel/SetNoInitramfs
	mv $(LINUX_DIR)/.config $(LINUX_DIR)/.config.old
	grep -v INITRAMFS $(LINUX_DIR)/.config.old > $(LINUX_DIR)/.config
	echo 'CONFIG_INITRAMFS_SOURCE=""' >> $(LINUX_DIR)/.config
endef

define Kernel/SetInitramfs
	mv $(LINUX_DIR)/.config $(LINUX_DIR)/.config.old
	grep -v INITRAMFS $(LINUX_DIR)/.config.old > $(LINUX_DIR)/.config
	echo 'CONFIG_INITRAMFS_SOURCE="$(INITRAMFS_SOURCE)"' >> $(LINUX_DIR)/.config
	echo 'CONFIG_INITRAMFS_ROOT_UID=$(shell id -u)' >> $(LINUX_DIR)/.config
	echo 'CONFIG_INITRAMFS_ROOT_GID=$(shell id -g)' >> $(LINUX_DIR)/.config
endef

#to support vlan in openwrt rootfs
define Kernel/SupportVLAN
	mv $(LINUX_DIR)/.config $(LINUX_DIR)/.config.old
	grep -v SFAX8_SWITCH_VLAN $(LINUX_DIR)/.config.old > $(LINUX_DIR)/.config
	echo 'CONFIG_SFAX8_SWITCH_VLAN=y' >> $(LINUX_DIR)/.config
	echo 'CONFIG_SFAX8_SWITCH_DPS=y' >> $(LINUX_DIR)/.config
	echo 'CONFIG_SFAX8_SWITCH_POWERSAVE=n' >> $(LINUX_DIR)/.config
endef

define Kernel/CopyKo
	mkdir -p $(INITRAMFS_SOURCE)/lib/modules/3.18.29
	rm $(INITRAMFS_SOURCE)/lib/modules/3.18.29/*.ko -rf
	find $(LINUX_DIR)/ -name *.ko -exec cp {} $(INITRAMFS_SOURCE)/lib/modules/3.18.29 \;
endef

define Kernel/PrepareSrc
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/lb_smac/bb_src/ -rf
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/lb_fmac/bb_src/ -rf
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/hb_smac/bb_src/ -rf
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/hb_fmac/bb_src/ -rf
	cp $(LINUX_DIR)/drivers/net/wireless/siflower/bb_src/ $(LINUX_DIR)/drivers/net/wireless/siflower/lb_smac/ -rf
	cp $(LINUX_DIR)/drivers/net/wireless/siflower/bb_src/ $(LINUX_DIR)/drivers/net/wireless/siflower/lb_fmac/ -rf
	cp $(LINUX_DIR)/drivers/net/wireless/siflower/bb_src/ $(LINUX_DIR)/drivers/net/wireless/siflower/hb_smac/ -rf
	cp $(LINUX_DIR)/drivers/net/wireless/siflower/bb_src/ $(LINUX_DIR)/drivers/net/wireless/siflower/hb_fmac/ -rf
endef

define Kernel/CleanSrc
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/lb_smac/bb_src/ -rf
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/lb_fmac/bb_src/ -rf
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/hb_smac/bb_src/ -rf
	rm $(LINUX_DIR)/drivers/net/wireless/siflower/hb_fmac/bb_src/ -rf
endef


.PHONY: all busybox
all: kernel

busybox:
	make -C $(BUSYBOX_DIR) sf16a18_defconfig
	+make -C $(BUSYBOX_DIR)
	rm -rf $(BUSYBOX_DIR)/_install
	make -C $(BUSYBOX_DIR) install
	cp -rf $(BUSYBOX_DIR)/_install/bin/* $(INITRAMFS_SOURCE_BUSYBOX)/bin/
	cp -rf $(BUSYBOX_DIR)/_install/sbin/* $(INITRAMFS_SOURCE_BUSYBOX)/sbin/
	cp -rf $(BUSYBOX_DIR)/_install/usr/bin/* $(INITRAMFS_SOURCE_BUSYBOX)/usr/bin/
	cp -rf $(BUSYBOX_DIR)/_install/usr/sbin/* $(INITRAMFS_SOURCE_BUSYBOX)/usr/sbin/

kernel:
	$(call Kernel/PrepareSrc)
ifneq ($(LINUX_DIR)/.config, $(wildcard $(LINUX_DIR)/.config))
	+make -C $(LINUX_DIR) $(BOARD)_defconfig
endif
ifeq ($(INITRAMFS_SOURCE), $(INITRAMFS_SOURCE_OPENWRT_WIFI))
	$(call Kernel/SupportVLAN)
endif
	$(call Kernel/SetNoInitramfs)
	+make -C $(LINUX_DIR)
	$(call Kernel/CopyImage)
	$(call Kernel/CopyKo)
	$(call Kernel/SetInitramfs)
	+make -C $(LINUX_DIR)
	$(call Kernel/CopyImage,-initramfs)
	cp $(LINUX_DIR)/vmlinux $(CURDIR)/vmlinux.debug
	+make -C $(IMAGE_DIR)
	$(call Kernel/CleanSrc)

dasm:
	$(call Kernel/Dasm,-initramfs)

clean:
	$(call Kernel/PrepareSrc)
	rm -rf $(CURDIR)/vmlinux*
	make -C $(LINUX_DIR) distclean
	make -C $(IMAGE_DIR) clean
	make -C $(BUSYBOX_DIR) distclean
	$(call Kernel/CleanSrc)
