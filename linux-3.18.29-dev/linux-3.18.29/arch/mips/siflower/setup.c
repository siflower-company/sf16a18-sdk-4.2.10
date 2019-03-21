/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 * Copyright (C) 2008 Dmitri Vorobiev
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/time.h>

#include <asm/fw/fw.h>
#include <asm/mips-cm.h>
#include <asm/mips-cpc.h>
#include <asm/traps.h>

#ifdef CONFIG_CORE1_MEM_RES
#include <asm/gic.h>
#endif

#ifdef CONFIG_SOC_SF16A18
#include <sf16a18.h>
#include <sf16a18int.h>
#endif

extern int sf16a18_be_handler(struct pt_regs *regs, int is_fixup);

const char *get_system_type(void)
{
	return "MIPS sf16a18";
}

#ifdef CONFIG_CORE1_MEM_RES
/**
 * Power up/down core1.
 * Attention! This is used to start core1 for wifi firmware.
 * If you don't actually know what they are, DO NOT use them!
 */
int core_pwrup(unsigned long entry)
{
	unsigned long core_entry = entry;
	int core = 1;

	//set core other access
	write_cpc_cl_other(core << CPC_Cx_OTHER_CORENUM_SHF);

	//set power down
	write_cpc_co_cmd(CPC_Cx_CMD_PWRDOWN);

	//config the reset base for core1 with GC:
	write_gcr_co_reset_base(CKSEG0ADDR((unsigned long)core_entry));

	//set power up
	write_cpc_co_cmd(CPC_Cx_CMD_PWRUP);

	return 0;
}
EXPORT_SYMBOL(core_pwrup);

int core_pwrdown(void)
{
	int core = 1;

	//set core other access
	write_cpc_cl_other(core << CPC_Cx_OTHER_CORENUM_SHF);

	//set power down
	write_cpc_co_cmd(CPC_Cx_CMD_PWRDOWN);

	return 0;
}
EXPORT_SYMBOL(core_pwrdown);

int set_ipi(unsigned int intr)
{
	writel(GIC_SH_WEDGE_SET(intr), (void*)GIC_ABS_REG(SHARED, GIC_SH_WEDGE));
	return 0;
}
EXPORT_SYMBOL(set_ipi);

#endif

void __init plat_mem_setup(void)
{
#ifdef CONFIG_OF
	extern void of_setup(void);
	of_setup();
#endif
	board_be_handler = sf16a18_be_handler;
}
