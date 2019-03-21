#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/hardirq.h>
#include <linux/preempt.h>
#include <linux/irqdomain.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/mips-cm.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/traps.h>

#include <asm/irq.h>
#include <asm/setup.h>

#include <asm/gic.h>

#ifdef CONFIG_SOC_SF16A18
#include <sf16a18int.h>
#include <sf16a18.h>
#endif

static struct irq_chip *irq_gic;

#ifdef CONFIG_MIPS_GIC_IPI
static unsigned int ipi_map[NR_CPUS];
DECLARE_BITMAP(ipi_ints, GIC_NUM_INTRS);
#endif

#define IPI_MAP_ENTRY 16
#define X GIC_UNUSED
#define HW_PIN(x) (x)
#define GIC_CPU_NMI GIC_MAP_TO_NMI_MSK

static struct gic_intr_map gic_intr_map[GIC_NUM_INTRS] = {
	{ X, X,        X,       X,      0 },
	{ X, X,        X,       X,      0 },
	{ X, X,        X,       X,      0 },
	{ X, X,        X,       X,      0 },
	{ X, X,        X,       X,      0 },
	{ X, X,        X,       X,      0 },
	{ 0, GIC_CPU_NMI,  GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_NMI,  GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },

	{ 0, HW_PIN(1), GIC_POL_POS, GIC_TRIG_EDGE, 0 },
	{ 1, HW_PIN(1), GIC_POL_POS, GIC_TRIG_EDGE, 0 },
	{ 2, HW_PIN(1), GIC_POL_POS, GIC_TRIG_EDGE, 0 },
	{ 3, HW_PIN(1), GIC_POL_POS, GIC_TRIG_EDGE, 0 },

	{ 0, HW_PIN(2), GIC_POL_POS, GIC_TRIG_EDGE, 0 },
	{ 1, HW_PIN(2), GIC_POL_POS, GIC_TRIG_EDGE, 0 },
	{ 2, HW_PIN(2), GIC_POL_POS, GIC_TRIG_EDGE, 0 },
	{ 3, HW_PIN(2), GIC_POL_POS, GIC_TRIG_EDGE, 0 },

};

#undef X


static char *tr[8] = {
	"mem",	"gcr",	"gic",	"mmio",
	"0x04", "0x05", "0x06", "0x07"
};

static char *mcmd[32] = {
	[0x00] = "0x00",
	[0x01] = "Legacy Write",
	[0x02] = "Legacy Read",
	[0x03] = "0x03",
	[0x04] = "0x04",
	[0x05] = "0x05",
	[0x06] = "0x06",
	[0x07] = "0x07",
	[0x08] = "Coherent Read Own",
	[0x09] = "Coherent Read Share",
	[0x0a] = "Coherent Read Discard",
	[0x0b] = "Coherent Ready Share Always",
	[0x0c] = "Coherent Upgrade",
	[0x0d] = "Coherent Writeback",
	[0x0e] = "0x0e",
	[0x0f] = "0x0f",
	[0x10] = "Coherent Copyback",
	[0x11] = "Coherent Copyback Invalidate",
	[0x12] = "Coherent Invalidate",
	[0x13] = "Coherent Write Invalidate",
	[0x14] = "Coherent Completion Sync",
	[0x15] = "0x15",
	[0x16] = "0x16",
	[0x17] = "0x17",
	[0x18] = "0x18",
	[0x19] = "0x19",
	[0x1a] = "0x1a",
	[0x1b] = "0x1b",
	[0x1c] = "0x1c",
	[0x1d] = "0x1d",
	[0x1e] = "0x1e",
	[0x1f] = "0x1f"
};

static char *core[8] = {
	"Invalid/OK",	"Invalid/Data",
	"Shared/OK",	"Shared/Data",
	"Modified/OK",	"Modified/Data",
	"Exclusive/OK", "Exclusive/Data"
};

static char *causes[32] = {
	"None", "GC_WR_ERR", "GC_RD_ERR", "COH_WR_ERR",
	"COH_RD_ERR", "MMIO_WR_ERR", "MMIO_RD_ERR", "0x07",
	"0x08", "0x09", "0x0a", "0x0b",
	"0x0c", "0x0d", "0x0e", "0x0f",
	"0x10", "0x11", "0x12", "0x13",
	"0x14", "0x15", "0x16", "INTVN_WR_ERR",
	"INTVN_RD_ERR", "0x19", "0x1a", "0x1b",
	"0x1c", "0x1d", "0x1e", "0x1f"
};

int sf16a18_be_handler(struct pt_regs *regs, int is_fixup)
{
	/* This duplicates the handling in do_be which seems wrong */
	int retval = is_fixup ? MIPS_BE_FIXUP : MIPS_BE_FATAL;

	if (mips_cm_present()) {
		unsigned long cm_error = read_gcr_error_cause();
		unsigned long cm_addr = read_gcr_error_addr();
		unsigned long cm_other = read_gcr_error_mult();
		unsigned long cause, ocause;
		char buf[256];

		cause = cm_error & CM_GCR_ERROR_CAUSE_ERRTYPE_MSK;
		if (cause != 0) {
			cause >>= CM_GCR_ERROR_CAUSE_ERRTYPE_SHF;
			if (cause < 16) {
				unsigned long cca_bits = (cm_error >> 15) & 7;
				unsigned long tr_bits = (cm_error >> 12) & 7;
				unsigned long cmd_bits = (cm_error >> 7) & 0x1f;
				unsigned long stag_bits = (cm_error >> 3) & 15;
				unsigned long sport_bits = (cm_error >> 0) & 7;

				snprintf(buf, sizeof(buf),
						"CCA=%lu TR=%s MCmd=%s STag=%lu "
						"SPort=%lu\n",
						cca_bits, tr[tr_bits], mcmd[cmd_bits],
						stag_bits, sport_bits);
			} else {
				/* glob state & sresp together */
				unsigned long c3_bits = (cm_error >> 18) & 7;
				unsigned long c2_bits = (cm_error >> 15) & 7;
				unsigned long c1_bits = (cm_error >> 12) & 7;
				unsigned long c0_bits = (cm_error >> 9) & 7;
				unsigned long sc_bit = (cm_error >> 8) & 1;
				unsigned long cmd_bits = (cm_error >> 3) & 0x1f;
				unsigned long sport_bits = (cm_error >> 0) & 7;
				snprintf(buf, sizeof(buf),
						"C3=%s C2=%s C1=%s C0=%s SC=%s "
						"MCmd=%s SPort=%lu\n",
						core[c3_bits], core[c2_bits],
						core[c1_bits], core[c0_bits],
						sc_bit ? "True" : "False",
						mcmd[cmd_bits], sport_bits);
			}

			ocause = (cm_other & CM_GCR_ERROR_MULT_ERR2ND_MSK) >>
				CM_GCR_ERROR_MULT_ERR2ND_SHF;

			pr_err("CM_ERROR=%08lx %s <%s>\n", cm_error,
					causes[cause], buf);
			pr_err("CM_ADDR =%08lx\n", cm_addr);
			pr_err("CM_OTHER=%08lx %s\n", cm_other, causes[ocause]);

			/* reprime cause register */
			write_gcr_error_cause(0);
		}
	}

	return retval;
}


#if defined(CONFIG_MIPS_MT_SMP)
static int gic_resched_int_base = 12;
static int gic_call_int_base = 8;

#define GIC_RESCHED_INT(cpu) (gic_resched_int_base+(cpu))
#define GIC_CALL_INT(cpu) (gic_call_int_base+(cpu))

static irqreturn_t ipi_resched_interrupt(int irq, void *dev_id)
{
#ifdef CONFIG_MIPS_VPE_APSP_API_CMP
	if (aprp_hook)
		aprp_hook();
#endif

	scheduler_ipi();

	return IRQ_HANDLED;
}

static irqreturn_t ipi_call_interrupt(int irq, void *dev_id)
{
	smp_call_function_interrupt();

	return IRQ_HANDLED;
}

static struct irqaction irq_resched = {
	.handler        = ipi_resched_interrupt,
	.flags          = IRQF_PERCPU,
	.name           = "ipi resched"
};

static struct irqaction irq_call = {
	.handler        = ipi_call_interrupt,
	.flags          = IRQF_PERCPU,
	.name           = "ipi call"
};

#endif

#ifdef CONFIG_MIPS_GIC_IPI
static void __init fill_ipi_map1(int baseintr, int cpu, int cpupin)
{
	int intr = baseintr + cpu;
	ipi_map[cpu] |= (1 << (cpupin + 2));
	bitmap_set(ipi_ints, intr, 1);
}

static void __init fill_ipi_map(void)
{
	int cpu;

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		fill_ipi_map1(gic_resched_int_base, cpu, GIC_CPU_INT1);
		fill_ipi_map1(gic_call_int_base, cpu, GIC_CPU_INT2);
	}
}
#endif

static void __init gic_fill_map(void)
{
	int i;
#ifdef CONFIG_MIPS_MT_SMP
	int normal_start = GIC_RESCHED_INT(nr_cpu_ids);
#else
	int normal_start = nr_cpu_ids;
#endif
	for (i = normal_start; i < ARRAY_SIZE(gic_intr_map); i++) {
		gic_intr_map[i].cpunum = 0;
		gic_intr_map[i].pin = GIC_CPU_INT0;
		gic_intr_map[i].polarity = GIC_POL_POS;
		gic_intr_map[i].trigtype = GIC_TRIG_LEVEL;
		gic_intr_map[i].flags = 0;
	}
#if defined(CONFIG_MIPS_GIC_IPI)
	fill_ipi_map();
#endif
}

void
gic_irq_ack(struct irq_data *d)
{
	int irq = (d->irq - gic_irq_base);

	GIC_CLR_INTR_MASK(irq);

	if (gic_irq_flags[irq] & GIC_TRIG_EDGE)
		GICWRITE(GIC_REG(SHARED, GIC_SH_WEDGE), irq);
}

void
gic_finish_irq(struct irq_data *d)
{
	GIC_SET_INTR_MASK(d->irq - gic_irq_base);
}

int siflower_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	unsigned int irq_nr = data->irq - MIPS_GIC_IRQ_BASE;
	int trigger = GIC_TRIG_LEVEL;
	int polarity = GIC_POL_POS;

	if (flow_type & IRQF_TRIGGER_PROBE)
		return 0;

	if (irq_nr > 0 && irq_nr < GIC_NUM_INTRS){
		switch (flow_type & IRQF_TRIGGER_MASK) {
		case IRQF_TRIGGER_RISING:
			trigger = GIC_TRIG_EDGE;
			polarity = GIC_POL_POS;
			break;
		case IRQF_TRIGGER_FALLING:
			trigger = GIC_TRIG_EDGE;
			polarity = GIC_POL_NEG;
			break;
		case IRQF_TRIGGER_HIGH:
			trigger = GIC_TRIG_LEVEL;
			polarity = GIC_POL_POS;
			break;
		case IRQF_TRIGGER_LOW:
			trigger = GIC_TRIG_LEVEL;
			polarity = GIC_POL_NEG;
			break;
		default:
			return -EINVAL;
		}
		GIC_SET_POLARITY(irq_nr, polarity);
		GIC_SET_TRIGGER(irq_nr, trigger);
		return 0;
	} else {
		return -EINVAL;
	}
}

void __init gic_platform_init(int irqs, struct irq_chip *irq_controller)
{
	irq_gic = irq_controller;
	//set irq_set_type func
	irq_controller->irq_set_type = siflower_irq_set_type;
}

static int sf_gic_get_int(void)
{
	DECLARE_BITMAP(interrupts, GIC_NUM_INTRS);

	bitmap_fill(interrupts, GIC_NUM_INTRS);
	gic_get_int_mask(interrupts, interrupts);

#ifdef CONFIG_CORE1_MEM_RES
	//irq 48
	if(interrupts[1] & 0x10000)
		interrupts[1] = interrupts[1] & (~0x10000);
	//irq 64
	if(interrupts[2] & 0x1)
		interrupts[2] = interrupts[2] & (~0x1);
#endif

	return find_first_bit(interrupts, GIC_NUM_INTRS);
}

static void gic_irqdispatch(void)
{
	unsigned int irq = sf_gic_get_int();
	if (likely(irq < GIC_NUM_INTRS)) {
		do_IRQ(MIPS_GIC_IRQ_BASE + irq);
	} else {
		pr_debug("Spurious GIC Interrupt!\n");
		spurious_interrupt();
	}

}

#if defined(CONFIG_MIPS_MT_SMP)
unsigned int
plat_ipi_call_int_xlate(unsigned int cpu)
{
	return GIC_CALL_INT(cpu);
}

unsigned int
plat_ipi_resched_int_xlate(unsigned int cpu)
{
	return GIC_RESCHED_INT(cpu);
}
#endif

static void sf16a18_ipi_irqdispatch(void)
{
#ifdef CONFIG_MIPS_GIC_IPI
	unsigned long irq;
	DECLARE_BITMAP(pending, GIC_NUM_INTRS);

	gic_get_int_mask(pending, ipi_ints);

	irq = find_first_bit(pending, GIC_NUM_INTRS);

	while (irq < GIC_NUM_INTRS) {
		do_IRQ(MIPS_GIC_IRQ_BASE + irq);

		irq = find_next_bit(pending, GIC_NUM_INTRS, irq + 1);
	}
#endif
	if (gic_compare_int())
		do_IRQ(MIPS_GIC_IRQ_BASE);
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_status() & read_c0_cause() & ST0_IM;

	if (unlikely(!pending)) {
		pr_err("Spurious CP0 Interrupt!\n");
		spurious_interrupt();
	}

	/* CPU_TIMER */
	if (pending & CAUSEF_IP7) {
		pending &= ~CAUSEF_IP7;
		do_IRQ(cp0_compare_irq);
	}

	/* IPI & GIC_TIMER */
	if (pending & (CAUSEF_IP3 | CAUSEF_IP4)) {
		pending &= ~(CAUSEF_IP3 | CAUSEF_IP4);
		sf16a18_ipi_irqdispatch();
	}

	/* other irqs */
	if (pending & CAUSEF_IP2) {
		pending &= ~CAUSEF_IP2;
		gic_irqdispatch();

	}

	/* Softirqs. This is not in use. */
	if (pending & (CAUSEF_IP0 | CAUSEF_IP1)) {
		do_IRQ((pending >> 8) + MIPS_GIC_IRQ_BASE);
		pending &= ~(CAUSEF_IP0 | CAUSEF_IP1);
	}

	/* IP5 & IP6 not used */
	if (pending != 0)
		pr_err("Unexpected interrupt, %u!\n", pending);
}

static int gic_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, irq_gic,
#if defined(CONFIG_MIPS_MT_SMP)
			(hw < GIC_RESCHED_INT(nr_cpu_ids)) ?
			handle_percpu_irq :
#endif
			handle_level_irq);

	return 0;
}

static const struct irq_domain_ops irq_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = gic_map,
};

static int __init of_gic_init(struct device_node *node,
		struct device_node *parent)
{
	struct irq_domain *domain;
	struct resource gic = { 0 };
	unsigned int gic_rev;

#if defined(CONFIG_MIPS_MT_SMP)
	int i;
#endif

	if (of_address_to_resource(node, 0, &gic))
		panic("Failed to get gic memory range");

	if (request_mem_region(gic.start, resource_size(&gic),
				gic.name) < 0)
		panic("Failed to request gic memory");

	gic_fill_map();

	if (mips_cm_present()) {
		write_gcr_gic_base(GIC_BASE_ADDR | CM_GCR_GIC_BASE_GICEN_MSK);
		gic_present = 1;
	}

	if (gic_present) {
		gic_init(gic.start, resource_size(&gic), gic_intr_map,
				ARRAY_SIZE(gic_intr_map), MIPS_GIC_IRQ_BASE);

		GICREAD(GIC_REG(SHARED, GIC_SH_REVISIONID), gic_rev);
		pr_info("gic: revision %d.%d\n", (gic_rev >> 8) & 0xff, gic_rev & 0xff);

		domain = irq_domain_add_legacy(node, GIC_NUM_INTRS, MIPS_GIC_IRQ_BASE,
				0, &irq_domain_ops, NULL);
		if (!domain)
			panic("Failed to add irqdomain");

#if defined(CONFIG_MIPS_MT_SMP)
		for (i = 0; i < nr_cpu_ids; i++) {
			setup_irq(MIPS_GIC_IRQ_BASE + GIC_RESCHED_INT(i), &irq_resched);
			setup_irq(MIPS_GIC_IRQ_BASE + GIC_CALL_INT(i), &irq_call);
		}
#endif
		change_c0_status(ST0_IM, STATUSF_IP7 | STATUSF_IP4 | STATUSF_IP3 |
				STATUSF_IP2);
	}

	return 0;
}

static struct of_device_id __initdata of_irq_ids[] = {
	{ .compatible = "mti,cpu-interrupt-controller", .data = mips_cpu_intc_init },
	{ .compatible = "siflower,sfax8-gic", .data = of_gic_init },
	{},
};

void __init
arch_init_irq(void)
{
	of_irq_init(of_irq_ids);
}
