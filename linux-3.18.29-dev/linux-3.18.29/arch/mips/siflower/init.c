/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * PROM library initialisation code.
 *
 * Copyright (C) 1999,2000,2004,2005,2012  MIPS Technologies, Inc.
 * All rights reserved.
 * Authors: Carsten Langgaard <carstenl@mips.com>
 *         Maciej W. Rozycki <macro@mips.com>
 *          Steven J. Hill <sjhill@mips.com>
 */
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>

#include <asm/cacheflush.h>
#include <asm/smp-ops.h>
#include <asm/traps.h>
#include <asm/fw/fw.h>
#include <asm/mips-cm.h>
#include <asm/mips-cpc.h>
#include <generic.h>

#ifdef CONFIG_SOC_SF16A18
#include <sf16a18.h>
#endif

static void __init mips_nmi_setup(void)
{
	void *base;
	extern char except_vec_nmi;

	base = cpu_has_veic ?
		(void *)(CAC_BASE + 0xa80) :
		(void *)(CAC_BASE + 0x380);
	memcpy(base, &except_vec_nmi, 0x80);
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

static void __init mips_ejtag_setup(void)
{
	void *base;
	extern char except_vec_ejtag_debug;

	base = cpu_has_veic ?
		(void *)(CAC_BASE + 0xa00) :
		(void *)(CAC_BASE + 0x300);
	memcpy(base, &except_vec_ejtag_debug, 0x80);
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

phys_t mips_cpc_default_phys_base(void)
{
	return CPC_BASE_ADDR;
}

void __init prom_init(void)
{
	board_nmi_handler_setup = mips_nmi_setup;
	board_ejtag_handler_setup = mips_ejtag_setup;

	fw_init_cmdline();
	fw_meminit();
#ifdef CONFIG_SERIAL_8250_CONSOLE
	console_config();
#endif
	mips_cm_probe();
	mips_cpc_probe();

	if (!register_cps_smp_ops())
		return;
	if (!register_cmp_smp_ops())
		return;
	if (!register_vsmp_smp_ops())
		return;
}
