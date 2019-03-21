struct sfax8_regulators;
struct clk_32k_ops{
	int (*enable)(struct device *);
	int (*disable)(struct device *);
	struct sfax8_regulators *sf_rg;
};
struct sfax8_register_mapping {
	u8 regulator_id;
	const char *name;
	const char *sname;
	u8 vsel_reg;
	u8 vsel_mask;
	int n_voltages;
	u32 enable_reg;
	u8 enable_mask;
	u32 control_reg;
	u8 mode_mask;
	u32 sleep_ctrl_reg;
	u8 sleep_ctrl_mask;
	int sf_pmu_current;
};

/*
struct sfax8_regulator_config_data {
	struct regulator_init_data *reg_init;
	bool enable_tracking;
	int ext_control;
};
*/

struct sfax8_regulators {
	struct device *dev;
	struct sfax8 *sfax8;
	struct regulator_dev **rdevs;
	struct regulator_desc *desc;
	struct of_regulator_match *of_match;
	int regulator_num;
};

extern int disable_clk_32k(void);
extern int enable_clk_32k(void);
