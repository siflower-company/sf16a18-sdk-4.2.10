/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 * Copyright (C) 2008-2009 Gabor Juhos <juhosg@openwrt.org>
 * Copyright (C) 2013 John Crispin <blogic@openwrt.org>
 */

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/sizes.h>
#include <linux/of_fdt.h>
#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_net.h>

#include <asm/reboot.h>
#include <asm/bootinfo.h>
#include <asm/addrspace.h>
#include <asm/prom.h>

__iomem void *rt_sysc_membase;
__iomem void *rt_memc_membase;

void __init device_tree_init(void)
{
	unflatten_and_copy_device_tree();
}

void __init of_setup(void)
{
	set_io_port_base(KSEG1);

#ifdef CONFIG_BUILTIN_DTB
	/*
	 * Load the builtin devicetree. This causes the chosen node to be
	 * parsed resulting in our memory appearing
	 */
	__dt_setup_arch(__dtb_start);
#endif
}

static const struct of_device_id siflower_of_match[] = {
	{ .compatible = "siflower,sf16a18-soc", },
	{ .compatible = "palmbus", },
	{},
};

static int __init plat_of_setup(void)
{
	if (!of_have_populated_dt())
		panic("device tree not present");

	platform_device_register_simple("cpufreq-dt", -1, NULL, 0);

	return of_platform_populate(NULL, siflower_of_match, NULL, NULL);
}

arch_initcall(plat_of_setup);

