/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2012 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/init.h>
#include <linux/time.h>
#include <linux/notifier.h>

#include <linux/clockchips.h>
#include <asm/gic.h>

#define GIC_CLK_NOTIFIER

static cycle_t gic_hpt_read(struct clocksource *cs)
{
	return gic_read_count();
}

static struct clocksource gic_clocksource = {
	.name	= "GIC",
	.read	= gic_hpt_read,
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

#ifdef GIC_CLK_NOTIFIER

extern struct clock_event_device gic_clockevent_device;
static void gic_update_frequency(void *data)
{
	unsigned long rate = (unsigned long)data;

	clockevents_update_freq(this_cpu_ptr(&gic_clockevent_device), rate);
}

static int gic_clk_notifier(struct notifier_block *nb, unsigned long action,
			    void *data)
{
	struct clk_notifier_data *cnd = data;

	if (action == POST_RATE_CHANGE)
		on_each_cpu(gic_update_frequency, (void *)cnd->new_rate, 1);

	return NOTIFY_OK;
}

static struct notifier_block gic_clk_nb = {
	.notifier_call = gic_clk_notifier,
};

#endif

void __init gic_clocksource_init(struct clk *clk)
{
	unsigned int config, bits;

	/* Calculate the clocksource mask. */
	GICREAD(GIC_REG(SHARED, GIC_SH_CONFIG), config);
	bits = 32 + ((config & GIC_SH_CONFIG_COUNTBITS_MSK) >>
		(GIC_SH_CONFIG_COUNTBITS_SHF - 2));

	/* Set clocksource mask. */
	gic_clocksource.mask = CLOCKSOURCE_MASK(bits);

	/* Calculate a somewhat reasonable rating value. */
	gic_clocksource.rating = 200 + clk->rate / 10000000;

	clocksource_register_hz(&gic_clocksource, clk->rate);
#ifdef GIC_CLK_NOTIFIER
	if (clk_notifier_register(clk, &gic_clk_nb) < 0)
		pr_warn("GIC: Unable to register clock notifier\n");
#endif
}

extern void gic_clockevent_update_freq(u32 freq);
void gic_clocksource_update(unsigned int frequency)
{
	clocksource_unregister(&gic_clocksource);
	clocksource_register_hz(&gic_clocksource, frequency);
	gic_clockevent_update_freq(frequency);

	return;
}
EXPORT_SYMBOL_GPL(gic_clocksource_update);
