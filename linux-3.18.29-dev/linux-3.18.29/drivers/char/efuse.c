#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <asm/delay.h>

static char *efuse_data;
#define EFUSE_MAJOR		231	
static ssize_t efuse_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos);
struct file_operations efuse_fops ={
	owner: THIS_MODULE,
    read:  efuse_read,
};


static ssize_t efuse_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	int ret = 0;
	int size = 0;
	if(efuse_data == NULL)	
		return 0;
	size = strlen(efuse_data);
	if(*ppos == size )
		return 0;
	if(*ppos){
		ret = (size - *ppos) > count ? count : (size - *ppos);
		copy_to_user(buf, efuse_data + *efuse_data, ret);
	}else{
		ret = size > count ? count : size;
		copy_to_user(buf, efuse_data, ret);
	}
	*ppos += ret;
	return ret;
}
static char *efuse_devnode(struct device *dev, umode_t *mode)
{
		
	*mode = (umode_t)0666;
	return NULL;
}

static int test_init(void)
{
	static struct class *efuse_class;	
	if(register_chrdev(EFUSE_MAJOR, "efuse", &efuse_fops))
		printk("unable to get major %d for efuse devs\n", EFUSE_MAJOR);
	efuse_class = class_create(THIS_MODULE, "efuse");
	efuse_class->devnode = efuse_devnode;
	if (IS_ERR(efuse_class))
		return PTR_ERR(efuse_class);
	device_create(efuse_class, NULL, MKDEV(EFUSE_MAJOR, 1),
			      NULL, "efuse");
	return 0;
}

static void test_exit(void)
{
}


static int __init parse_efusedata(char *s)
{
	efuse_data = s;
	return 0;
}
__setup("efusedata=", parse_efusedata);

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");
