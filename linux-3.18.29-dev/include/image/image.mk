#
# Copyright (C) 2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#


LZMA_XZ_OPTIONS := -Xpreset 9 -Xe -Xlc 0 -Xlp 2 -Xpb 2
SQUASHFSCOMP_XZ := xz $(LZMA_XZ_OPTIONS)
SQUASHFS_BLOCKSIZE := 256k
SQUASHFSOPT := -b $(SQUASHFS_BLOCKSIZE)
SQUASHFSOPT += -p '/dev d 755 0 0' -p '/dev/console c 600 0 0 5 1'

define get_kernel_entry
0x$(shell $(CROSS_COMPILE)nm $(1) 2>/dev/null | grep " kernel_entry" | cut -f1 -d ' ')
endef

define CompressLzma
  $(STAGING_DIR_HOST)/bin/lzma e $(1) -lc1 -lp2 -pb2 $(2)
endef

define CompressGzip
	gzip -9 -c $(1) > $(2)
endef

define MkuImage
	if [ "${BOOT_FLAG}" = "SECURITY_BOOT" ]; then \
		sh $(SIGN)/sign.sh $1 $(call get_kernel_entry,$(KDIR)/vmlinux.debug) $(3) $(4);	\
	else	\
		mkimage -A mips -O linux -T kernel -a 0x80100000 -C $(1) $(2) \
			-e $(call get_kernel_entry,$(KDIR)/vmlinux.debug) -n 'MIPS OpenWrt Linux-$(LINUX_VERSION)' \
			-d $(3) $(4); \
	fi
endef

define Image/Prepare
	$(call CompressLzma,$(KDIR)/vmlinux,$(KDIR)/vmlinux.bin.lzma)
	$(call MkuImage,lzma,,$(KDIR)/vmlinux.bin.lzma,$(KDIR)/uImage.lzma)
	$(call CompressGzip,$(KDIR)/vmlinux,$(KDIR)/vmlinux.bin.gz)
	$(call MkuImage,gzip,,$(KDIR)/vmlinux.bin.gz,$(KDIR)/uImage.gz)
endef

define Image/Prepare-ramfs-img
	$(call CompressLzma,$(KDIR)/vmlinux-initramfs,$(KDIR)/vmlinux-initramfs.bin.lzma)
	$(call MkuImage,lzma,,$(KDIR)/vmlinux-initramfs.bin.lzma,$(KDIR)/uImage-initramfs.lzma)
	$(call CompressGzip,$(KDIR)/vmlinux-initramfs,$(KDIR)/vmlinux-initramfs.bin.gz)
	$(call MkuImage,gzip,,$(KDIR)/vmlinux-initramfs.bin.gz,$(KDIR)/uImage-initramfs.gz)
endef

define Image/mkfs/squashfs_xz
	$(STAGING_DIR_HOST)/bin/mksquashfs4 $(TARGET_DIR) $(KDIR)/root.squashfs -nopad -noappend -root-owned -comp $(SQUASHFSCOMP_XZ) $(SQUASHFSOPT) -processors $(if $(CONFIG_PKG_BUILD_JOBS),$(CONFIG_PKG_BUILD_JOBS),1)
endef

# pad to 4k, 8k, 16k, 64k, 128k, 256k and add jffs2 end-of-filesystem mark
define prepare_generic_squashfs
	$(STAGING_DIR_HOST)/bin/padjffs2 $(1) 4 8 16 64 128 256
endef

#use lzma uimage and xt compressed suqashfs
define Image/MakeSysupgradeBinary
	$(eval output_name=$(patsubst $(KDIR)/root_fs/%,%-squashfs-sysupgrade.bin,$(TARGET_DIR)))
	$(call Image/mkfs/squashfs_xz)
	cat $(KDIR)/uImage.lzma $(KDIR)/root.squashfs > $(KDIR)/$(output_name)
	$(call prepare_generic_squashfs,$(KDIR)/$(output_name))
endef

define Image/Build
	$(call Image/Build/$(1))
	dd if=$(KDIR)/root.$(1) of=$(BIN_DIR)/$(IMG_PREFIX)-root.$(1) bs=128k conv=sync
endef

define Image/CleanTmp
	rm -rf $(KDIR)/vmlinux.bin.lzma
	rm -rf $(KDIR)/vmlinux.bin.gz
	rm -rf $(KDIR)/vmlinux-initramfs.bin.lzma
	rm -rf $(KDIR)/vmlinux-initramfs.bin.gz
	rm -rf $(KDIR)/root.squashfs
endef

define Image/Clean
	rm -rf $(KDIR)/uImage.lzma
	rm -rf $(KDIR)/uImage.gz
	rm -rf $(KDIR)/uImage-initramfs.lzma
	rm -rf $(KDIR)/uImage-initramfs.gz
	rm -rf $(KDIR)/*-sysupgrade.bin
endef
