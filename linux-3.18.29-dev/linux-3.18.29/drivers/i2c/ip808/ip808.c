#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
//**************************************

#define IP808DEBUG

#include "ip808.h"
#include "ip808lib.h"
//MODULE_LICENSE("Dual BSD/GPL");

//#define IP808_GPIO_I2C

//**************************************
//I2C
//**************************************
#define IP3210_GPIO

#ifdef IP3210_GPIO
#define GPREG(reg) (*(unsigned long*)reg)
#define I2C_DELAY	1000
#define I2C_SCL		8
#define I2C_SDA_I	11
#define I2C_SDA_O	16

#define GPVAL		0xA3005000
#define GPSEL		0xA3005010
#define GPDIR		0xA300500C
#endif

static struct sfax8_ip808 *sfax8;

unsigned int old_reg_ip808=0;
void ic_i2c_init(void)//enable gpio
{
#ifdef IP3210_GPIO
	unsigned int reg;  
	reg=GPREG(GPSEL);
	reg |= (0x01<<I2C_SDA_I) | (0x01<<I2C_SDA_O);
	reg |= (0x1<<I2C_SCL);
	GPREG(GPSEL)=reg;
#endif	
}

void i2c_set_direction(void)//set scl/sda_o output, sda_i input
{
#ifdef IP3210_GPIO
	unsigned int reg;
	reg=GPREG(GPDIR);
	reg |= ((0x01<<I2C_SDA_O)|(0x01<<I2C_SCL));
	reg &= ~(0x1<<I2C_SDA_I);
	GPREG(GPDIR)=reg;
	
	old_reg_ip808 = (0x01<<I2C_SCL)|(0x01<<I2C_SDA_O);
	reg=GPREG(GPVAL);
	reg |= old_reg_ip808;
	GPREG(GPVAL)=reg;
#endif	
}

void i2c_set_SCL_1(void)
{
#ifdef IP3210_GPIO
	unsigned int reg;
	old_reg_ip808 &= ((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));
	old_reg_ip808 |= (0x01<<I2C_SCL);  
	reg=GPREG(GPVAL);   
	reg &= ~((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));	
	reg |= old_reg_ip808;
	GPREG(GPVAL)=reg;
#endif	
}
void i2c_set_SCL_0(void)
{
#ifdef IP3210_GPIO
	unsigned int reg;  
	old_reg_ip808 &= ((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));
	old_reg_ip808 &= ~(0x01<<I2C_SCL); 
	reg=GPREG(GPVAL);
	reg &= ~((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));		
	reg |= old_reg_ip808;
	GPREG(GPVAL)=reg;
#endif	
}
void i2c_set_SDAO_1(void)
{
#ifdef IP3210_GPIO
	unsigned int reg;  
	old_reg_ip808 &= ((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));
	old_reg_ip808 |= (0x01<<I2C_SDA_O);  
	reg=GPREG(GPVAL); 
	reg &= ~((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));		
	reg |= old_reg_ip808;
	GPREG(GPVAL)=reg;
#endif
}
void i2c_set_SDAO_0(void)
{
#ifdef IP3210_GPIO
	unsigned int reg; 
	old_reg_ip808 &= ((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));
	old_reg_ip808 &= ~(0x01<<I2C_SDA_O); 
	reg=GPREG(GPVAL); 
	reg &= ~((0x01<<I2C_SCL)|(0x01<<I2C_SDA_O));	
	reg |= old_reg_ip808;
	GPREG(GPVAL)=reg;
#endif	
}
unsigned int i2c_get_SDAI_value(void)
{
#ifdef IP3210_GPIO
	if(GPREG(GPVAL)&(0x01<<I2C_SDA_I))
	{
		return 0x1;
	}
	else 
	{
		return 0x0;
	}
#endif	
}


#define NOACK 1
#define ACK 0

void PoE_i2c_wait254(void)
{
  int i;
	for(i=0;i<I2C_DELAY;i++);
}

void PoE_i2c_wait127(void)
{
  int i;
	for(i=0;i<(I2C_DELAY/2);i++);
}

void PoE_i2c_start(void)
{
	PoE_i2c_wait254();
	PoE_i2c_wait254();
	i2c_set_SDAO_1();//I2C_SDA_O=HIGH;
	i2c_set_SCL_1();//I2C_SCL=HIGH;
	PoE_i2c_wait254();
	i2c_set_SDAO_0();//I2C_SDA_O=LOW;
	PoE_i2c_wait254();
	PoE_i2c_wait254();	
	i2c_set_SCL_0();//I2C_SCL=LOW;
	PoE_i2c_wait254();
}

void PoE_i2c_stop(void)
{
	PoE_i2c_wait254();
	PoE_i2c_wait254();
	i2c_set_SDAO_0();//I2C_SDA_O=LOW;
	PoE_i2c_wait254();
	i2c_set_SCL_1();//I2C_SCL=HIGH;
	PoE_i2c_wait127();
	i2c_set_SDAO_1();//I2C_SDA_O=HIGH;
}

int PoE_i2c_write_byte(unsigned char value)
{
	unsigned char i=8, ack=0;

	while(i--)
	{
		//I2C_SDA_O=(value&0x80)?HIGH:LOW;
		if(value&0x80)
			i2c_set_SDAO_1();
		else
			i2c_set_SDAO_0();		  

		PoE_i2c_wait127();
		i2c_set_SCL_1();//I2C_SCL=HIGH;
		value<<=1;
		PoE_i2c_wait254();
		i2c_set_SCL_0();//I2C_SCL=LOW;
		PoE_i2c_wait127();		
	}

	PoE_i2c_wait127();
	i2c_set_SCL_1();//I2C_SCL=HIGH;
	PoE_i2c_wait254();
	ack=i2c_get_SDAI_value();//I2C_SDA_I;
	PoE_i2c_wait127();
	i2c_set_SCL_0();//I2C_SCL=LOW;
	PoE_i2c_wait254();

	return ack;
}

int PoE_i2c_read_byte(unsigned char ack)
{
	unsigned int i=8,value;

	value=0;
	while(i--)
	{
		value<<=1;
		PoE_i2c_wait127();
		i2c_set_SCL_1();//I2C_SCL=HIGH;
		PoE_i2c_wait254();
		value|=i2c_get_SDAI_value();//I2C_SDA_I;
		PoE_i2c_wait127();
		i2c_set_SCL_0();//I2C_SCL=LOW;
		PoE_i2c_wait127();
	}
	//I2C_SDA_O=ack;
	if(ack)
    i2c_set_SDAO_1();
  else
    i2c_set_SDAO_0();
	PoE_i2c_wait127();
	i2c_set_SCL_1();//I2C_SCL=HIGH;
	PoE_i2c_wait254();
	i2c_set_SCL_0();//I2C_SCL=LOW;
	PoE_i2c_wait127();
	//I2C_SDA_I=HIGH;	
	PoE_i2c_wait254();
	return value;
}


int PoE_i2c_write(int device, int write_addr, int write_value)
{
#if 0
	int i=0;
	device<<=1; //shift for R/W Bit
	
	ic_i2c_init();
	i2c_set_direction();
	
	for(i=0;i<I2C_RETRYTIMES;i++)
	{
		i2c_set_SDAO_1();//I2C_SDA_O = HIGH;
		PoE_i2c_start();
		if(PoE_i2c_write_byte(device&0xFE)){PoE_i2c_stop();continue;} //write operation
		if(PoE_i2c_write_byte(write_addr&0xFF)){PoE_i2c_stop();continue;}			//LSB
		if(PoE_i2c_write_byte(write_value&0xFF)){PoE_i2c_stop();continue;}			
		PoE_i2c_stop();
		break;
	}
	i2c_set_SDAO_1();//I2C_SDA_O = HIGH;
	if(i==I2C_RETRYTIMES)
	{
		return ERROR;
	}
	else
	{
		return OK;
	}
#else
	return regmap_bulk_write(sfax8->regmap, write_addr, &write_value, 1);
#endif
}

int PoE_i2c_read(int device, int read_addr, unsigned char *value)
{
#if 0
	int i=0;
	device<<=1; //shift for R/W Bit

	ic_i2c_init();
	i2c_set_direction();
	
	for(i=0;i<I2C_RETRYTIMES;i++)
	{
		i2c_set_SDAO_1();//I2C_SDA_O = HIGH;
		PoE_i2c_start();
		if(PoE_i2c_write_byte(device&0xFE)){PoE_i2c_stop();continue;} //write operation
		if(PoE_i2c_write_byte(read_addr&0xFF)){PoE_i2c_stop();continue;}
		PoE_i2c_start();
		if(PoE_i2c_write_byte(device|0x1)){PoE_i2c_stop();continue;} //read operation
		*value = PoE_i2c_read_byte(NOACK);//<<8;		//MSB
		PoE_i2c_stop();
		break;
	}
	i2c_set_SDAO_1();//I2C_SDA_O = HIGH;
	if(i==I2C_RETRYTIMES)
	{
		return ERROR;
	}
	else
	{
		return OK;	
	}
#else
	return regmap_bulk_read(sfax8->regmap, read_addr, value, 1);
#endif
}





static struct cdev ip808_cdev;
static int ip808_major = 247;
static DEFINE_MUTEX(ip808_mutex);

#define IP808_NAME	"ip808_cdev"


static int ip808_open(struct inode *inode, struct file *fs)
{
#ifdef IP808DEBUG
	printk("ip808: open...\n");
#endif
	try_module_get(THIS_MODULE);

	return 0;
}

static int ip808_release(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
#ifdef IP808DEBUG
	printk("ip808: release!\n");
#endif
	return 0;
}

static ssize_t ip808_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
	return 0;
}

static ssize_t ip808_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	return 0;
}

static int ip808_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	unsigned long len, rwcmd;
	unsigned short regaddr,regdata,ip808_addr;
	unsigned char readdata;
	void *cptr;
	char *cdata;
	int ret=0x0;

#ifdef IP808DEBUG
	printk(KERN_ALERT "ip808: +ioctl...\n");
#endif
	len = (int)(_IOC_SIZE(cmd));
	rwcmd = (int)(_IOC_DIR(cmd));
	cptr = (void *)arg;
  

		cdata = kmalloc(len, GFP_KERNEL);
		if (!cdata)
		{
			ret = -ENOMEM;
			goto out_ip808_ioctl;
		}

		if (copy_from_user(cdata, cptr, len))
		{
			ret = -EFAULT;
			goto out_ip808_ioctl;
		}
		ip808_addr = *((unsigned short *)(cdata));
		regaddr = *((unsigned short *)(cdata+2));
		regdata = *((unsigned short *)(cdata+4));

#ifdef IP808DEBUG
		printk(KERN_ALERT "regaddr=0x%04x  regdata=0x%04x\n", (unsigned short)regaddr,(unsigned short)regdata);
#endif
    if(regaddr>0xff)
    {
      ret = -EINVAL;
      goto out_ip808_ioctl;
    }
    switch(cmd)
    {
        case IP808_READ:     
			ret = PoE_i2c_read(ip808_addr,regaddr,&readdata);
			printk(KERN_ALERT "PoE_i2c_read ret = %d\n", ret);
			*((unsigned short *)(cdata+4)) = (unsigned short)readdata;
#ifdef IP808DEBUG
		printk(KERN_ALERT "IP808_READ regaddr=0x%04x  regdata=0x%04x\n", (unsigned short)regaddr,(unsigned short)readdata);
#endif   			
		ret = copy_to_user((void __user *)cptr, cdata, len);
		printk(KERN_ALERT "copy to user ret = 0x%x\n", ret);
		if(ret)
		{
			ret = -EFAULT;
			goto out_ip808_ioctl;
		}
			break;
        case IP808_WRITE:
#ifdef IP808DEBUG
    printk(KERN_ALERT "IP808_WRITE regaddr=0x%04x  regdata=0x%04x\n", (unsigned short)regaddr,(unsigned short)regdata);  
#endif 
          ret = PoE_i2c_write(ip808_addr,regaddr,regdata);         
		  printk(KERN_ALERT "PoE_i2c_write ret = %d\n", ret);
          break;
    }

	//cptr= (void *)*((unsigned long *)cdata);
	//len = *((unsigned long *)(cdata+4));

	kfree(cdata);
	cdata = NULL;
		
out_ip808_ioctl:
	if(cdata)
	{
		//memset(cdata, 0x0, len);
		kfree(cdata);
	}
#ifdef IP808DEBUG
	printk(KERN_ALERT "ip808: -ioctl...\n");
#endif
	return (ret < 0) ? ret : 0;
}

static long ip808_unlocked_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&ip808_mutex);
	ret = ip808_ioctl(filep, cmd, arg);
	mutex_unlock(&ip808_mutex);

	return ret;
}

static struct file_operations ip808_fops = {
	.owner			= THIS_MODULE,
	.read			= ip808_read, 
	.write			= ip808_write,
	.unlocked_ioctl	= ip808_unlocked_ioctl,
	.open			= ip808_open,
	.release		= ip808_release
};

static int __init ip808_init(void)
{
	int result;

  //lut_set_hash_algorithm(LUT_HASH_DIRECT);
	result = register_chrdev_region(MKDEV(ip808_major, 0), 1, IP808_NAME);
	if (result < 0)
	{
    printk(KERN_WARNING "ip808: can't get major %d\n", ip808_major);
    return result;
	}

	cdev_init(&ip808_cdev, &ip808_fops);
	ip808_cdev.owner = THIS_MODULE;
	result = cdev_add(&ip808_cdev, MKDEV(ip808_major, 0), 1);
	
	if (result)
	{
		printk(KERN_WARNING "ip808: error %d adding driver\n", result);
		return result;
	}
	else
	{
		printk("ip808: driver loaded!\n");
 		return 0;
	}
}

static void __exit ip808_exit(void)
{
	cdev_del(&ip808_cdev);
	unregister_chrdev_region(MKDEV(ip808_major, 0), 1);
	printk("ip808: driver unloaded!\n");
}  

//module_init(ip808_init);
//module_exit(ip808_exit);



static struct sfax8_ip808_info ip808_infos[] = {
	{
		.addr = 0x54,
		.name = "IP808",
		.config = &sfax8_ip808_regmap_config,
	},
};


static int sfax8_ip808_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	int ret;

	ip808_init();

	sfax8 = devm_kzalloc(&i2c->dev, sizeof(struct sfax8_ip808), GFP_KERNEL);
	if (!sfax8)
		return -ENOMEM;
	
	
	sfax8->dev = &i2c->dev;
	sfax8->chip_irq = i2c->irq;
	i2c_set_clientdata(i2c, sfax8);

	dev_dbg(sfax8->dev, "start regmap init\n");

	sfax8->regmap = devm_regmap_init_i2c(i2c, ip808_infos[0].config);
	if (IS_ERR(sfax8->regmap)) {
		ret = PTR_ERR(sfax8->regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	IP808_init(0x54);

	return 0;
}

static int sfax8_ip808_remove(struct i2c_client *i2c)
{

	ip808_exit();

	return 0;
}

static const struct of_device_id sfax8_of_match[] = {
	{ .compatible = "siflower, sfax8-ip808", },
	{},
};
MODULE_DEVICE_TABLE(of, sfax8_of_match);

static const struct i2c_device_id sfax8_ip808_id[] = {
	{ "sfax8", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sfax8_ip808_id);

static struct i2c_driver sfax8_ip808_driver = {
	.driver = {
		.name = "sfax8-ip808",
		.owner = THIS_MODULE,
		.of_match_table = sfax8_of_match,
	},
	.probe = sfax8_ip808_probe,
	.remove = sfax8_ip808_remove,
	.id_table = sfax8_ip808_id,
};

module_i2c_driver(sfax8_ip808_driver);

MODULE_DESCRIPTION("I2C support for IP808");
MODULE_AUTHOR("Mark cai <mark.cai@siflower.com.cn>");
MODULE_LICENSE("GPL");

