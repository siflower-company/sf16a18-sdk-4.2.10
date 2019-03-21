#ifndef __ASM_MACH_MIPS_IRQ_H
#define __ASM_MACH_MIPS_IRQ_H

#define CPU_HW_INTRS (8)
#define GIC_NUM_INTRS (256)
#define GPIO_NUM_INTRS (64)
#define NR_IRQS (CPU_HW_INTRS + GIC_NUM_INTRS + GPIO_NUM_INTRS)

#include_next <irq.h>

#endif /* __ASM_MACH_MIPS_IRQ_H */
