#ifndef __PAD_H__
#define __PAD_H__
#include <types.h>

#define FUNC_SW_SEL (1 << 3)
#define FMUX_SEL	(1 << 2)
#define MODE_BIT1	(1 << 1)
#define MODE_BIT0	(1 << 0)

#define SW_OEN		(1 << 7)
#define SW_ST		(1 << 6)
#define SW_IE		(1 << 5)
#define SW_PD		(1 << 4)
#define SW_PU		(1 << 3)
#define SW_DS2		(1 << 2)
#define	SW_DS1		(1 << 1)
#define SW_DS0		(1 << 0)

#define PAD_BASE            (SIFLOWER_SYSCTL_BASE+0x30000)
#define PAD_IO_BASE			(PAD_BASE + 0xFC00)
#define PAD_IO_REG1(n)		(PAD_IO_BASE + 0x8*n)
#define PAD_IO_REG2(n)		(PAD_IO_BASE + 0x8*n + 0x4)
//#define PAD_FUCN_SEL        (PAD_BASE+0xFF60)
//#define PAD_MODE_SEL        (PAD_BASE+0xFC00)
//#define PAD_OFFSET          (4)
#define PAD_PER_GROUP_PINS  (4)
#define PAD_INDEX_MAX 48
#define PAD_INDEX_INVALID 0xFF
#define PAD_IRQ_PER_GROUP 16

//#define PAD_IO_PULLUP(n)	(PAD_BASE + 0xFDE0 + (n)*(0x4))
//#define PAD_IO_PULLDOWN(n)	(PAD_BASE + 0xFE40 + (n)*(0x4))

#define GPIO_BASE		    (SIFLOWER_GPIO_BASE)
#define GPIO_RDAT(n)		(GPIO_BASE + (n)*(0x40) + 0x0)
#define GPIO_WDAT(n)		(GPIO_BASE + (n)*(0x40) + 0x4)
#define GPIO_DIR(n)         (GPIO_BASE + (n)*(0x40) + 0x8)

#define GPIO_INTMSK(n)		(GPIO_BASE + (n)*(0x40) + 0xC)
#define GPIO_INTGMSK(n)		(GPIO_BASE + (n)*(0x40) + 0x10)
#define GPIO_INTPEND(n)		(GPIO_BASE + (n)*(0x40) + 0x14)
#define GPIO_INTTYPE(n)		(GPIO_BASE + (n)*(0x40) + 0x18)
#define GPIO_FILTER(n)		(GPIO_BASE + (n)*(0x40) + 0x1C)
#define GPIO_CLKDIV(n)		(GPIO_BASE + (n)*(0x40) + 0x20)
#define GPIO_INTPENDGLB(n)	(GPIO_BASE + (n)*4 + 0x4000)

typedef enum pad_trigger_type_t
{
	EINT_TRIGGER_LOW =0,
	EINT_TRIGGER_HIGH,
	EINT_TRIGGER_FALLING = 3,
	EINT_TRIGGER_RISING = 5,
	EINT_TRIGGER_DOUBLE = 7
} pad_trigger_type;


typedef enum pad_func_t{
	FUNC0,
	FUNC1,
	FUNC2,
	GPIO_INPUT,
	GPIO_OUTPUT
} pad_func;

typedef enum pad_io_t{
	INPUT,
	OUTPUT,
	TRISTATE
} pad_io;

typedef enum pad_pull_t{
	NOPULL,
	PULLUP,
	PULLDOWN
} pad_pull;

typedef enum pad_output_levet_t{
	LOW_LEVEL,
	HIGH_LEVEL,
} pad_output_level;

typedef enum pad_group_t{
	group0,
	group1,
	group2,
	group3
} pad_pad_group;

typedef struct pad_index_t {
	u16 high;
	u16 low;
	pad_func func;
} pad_index;

typedef enum sf_module_t{
	SF_SPI1,
	SF_UART0,
	SF_I2C0,
	SF_I2C1,
	SF_SPI2,
	SF_UART1,
	SF_ETH,
	SF_JTAG,
	SF_PWM
} sf_module;


extern int sf_pad_set_func(u32 index, pad_func func);
extern pad_func sf_pad_get_func(u32 index);
extern int sf_pad_set_pull(u32 index, pad_pull pull);
extern pad_pull sf_pad_get_pull(u32 index);
extern int sf_pad_set_value(u32 index, pad_output_level level);
extern u32 sf_pad_get_value(u32 index);

extern int sf_pad_to_irq(u32 index);
extern int sf_irq_to_pad(u32 irq);
extern int sf_pad_irq_config(u32 index, pad_trigger_type trigger, u16 filter);
extern int sf_pad_irq_pending(u32 index);
extern int sf_pad_irq_clear(u32 index);
extern int sf_pad_irq_mask(u32 index, int mask);

extern int sf_module_set_pad_func(sf_module module);
extern void sf_init_pad_status(void);

extern int sf_sw_sel_set(u32 index , u8 value);
extern int sf_sw_oen_set(u32 index , u8 value);
#endif
