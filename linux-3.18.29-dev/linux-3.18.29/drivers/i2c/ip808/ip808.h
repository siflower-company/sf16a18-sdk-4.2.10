#ifndef IP808_H
#define IP808_H
//#define IP808DEBUG
#include <linux/regmap.h>

#define I2C_RETRYTIMES 3

#define IP808_IOC_TYPE		0xE0
#define IP808_READ   _IOC(_IOC_READ, IP808_IOC_TYPE, 0, 6)
#define IP808_WRITE  _IOC(_IOC_WRITE, IP808_IOC_TYPE, 0, 6)

struct sfax8_ip808 {
	struct device *dev;
	struct regmap *regmap;
	int chip_irq;
	unsigned long irq_flags;
	bool en_intern_int_pullup;
	bool en_intern_i2c_pullup;
	struct regmap_irq_chip_data *irq_data;
};

struct sfax8_ip808_info{
	unsigned addr;
	char name[20];
	const struct regmap_config *config;	
};



static const struct regmap_range sfax8_ip808_readable_ranges[] = {
	regmap_reg_range(0x0, 0xFF),
};

static const struct regmap_access_table sfax8_ip808_readable_table = {
	.yes_ranges = sfax8_ip808_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(sfax8_ip808_readable_ranges),
};

static const struct regmap_range sfax8_ip808_writable_ranges[] = {
	regmap_reg_range(0x0, 0xFF),
};

static const struct regmap_access_table sfax8_ip808_writable_table = {
	.yes_ranges = sfax8_ip808_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(sfax8_ip808_writable_ranges),
};

static const struct regmap_config sfax8_ip808_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
	.cache_type = REGCACHE_NONE,
	.rd_table = &sfax8_ip808_readable_table,
	.wr_table = &sfax8_ip808_writable_table,
//	.volatile_table = &sfax8_ip808_volatile_table,
};

//extern void ip808_mdio_wr(unsigned short pa, unsigned short ra, unsigned short va);
//extern unsigned short ip808_mdio_rd(unsigned short pa, unsigned short ra);
extern int PoE_i2c_write(int device, int write_addr, int write_value);
extern int PoE_i2c_read(int device, int read_addr, unsigned char *value);
#endif		/* IP1829_H */
