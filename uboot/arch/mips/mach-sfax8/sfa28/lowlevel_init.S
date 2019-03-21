#include <asm-offsets.h>
#include <config.h>
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>

	.text
	.set noreorder
	.set mips32
	.globl  lowlevel_init

ENTRY(lowlevel_init)


#if defined(CONFIG_SPL_BUILD)
	PTR_LA  t9, ddr_init
	jalr    t9
	nop
#endif
	jr ra
	nop

	END(lowlevel_init)

