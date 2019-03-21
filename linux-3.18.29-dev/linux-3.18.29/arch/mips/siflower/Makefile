#
# Carsten Langgaard, carstenl@mips.com
# Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
#
# Copyright (C) 2008 Wind River Systems, Inc.
#   written by Ralf Baechle <ralf@linux-mips.org>
#
obj-y			:= init.o time.o init.o \
				   reset.o setup.o

obj-$(CONFIG_OF)    += of.o

obj-$(CONFIG_MIPS_CMP)		+= sf16a18-amon.o
obj-$(CONFIG_SOC_SF16A18) += sf16a18-clk.o sf16a18-memory.o sf16a18-int.o
obj-$(CONFIG_SOC_SF16A28) += sf16a28-clk.o sf16a28-memory.o sf16a28-int.o

