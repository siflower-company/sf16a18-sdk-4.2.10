#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/slab.h>

//enable 32k clock
#include <linux/mfd/sfax8.h>
#include "internal.h"

#define TOTAL_DCDC	4
#define TOTAL_LDO	5

#define REGULATOR_MAX_NUM (TOTAL_DCDC + TOTAL_LDO)

struct pwr_mgmt {
	struct regulator *regulator[REGULATOR_MAX_NUM];
	u32 default_uv[REGULATOR_MAX_NUM];
	u32 num;
};

static ssize_t pwr_mgmt_write(struct file *file, const char __user *buffer,
							size_t count, loff_t *f_ops)
{
	struct regulator *data;
	unsigned int  value;
	int ret;

	sscanf(buffer, "%u", &value);

	data = (struct regulator*)(((struct seq_file *)file->private_data)->private);

	ret = regulator_set_voltage(data, value, value);
	if (ret){
		pr_err("regulator_set_voltage error\n");
		goto fin;
	}

	if (regulator_is_enabled(data) == 0){
		//regulator not enabled
		ret = regulator_enable(data);
		if (ret < 0){
			pr_err("regulator_enable error\n");
			goto fin;
		}
	}

	value = regulator_get_voltage(data);
	pr_debug("Voltage is %d\n", value);

fin:
	return count;
}

static ssize_t pwr_mgmt_read(struct file *file, char __user *buffer,
							size_t count, loff_t *f_ops)
{
	struct regulator *data;
	unsigned int  value;
	int ret, step;
	char *buff;
	char name[32];

	if(*f_ops > 0){
		return 0;
	}

	buff = kmalloc(sizeof(char) * count, GFP_KERNEL);
	if(!buff)
		return -ENOMEM;

	data = (struct regulator*)(((struct seq_file *)file->private_data)->private);

	step = data->rdev->desc->uV_step;
	strcpy(name, data->rdev->desc->name);
	value = regulator_get_voltage(data);

	pr_debug("Voltage is %d\n", value);
	ret = snprintf(buff, count, "%s's current voltage is %d uv, step is %d uv.\n", name, value, step);
	if(copy_to_user(buffer, buff, ret))
		ret = -EFAULT;
	*f_ops += ret;
	kfree(buff);
	return ret;
}

static int pwr_mgmt_show(struct seq_file *m, void *v)
{
	seq_printf(m,"Nothing to show\n");

	return 0;
}

static int pwr_mgmt_open(struct inode *inode, struct file *file)
{
	return single_open(file, pwr_mgmt_show, PDE_DATA(inode));
}

static int pwr_mgmt_status_show(struct device *dev, struct pwr_mgmt *sfax8_pm)
{
	unsigned char status;
	int ret;
	int i = 0;
	struct sfax8 *data = dev_get_drvdata(dev->parent);
	char * info[] = {
	"by PWRON assert.",
	"detecting an abnormal temperature.",
	"detecting the low power condition in VINDET circuit.",
	"by IODET asserts.",
	"by SWPWROFF bit setting, PSHOLD(GPIO*) is low or PSHOLD(GPIO*) is timeout.",
	"by the watchdog function.",
	"by the overcurrent of DCDC1-4 by the current limit circuit.",
	"by N_OE asserts or HRESET asserts.",
	};

	if(dev == NULL){
		dev_err(dev, "device is null.\n");
		return -ENOMEM;
	}
	ret = regmap_bulk_read(data->regmap, SFAX8_RN5T567_PONHIS, &status, 1);
	if(ret){
		printk("Can't get power on status\n");
	}else {
		printk("PMU power on status is 0x%x\n", status);
	}

	ret = regmap_bulk_read(data->regmap, SFAX8_RN5T567_POFFHIS, &status, 1);
	if(ret){
		printk("Can't get power off history status.\n");
	}else {
		printk("PMU power off history status is 0x%x\n", status);
		i = 0;
		while( i < 8){
			if(status & BIT(i))
				printk("PMU power-off has occurred %s\n", info[i]);
			i++;
		}
	}

	return 0;
}
static struct file_operations pwr_mgmt_ops = {
	.owner		= THIS_MODULE,
	.open		= pwr_mgmt_open,
	.read		= pwr_mgmt_read,
	.write		= pwr_mgmt_write,
	.release	= single_release,
	.llseek		= seq_lseek,
};

static int create_pwrmgmt_file(struct pwr_mgmt *sfax8_pm)
{

	struct proc_dir_entry *parent, *file;
	int i;
	parent = proc_mkdir("power-manager", NULL);
	if(!parent){
		printk(KERN_ERR "%s: can not create power-manager dir\n", __func__);
		return -ENOMEM;
	}

	for( i = 0; i < sfax8_pm->num; i++){
		file = proc_create_data(sfax8_pm->regulator[i]->rdev->desc->name, 0644, parent, &pwr_mgmt_ops, sfax8_pm->regulator[i]);
		if(!file){
			printk(KERN_ERR "%s: can not create %s file\n", __func__, sfax8_pm->regulator[i]->rdev->desc->name);
			return -ENOMEM;
		}
	}
	return 0;
}

int rn5t567_pwr_mgmt_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct regulator *rg;
	struct pwr_mgmt *sfax8_pm;
	char name[32], vname[32], full_name[32];
	char pre_name[2][8] = {"dcdc", "ldo"};
	u32 i, find;
	int err;

	np = of_get_child_by_name(pdev->dev.parent->of_node, "power-management");
	if (!np) {
		dev_err(&pdev->dev, "Device is not having regulator configuration.\n");
	}
	pdev->dev.of_node = np;

	sfax8_pm = devm_kzalloc(&pdev->dev, sizeof(struct pwr_mgmt),
			GFP_KERNEL);
	if (!sfax8_pm){
		of_node_put(np);
		dev_err(&pdev->dev, "malloc memory failed\n");
		return -ENOMEM;
	}

	for (i = 0, find = 0; i < REGULATOR_MAX_NUM; i++){
		if ( i < TOTAL_DCDC)
			sprintf(name, "%s%d", pre_name[0], i + 1);
		else
			sprintf(name, "%s%d", pre_name[1], i + 1 - TOTAL_DCDC);
		sprintf(full_name, "rn5t567-%s", name);
		dev_dbg(&pdev->dev, "Num %d : name is %s", i, name);

		rg = devm_regulator_get(&pdev->dev, full_name);
		if (IS_ERR(rg)) {
			continue;
		}
		if (!strcmp(rg->rdev->desc->name, "regulator-dummy")){
			dev_warn(&pdev->dev, "find a dummy regulator, ignore it\n");
			continue;
		}

		sfax8_pm->regulator[find] = rg;

		sprintf(vname, "%s-default-uv", full_name);
		dev_dbg(&pdev->dev, "Num %d: default uv name is %s", i, vname);

		err = of_property_read_u32(np, vname, &(sfax8_pm->default_uv[find]));
		if (!err){
			printk("%s defualt voltage is set to %u uv.\n", full_name, sfax8_pm->default_uv[find]);
			err = regulator_set_voltage(rg, sfax8_pm->default_uv[find], sfax8_pm->default_uv[find]);
			if (err){
				pr_err("Can not set %s voltage to %u uv.\n", full_name, sfax8_pm->default_uv[find]);
				return -EINVAL;
			}
		} else {
			sfax8_pm->default_uv[find] = 0;
		}
		find++;
	}
	sfax8_pm->num = find;

	err = pwr_mgmt_status_show(&pdev->dev, sfax8_pm);
	if(err)
		printk("Can't get pmu status.\n");
	if(create_pwrmgmt_file(sfax8_pm))
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(rn5t567_pwr_mgmt_probe);
