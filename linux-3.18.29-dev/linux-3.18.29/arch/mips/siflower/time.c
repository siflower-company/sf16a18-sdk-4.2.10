/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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
 *
 * Setting up the clock on the MIPS boards.
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/timex.h>

#include <asm/cpu.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/hardirq.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/setup.h>
#include <asm/time.h>
#include <asm/gic.h>

#include <generic.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clocksource.h>

#include <linux/clk-private.h>

#ifdef CONFIG_SOC_SF16A18
#include <sf16a18int.h>
#endif


static int mips_cpu_timer_irq;
static int mips_cpu_perf_irq;
extern int cp0_perfcount_irq;
extern int cpu_clkratio_setup(void);

static void mips_timer_dispatch(void)
{
	do_IRQ(mips_cpu_timer_irq);
}

static void mips_perf_dispatch(void)
{
	do_IRQ(mips_cpu_perf_irq);
}

static unsigned int freqround(unsigned int freq, unsigned int amount)
{
	freq += amount;
	freq -= freq % (amount*2);
	return freq;
}

static void __init plat_perf_setup(void)
{
	if (cp0_perfcount_irq >= 0) {
		if (cpu_has_vint)
			set_vi_handler(cp0_perfcount_irq, mips_perf_dispatch);
		mips_cpu_perf_irq = MIPS_CPU_IRQ_BASE + cp0_perfcount_irq;
#ifdef CONFIG_SMP
		irq_set_handler(mips_cpu_perf_irq, handle_percpu_irq);
#endif
	}
}

unsigned int get_c0_compare_int(void)
{
	if (cpu_has_vint)
		set_vi_handler(cp0_compare_irq, mips_timer_dispatch);
	mips_cpu_timer_irq = MIPS_CPU_IRQ_BASE + cp0_compare_irq;

	return mips_cpu_timer_irq;
}

void __init plat_time_init(void)
{
	unsigned int freq;
	struct device_node *gic_node = NULL;
	struct clk *gic_clk;

	of_clk_init(NULL);
	gic_node = of_find_compatible_node(NULL, NULL, "siflower,sfax8-gic");

	if(gic_node) {
		gic_clk = of_clk_get(gic_node,0);
		if(IS_ERR(gic_clk)) {
			printk("Can't get GIC clock!\n");
			return;
		} else {
			if (clk_prepare_enable(gic_clk) < 0) {
				pr_err("GIC failed to enable clock\n");
				clk_put(gic_clk);
				return;
			}
			gic_frequency = clk_get_rate(gic_clk);
			printk("get gic frequency from dts=%d\n", gic_frequency);
		}
		of_node_put(gic_node);
	} else {
		printk("error-can not get gic node\n");
	}

	//now we don't use cpu timer,so it is no sense here
	mips_hpt_frequency = gic_frequency/2;

#ifdef CONFIG_IRQ_GIC
	if (gic_present) {
		freq = freqround(gic_frequency, 5000);
		printk("GIC frequency %d.%02d MHz\n", freq/1000000,
		       (freq%1000000)*100/1000000);
#ifdef CONFIG_CSRC_GIC
		gic_clocksource_init(gic_clk);
#endif
	}
#endif
	clocksource_of_init();
	plat_perf_setup();
}
