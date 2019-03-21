/*
* ccm3310 spi driver for mtk
*/

#include "spi_ccm3310.h"

#define    WAKE_TIME                        4
#define    WAIT_WAKEUP_TIME                  2000
#define    GPIO_OUT_ZERO					0
#define    GPIO_OUT_ONE						1

static void mt_set_gpio_out(unsigned gpio, int value)
{
	gpio_set_value(gpio, value);
}

static int mt_get_gpio_in(unsigned gpio)
{
	u32 ret;

	ret = gpio_get_value(gpio);

	return ret;
}

void ccm3310_spi_wakeup(int count)
{
	int i = 0;

	mt_set_gpio_out(GPIO_SPI_WAKEUP_GPIO, GPIO_OUT_ZERO);

	for(i = 0; i< count; i++)
	{
		udelay(40);
	}

	mt_set_gpio_out(GPIO_SPI_WAKEUP_GPIO,  GPIO_OUT_ONE);

	udelay(WAIT_WAKEUP_TIME);

	return;
}

int ccm3310_spi_wake(int count)
{
	int try_count = 0;

	while(0 != mt_get_gpio_in(GPIO_SPI_RB_GPIO))// check busy
	{
		CCM3310_SPI_INFO("ccm3310_spi_wake...............\n");

		if(try_count)
			mdelay(10);

		ccm3310_spi_wakeup(count);

		mdelay(2);

		if(try_count)
			CCM3310_SPI_ERR("ccm3310_spi_wake.......reset again ..,try_count = %d\n",try_count);

		if(try_count++ > 6)
		{   
			CCM3310_SPI_ERR("Wake failed\n");            
			return -1;
		}
	};
	return 0;
}

static int ccm3310_wait_ready(void)
{
	unsigned int time = 0;

	//CCM3310_SPI_TRACE;

	CCM3310_SPI_INFO("ccm3310_wait_ready in........\n");

	while(0 != mt_get_gpio_in(GPIO_SPI_RB_GPIO))// check busy
	{
		time++;
		udelay(1);

		if(time>20000000)	//add delay for RSA 2048 Gen key
		{
			CCM3310_SPI_INFO("wait busy timeout!!\n");
			if(ccm3310_spi_wake(WAKE_TIME))
				return -1; 
		}
	}
	if(time > 0)
	{
		CCM3310_SPI_INFO("wait busy %d us....\n", time);
	}

	return 0;
}

static void ccm3310_set_spiport(void)
{
	CCM3310_SPI_TRACE;
#if 0
	mt_set_gpio_mode(GPIO_SPI_CS_PIN, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO_SPI_SCK_PIN, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO_SPI_MISO_PIN, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO_SPI_MOSI_PIN, GPIO_MODE_01);
#endif

	return;
}

static void ccm3310_set_gpio(void)
{
	CCM3310_SPI_TRACE;
#if 0
	mt_set_gpio_mode(GPIO_SPI_RB_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_SPI_RB_GPIO, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_SPI_RB_GPIO, GPIO_PULL_ENABLE);

	mt_set_gpio_mode(GPIO_SPI_WAKEUP_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_SPI_WAKEUP_GPIO, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(GPIO_SPI_WAKEUP_GPIO, GPIO_PULL_ENABLE);
	mt_set_gpio_out(GPIO_SPI_WAKEUP_GPIO, GPIO_OUT_ONE);
#endif
	gpio_request(GPIO_SPI_RB_GPIO, "RB_GPIO");
	gpio_direction_input(GPIO_SPI_RB_GPIO);

	gpio_request(GPIO_SPI_WAKEUP_GPIO, "WAKEUP_GPIO");
	gpio_direction_output(GPIO_SPI_WAKEUP_GPIO, 1);

	return;
}


static ssize_t spi_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi;
	struct mt_chip_conf *chip_config;
	int len;

	spi = container_of(dev, struct spi_device, dev);
	chip_config = (struct mt_chip_conf *) spi->controller_data;

	CCM3310_SPI_INFO("test show chip_config:%lx\n", (unsigned long)chip_config);

	if (!chip_config) 
	{
		CCM3310_SPI_INFO("chip_config is NULL.\n");
		chip_config = kzalloc (sizeof(struct mt_chip_conf), GFP_KERNEL);
		if (!chip_config)
			return -ENOMEM;
	}

	len = snprintf(buf, PAGE_SIZE, 
			" setuptime: %d\n holdtime: %d \
			\n high_time: %d\n low_time: %d \
			\n cs_idletime: %d\n ulthgh_thrsh: %d \
			\n cpol: %d\n cpha: %d \
			\n tx_mlsb: %d\n rx_mlsb: %d \
			\n tx_endian: %d\n rx_endian: %d \
			\n com_mod: %d\n pause: %d \
			\n finish_intr: %d\n deassert: %d \
			\n ulthigh: %d\n tckdly: %d deassert: %d\n", 
			chip_config->setuptime, chip_config->holdtime,
			chip_config->high_time, chip_config->low_time,
			chip_config->cs_idletime, chip_config->ulthgh_thrsh,
			chip_config->cpol, chip_config->cpha,
			chip_config->tx_mlsb, chip_config->rx_mlsb,
			chip_config->tx_endian, chip_config->rx_endian,
			chip_config->com_mod, chip_config->pause,
			chip_config->finish_intr, chip_config->deassert,
			chip_config->ulthigh, chip_config->tckdly,
			chip_config->deassert);
	return (len >= PAGE_SIZE) ? (PAGE_SIZE - 1) : len;
}

static ssize_t spi_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device *spi;
	//int i,rest_time;
	struct mt_chip_conf *chip_config;

	u32 setuptime, holdtime, high_time, low_time;
	u32 cs_idletime, ulthgh_thrsh;
	int cpol, cpha,tx_mlsb, rx_mlsb, tx_endian;
	int rx_endian, com_mod, pause, finish_intr;
	int deassert, tckdly, ulthigh;

	spi = container_of(dev, struct spi_device, dev);

	CCM3310_SPI_INFO("SPIDEV name is:%s\n", spi->modalias);
	CCM3310_SPI_INFO("==== buf:%s\n", buf);

	chip_config = (struct mt_chip_conf *) spi->controller_data;

	CCM3310_SPI_INFO("test store chip_config:%lx\n", (unsigned long)chip_config);

	if (!chip_config) 
	{
		CCM3310_SPI_INFO( "chip_config is NULL.\n");
		return -ENOMEM;
	}	

	if (!strncmp(buf, "-h", 2 ) ) 
	{
		CCM3310_SPI_INFO("Please input the parameters for this device.\n");
	}
	else if ( !strncmp(buf, "-w", 2 ) ) 
	{
		buf += 3;
		if (!buf) 
		{
			CCM3310_SPI_INFO("buf is NULL.\n");
			goto out;
		}
		if (!strncmp(buf, "setuptime=", 10) && (1 == sscanf(buf + 10, "%d", &setuptime))) 
		{
			CCM3310_SPI_INFO("setuptime is:%d\n", setuptime);
			chip_config->setuptime = setuptime;
		}
		else if (!strncmp(buf, "holdtime=", 9)&&(1 == sscanf(buf + 9, "%d", &holdtime))) 
		{
			CCM3310_SPI_INFO("Set holdtime is:%d\n", holdtime);
			chip_config->holdtime = holdtime;	
		}
		else if (!strncmp(buf, "high_time=", 10)&&(1 == sscanf(buf + 10, "%d", &high_time))) 
		{
			CCM3310_SPI_INFO("Set high_time is:%d\n", high_time);
			chip_config->high_time = high_time;	
		}
		else if (!strncmp(buf, "low_time=", 9)&&(1 == sscanf(buf + 9, "%d", &low_time))) 
		{
			CCM3310_SPI_INFO("Set low_time is:%d\n", low_time);
			chip_config->low_time = low_time;
		}
		else if (!strncmp(buf, "cs_idletime=", 12)&&(1 == sscanf(buf + 12, "%d", &cs_idletime))) 
		{
			CCM3310_SPI_INFO("Set cs_idletime is:%d\n", cs_idletime);
			chip_config->cs_idletime = cs_idletime;	
		}
		else if (!strncmp(buf, "ulthgh_thrsh=", 13)&&(1 == sscanf(buf + 13, "%d", &ulthgh_thrsh))) 
		{
			CCM3310_SPI_INFO("Set slwdown_thrsh is:%d\n", ulthgh_thrsh);
			chip_config->ulthgh_thrsh = ulthgh_thrsh; 
		}
		else if (!strncmp(buf, "cpol=", 5) && (1 == sscanf(buf + 5, "%d", &cpol)))
		{
			CCM3310_SPI_INFO("Set cpol is:%d\n", cpol);
			chip_config->cpol = cpol;
		}
		else if (!strncmp(buf, "cpha=", 5) && (1 == sscanf(buf + 5, "%d", &cpha))) 
		{
			CCM3310_SPI_INFO("Set cpha is:%d\n", cpha);
			chip_config->cpha = cpha;
		}
		else if (!strncmp(buf, "tx_mlsb=", 8)&&(1 == sscanf(buf + 8, "%d", &tx_mlsb))) 
		{
			CCM3310_SPI_INFO("Set tx_mlsb is:%d\n", tx_mlsb);
			chip_config->tx_mlsb = tx_mlsb;	
		}
		else if (!strncmp(buf, "rx_mlsb=", 8)&&(1 == sscanf(buf + 8, "%d", &rx_mlsb))) 
		{
			CCM3310_SPI_INFO("Set rx_mlsb is:%d\n", rx_mlsb);
			chip_config->rx_mlsb = rx_mlsb;	
		}
		else if (!strncmp(buf, "tx_endian=", 10)&&(1 == sscanf(buf + 10, "%d", &tx_endian))) 
		{
			CCM3310_SPI_INFO("Set tx_endian is:%d\n", tx_endian);
			chip_config->tx_endian = tx_endian;	
		}
		else if (!strncmp(buf, "rx_endian=", 10)&&(1 == sscanf(buf + 10, "%d", &rx_endian))) 
		{
			CCM3310_SPI_INFO("Set rx_endian is:%d\n", rx_endian);
			chip_config->rx_endian = rx_endian;	
		}
		else if (!strncmp(buf, "com_mod=", 8)&&(1 == sscanf(buf + 8, "%d", &com_mod))) 
		{
			chip_config->com_mod = com_mod;
			CCM3310_SPI_INFO("Set com_mod is:%d\n", com_mod);
		}
		else if (!strncmp(buf, "pause=", 6)&&(1 == sscanf(buf + 6, "%d", &pause))) 
		{
			CCM3310_SPI_INFO("Set pause is:%d\n", pause);
			chip_config->pause = pause;
		}
		else if (!strncmp(buf, "finish_intr=", 12)&&(1==sscanf(buf + 12, "%d", &finish_intr))) 
		{
			CCM3310_SPI_INFO("Set finish_intr is:%d\n", finish_intr);
			chip_config->finish_intr = finish_intr;
		}
		else if (!strncmp(buf, "deassert=", 9)&&(1 == sscanf(buf + 9, "%d", &deassert))) 
		{
			CCM3310_SPI_INFO("Set deassert is:%d\n", deassert);
			chip_config->deassert = deassert;	
		}
		else if (!strncmp(buf, "ulthigh=", 8 ) && ( 1 == sscanf(buf + 8, "%d", &ulthigh))) 
		{
			CCM3310_SPI_INFO("Set ulthigh is:%d\n", ulthigh);	
			chip_config->ulthigh = ulthigh;
		}
		else if (!strncmp(buf, "tckdly=",7) && ( 1 == sscanf(buf + 7, "%d", &tckdly))) 
		{
			CCM3310_SPI_INFO("Set tckdly is:%d\n", tckdly);
			chip_config->tckdly = tckdly;
		}	
		else 
		{
			CCM3310_SPI_INFO("Wrong parameters.\n");
			goto out;
		}
		spi->controller_data = chip_config;

	}
out:
	return count;
}


static DEVICE_ATTR(spi, 0660, spi_show, spi_store);
static struct device_attribute *spi_attribute[]={
	&dev_attr_spi,
};

static int spi_create_attribute(struct device *dev)
{
	int num,idx;
	int err =0;

	CCM3310_SPI_INFO("dev name %s\n", dev->kobj.name);

	num = (int)(sizeof(spi_attribute)/sizeof(spi_attribute[0]));

	for (idx = 0; idx < num; idx ++) {

		if ((err = device_create_file(dev, spi_attribute[idx])))
			break;

	}	
	return err;

}

static void spi_remove_attribute(struct device *dev)
{
	int num, idx;
	CCM3310_SPI_INFO("dev remove name %s\n", dev->kobj.name);
	num = (int)(sizeof(spi_attribute) / sizeof(spi_attribute[0]));
	for (idx = 0; idx < num; idx ++) 
	{		
		device_remove_file(dev, spi_attribute[idx]);		
	}	
	return;
}

static int ccm3310_open(struct inode * inode, struct file *file)
{
	int ret = 0;
	CCM3310_SPI_TRACE;
	ret = nonseekable_open(inode, file);
	CCM3310_SPI_TRACE_OUT;
	return ret;
}


static long ccm3310_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	int ret = 0;
	struct veb_ccm3310_data *ccm3310_dev = NULL;
	struct miscdevice *miscdev = NULL;

	CCM3310_SPI_TRACE;

	miscdev = (struct miscdevice *)(file->private_data);
	ccm3310_dev = (struct veb_ccm3310_data *)dev_get_drvdata(miscdev->this_device);
	
	if (_IOC_TYPE(cmd) != CCM3310_IOCTL_MAGIC) 
	{
		CCM3310_SPI_ERR("ccm3310_ioctl error cmd!\n");
		return -1;
	}
	cmd = _IOC_NR(cmd);

	switch (cmd) 
	{    
		case CCM3310_CHIP_CON_NEW:
		{
			/*var for spi*/
			struct spi_transfer xfer, xfer_left;
			struct spi_message p;
			unsigned char *tx_tmp = NULL;
			unsigned char *rx_tmp = NULL;
			/*var for control info*/
			ccm3310_duplex_con ccm3310_con;
			int count, left, length;
			unsigned char *tx_left, *rx_left;

			mutex_lock(&(ccm3310_dev->dev_lock));
			ret = copy_from_user(&ccm3310_con, (ccm3310_duplex_con*)arg, sizeof(ccm3310_duplex_con));
			count = ccm3310_con.len;
			if(count < MAX_TRANSFER_LEN){
				left = count % 1024;
				length = count - left;
			}else if(count == MAX_TRANSFER_LEN){
				left = 0;
				length = MAX_TRANSFER_LEN;					
			}else /*(count > MAX_TRANSFER_LEN )*/{
				CCM3310_SPI_ERR("ccm3310_ioctl data should not over 256K\n");
				mutex_unlock(&(ccm3310_dev->dev_lock));
				goto func_error;
			}		

		#ifdef CCM3310_SPI_USE_MMAP
			if (CCM3310_ALREADY_MAPPED == ccm3310_dev->mmapped){
			//use mmap for big data
				tx_tmp = (unsigned char*)ccm3310_dev->mmap_addr;
				rx_tmp = (unsigned char *)ccm3310_dev->mmap_addr + (CCM3310_MMAP_SIZE >> 1);
			}
		#endif

			#ifdef SPI_MSG_DUMP
			CCM3310_SPI_INFO("ccm3310_ioctl: CCM3310_DUPLEX_CON_NEW mmap\n");
			CCM3310_SPI_INFO("ccm3310_ioctl mmap dump count:%d left:%d\n", count, left);
			#endif

			ret = ccm3310_wait_ready();  
			if(0 != ret)
			{
				mutex_unlock(&(ccm3310_dev->dev_lock));
				goto func_error;
			}
			spi_message_init(&p);

			xfer.tx_buf     = tx_tmp;
			xfer.rx_buf     = rx_tmp;
			xfer.len        = length;
			xfer.cs_change  = 0;
			xfer.bits_per_word = BITS_PER_WORD;
			xfer.delay_usecs = 3;
			xfer.tx_nbits = SPI_NBITS_SINGLE;
			xfer.rx_nbits = SPI_NBITS_SINGLE;
			spi_message_add_tail(&xfer, &p);

			if(left){
				tx_left = tx_tmp + length;
				rx_left = rx_tmp + length;

				xfer_left.tx_buf     = tx_left;
				xfer_left.rx_buf     = rx_left;
				xfer_left.len        = left;
				xfer_left.cs_change  = 0;
				xfer_left.bits_per_word = BITS_PER_WORD;
				xfer_left.delay_usecs = 3;
				xfer_left.tx_nbits = SPI_NBITS_SINGLE;
				xfer_left.rx_nbits = SPI_NBITS_SINGLE;
				spi_message_add_tail(&xfer_left, &p);
			}

			#ifdef SPI_MSG_DUMP
			CCM3310_SPI_INFO("ccm3310_ioctl: start sync\n");
			#endif

			ret = spi_sync(ccm3310_dev->spi, &p);
			if(ret != 0)
			{
				CCM3310_SPI_ERR("spi sync failed(%d)!\n", ret);
			}
			
			#ifdef SPI_MSG_DUMP
			CCM3310_SPI_INFO("ccm3310_ioctl: sync end\n");
			#endif

			#ifdef CCM3310_SPI_USE_MMAP
			if (CCM3310_ALREADY_MAPPED == ccm3310_dev->mmapped){
				//nothing to do
			}
			#endif
			mutex_unlock(&(ccm3310_dev->dev_lock));
			break;
		}
		default:
			CCM3310_SPI_ERR("ccm3310_ioctl cmd error! cmd: %d \n", cmd);
			ret = -1;
			break;

	}
	CCM3310_SPI_TRACE_OUT;
	return ret;

func_error:
	CCM3310_SPI_ERR("ccm3310_ioctl error\n");
	return -1;
}


static int ccm3310_release(struct inode *inode, struct file *file)
{
	struct veb_ccm3310_data *ccm3310_dev = NULL;
	struct miscdevice *miscdev = NULL;
	CCM3310_SPI_TRACE;

	miscdev = (struct miscdevice *)(file->private_data);
	ccm3310_dev = (struct veb_ccm3310_data *)dev_get_drvdata(miscdev->this_device);
	mutex_lock(&(ccm3310_dev->dev_lock));
	
	ccm3310_dev->mmapped = CCM3310_NOT_MAPPED;
	
	mutex_unlock(&(ccm3310_dev->dev_lock));
	CCM3310_SPI_TRACE_OUT;
	return 0;
}

static ssize_t ccm3310_read(struct file *file, char __user *buf , size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned char * rx_tmp = NULL;
	unsigned char * rx_left = NULL;

	int left, length;
	struct spi_transfer xfer, xfer_left;
	struct spi_message p;

	struct veb_ccm3310_data *ccm3310_dev = NULL;
	struct miscdevice *miscdev = NULL;
	CCM3310_SPI_TRACE;

	miscdev = (struct miscdevice *)(file->private_data);
	ccm3310_dev = (struct veb_ccm3310_data *)dev_get_drvdata(miscdev->this_device);
	mutex_lock(&(ccm3310_dev->dev_lock));

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_INFO("ccm3310_read dump count:%d \n", (unsigned int)count);
	#endif

	#ifdef CCM3310_SPI_USE_MMAP
	if (CCM3310_ALREADY_MAPPED == ccm3310_dev->mmapped){
		rx_tmp = (unsigned char*)ccm3310_dev->mmap_addr;
	}else
	#endif
	{
		rx_tmp = ccm3310_dev->buffer;
	}

	if(count < MAX_TRANSFER_LEN){
		left = count % 1024;
		length = count - left;
	}else if(count == MAX_TRANSFER_LEN){
		left = 0;
		length = MAX_TRANSFER_LEN;
	}else /*(count > MAX_TRANSFER_LEN)*/{
		CCM3310_SPI_ERR("ccm3310_read data should not over 256K\n");
		goto func_error;
	}

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_INFO("ccm3310_read dump count:%d length:%d left:%d\n", (int)count, length, left);
	#endif

	ret = ccm3310_wait_ready();  
	if(0 != ret)
	{
		goto func_error;
	}

	spi_message_init(&p);

	if(length > 0){
		xfer.tx_buf     = rx_tmp,
		xfer.rx_buf     = rx_tmp,
		xfer.len        = length,
		xfer.cs_change  = 0,
		xfer.bits_per_word = BITS_PER_WORD,
		xfer.delay_usecs = 3,
		xfer.tx_nbits = SPI_NBITS_SINGLE;
		xfer.rx_nbits = SPI_NBITS_SINGLE;
		spi_message_add_tail(&xfer, &p);
	}

	if(left){
		rx_left = rx_tmp + length;
		xfer_left.tx_buf     = rx_left,
		xfer_left.rx_buf     = rx_left,
		xfer_left.len        = left,
		xfer_left.cs_change  = 0,
		xfer_left.bits_per_word = BITS_PER_WORD,
		xfer_left.delay_usecs = 3,
		xfer_left.speed_hz = 1000000,
		xfer_left.tx_nbits = SPI_NBITS_SINGLE;
		xfer_left.rx_nbits = SPI_NBITS_SINGLE;
		spi_message_add_tail(&xfer_left, &p);
	}

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_INFO("ccm3310_read: start sync\n");
	#endif

	ret = spi_sync(ccm3310_dev->spi, &p);

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_INFO("ccm3310_read: sync end\n");
	#endif

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_ARRAY_WITH_PREFIX("ccm3310_read dump buffer :", rx_tmp, count);
	#endif

	#ifdef CCM3310_SPI_USE_MMAP
	if (CCM3310_ALREADY_MAPPED == ccm3310_dev->mmapped){
		//nothing to do
	}else
	#endif
	{
		ret = copy_to_user(buf, rx_tmp, count);
	}
	mutex_unlock(&(ccm3310_dev->dev_lock));
	CCM3310_SPI_TRACE_OUT;
	return count;

func_error:
	mutex_unlock(&(ccm3310_dev->dev_lock));
	CCM3310_SPI_ERR("veb_ccm3310_read error\n");
	return 0;
}

static ssize_t ccm3310_write(struct file *file, const char __user *buf , size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned char * tx_tmp = NULL;
	unsigned char * rx_tmp = NULL;
	unsigned char * tx_left = NULL;
	unsigned char * rx_left = NULL;

	int left, length;
	struct spi_transfer xfer, xfer_left;
	struct spi_message p;

	struct veb_ccm3310_data *ccm3310_dev = NULL;
	struct miscdevice *miscdev = NULL;

	CCM3310_SPI_TRACE;

	miscdev = (struct miscdevice *)(file->private_data);
	ccm3310_dev = (struct veb_ccm3310_data *)dev_get_drvdata(miscdev->this_device);
	mutex_lock(&(ccm3310_dev->dev_lock));

	#ifdef CCM3310_SPI_USE_MMAP
	if (CCM3310_ALREADY_MAPPED == ccm3310_dev->mmapped){
		tx_tmp = (unsigned char*)ccm3310_dev->mmap_addr;
	}else
	#endif
	{
		if(copy_from_user(ccm3310_dev->buffer, (const char __user *)buf, count))
			goto func_error;
		tx_tmp = (unsigned char*)ccm3310_dev->buffer;
	}

	if(count < MAX_TRANSFER_LEN){
		left = count % 1024;
		length = count - left;
	}else if(count == MAX_TRANSFER_LEN){
		left = 0;
		length = MAX_TRANSFER_LEN;
	}else /*(count > MAX_TRANSFER_LEN)*/{
		CCM3310_SPI_ERR("ccm3310_write data should not over 256K\n");
		goto func_error;
	}

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_ARRAY_WITH_PREFIX("ccm3310_write dump buffer :", tx_tmp, count);
	#endif

	ret = ccm3310_spi_wake(WAKE_TIME*5);
	if(0 != ret)
	{
		goto func_error;
	}

	spi_message_init(&p);
	
	if(length > 0){
		xfer.tx_buf     = tx_tmp,
		xfer.rx_buf     = rx_tmp,
		xfer.len        = length,
		xfer.cs_change  = 0,
		xfer.bits_per_word = BITS_PER_WORD,
		xfer.delay_usecs = 3,
		xfer.tx_nbits = SPI_NBITS_SINGLE;
		xfer.rx_nbits = SPI_NBITS_SINGLE;
		spi_message_add_tail(&xfer, &p);
	}

	if(left){
		tx_left = tx_tmp + length;
		xfer_left.tx_buf     = tx_left,
		xfer_left.rx_buf     = rx_left,
		xfer_left.len        = left,
		xfer_left.cs_change  = 0,
		xfer_left.bits_per_word = BITS_PER_WORD,
		xfer_left.delay_usecs = 3,
		xfer_left.speed_hz = 1000000,
		xfer_left.tx_nbits = SPI_NBITS_SINGLE;
		xfer_left.rx_nbits = SPI_NBITS_SINGLE;
		spi_message_add_tail(&xfer_left, &p);
	}

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_INFO("ccm3310_write: start sync\n");
	#endif

	ret = spi_sync(ccm3310_dev->spi, &p);

	#ifdef SPI_MSG_DUMP
	CCM3310_SPI_INFO("ccm3310_write: sync end\n");
	#endif

	//ccm3310_spi_send(ccm3310_dev->spi, tx_tmp, count);  

	mutex_unlock(&(ccm3310_dev->dev_lock));
	CCM3310_SPI_TRACE_OUT;
	return count;

func_error:
	mutex_unlock(&(ccm3310_dev->dev_lock));
	CCM3310_SPI_ERR("veb_ccm3310_write error\n");
	return 0;

}

#ifdef CCM3310_SPI_USE_MMAP
static int ccm3310_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long phys;
	struct veb_ccm3310_data *ccm3310_dev = NULL;
	struct miscdevice *miscdev = NULL;
	CCM3310_SPI_TRACE;

	miscdev = (struct miscdevice *)(file->private_data);
	ccm3310_dev = (struct veb_ccm3310_data *)dev_get_drvdata(miscdev->this_device);
	mutex_lock(&(ccm3310_dev->dev_lock));
	
	phys = virt_to_phys(ccm3310_dev->mmap_addr);
	if(remap_pfn_range(vma,
						vma->vm_start,
						phys >> PAGE_SHIFT,
						vma->vm_end - vma->vm_start,
						vma->vm_page_prot)){
		CCM3310_SPI_ERR("ccm3310_mmap error!\n");
		ccm3310_dev->mmapped = CCM3310_NOT_MAPPED;
		mutex_unlock(&(ccm3310_dev->dev_lock));
		return -1;
	}

	ccm3310_dev->mmapped = CCM3310_ALREADY_MAPPED;
	mutex_unlock(&(ccm3310_dev->dev_lock));
	CCM3310_SPI_TRACE_OUT;
	return 0;
}
#endif


static const struct file_operations ccm3310_fops = {
	.owner = THIS_MODULE,
	.open  = ccm3310_open,
	.unlocked_ioctl = ccm3310_ioctl,
	.compat_ioctl = ccm3310_ioctl,
	.read = ccm3310_read,
	.write = ccm3310_write,
#ifdef CCM3310_SPI_USE_MMAP
	.mmap = ccm3310_mmap,
#endif
	.release = ccm3310_release,
};

static struct miscdevice ccm3310_miscdev = {
	MISC_DYNAMIC_MINOR,
	CCM3310_NAME,
	&ccm3310_fops
};

static int ccm3310_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct veb_ccm3310_data	 *ccm3310_dev = NULL;
//#ifdef CCM3310_SPI_USE_MMAP
#if 1
	int i;
	int order = 0;
#endif
	CCM3310_SPI_TRACE;

	ccm3310_dev = kzalloc(sizeof(struct veb_ccm3310_data), GFP_KERNEL);
	if (NULL == ccm3310_dev)
	{
		CCM3310_SPI_ERR("ccm3310_spi_probe spidev malloc error!\n");
		return -ENOMEM;
	}

    ccm3310_dev->spi = spi;
	ccm3310_dev->miscdev = &ccm3310_miscdev;
	mutex_init(&(ccm3310_dev->dev_lock));
	
	ccm3310_dev->buffer = (unsigned char*)kzalloc(XFER_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!ccm3310_dev->buffer) 
	{
		CCM3310_SPI_ERR("ccm3310_spi_probe ccm3310_dev.buffer malloc failed\n");
		ret = -ENOMEM;
		goto end;
	}
	
	ret = spi_create_attribute( &(ccm3310_dev->spi->dev) );
	if(ret < 0)
	{
		CCM3310_SPI_ERR("create attribute failed\n");
		kfree(ccm3310_dev->buffer);
		goto end;
	}
	
	ccm3310_dev->mmapped = CCM3310_NOT_MAPPED;
//#ifdef CCM3310_SPI_USE_MMAP
#if 0
	mutex_init(&(ccm3310_dev->mmap_lock));
#endif
	//reserve 2M buffer for mmap
	order = get_order(CCM3310_MMAP_SIZE);

	ccm3310_dev->mmap_addr = (unsigned char *)__get_free_pages(GFP_KERNEL, order);

	if(ccm3310_dev->mmap_addr){
		CCM3310_SPI_INFO("ccm3310_spi_probe kernel_memaddr:0x%lx  order:%d  PAGE_SIZE:%ld\n", (unsigned long)ccm3310_dev->mmap_addr, order, PAGE_SIZE);
		for(i=0;i<CCM3310_MMAP_SIZE/PAGE_SIZE;i++)
			SetPageReserved(virt_to_page(ccm3310_dev->mmap_addr + i*PAGE_SIZE));
	}else{
		CCM3310_SPI_ERR("ccm3310_spi_probe membuffer malloc failed\n");
		ret = -ENOMEM;
		goto end;
	}

	spi_set_drvdata(spi, ccm3310_dev);	
	
	ccm3310_set_spiport();
	ccm3310_set_gpio();

	spi->mode = SPI_MODE_3;
	spi->bits_per_word = BITS_PER_WORD;
	spi->controller_data = (void*)&ccm3310_spi_conf;
	spi->max_speed_hz = 1000000;
	ret = spi_setup(spi);
	if(ret < 0)
	{
		CCM3310_SPI_ERR("ccm3310_spi_probe  spidev spi_setup error!\n");
		ret = -1;
		goto end;
	}

	if (misc_register(&ccm3310_miscdev))
	{
		CCM3310_SPI_ERR("ccm3310_spi_probe %s: failed to register device\n", CCM3310_NAME);
		ret = -1;
		goto end;
	}
	dev_set_drvdata(ccm3310_miscdev.this_device, ccm3310_dev);
	CCM3310_SPI_INFO("ccm3310_spi_probe %s: v%s\n", CCM3310_DESC, CCM3310_VERSION);
	
	CCM3310_SPI_TRACE_OUT;
	return ret;
end:
	if(NULL != ccm3310_dev->buffer){
		kfree(ccm3310_dev->buffer);
		ccm3310_dev->buffer = NULL;
	}
	if (NULL != ccm3310_dev){
		kfree(ccm3310_dev);
		ccm3310_dev = NULL;
	}
	return ret;
}

static int ccm3310_spi_remove(struct spi_device *spi)
{
	struct veb_ccm3310_data	 *ccm3310_dev = NULL;
#ifdef CCM3310_SPI_USE_MMAP
	int i;
	int order = 0;
#endif
	CCM3310_SPI_TRACE;

	ccm3310_dev = spi_get_drvdata(spi);

#ifdef CCM3310_SPI_USE_MMAP
	order = get_order(CCM3310_MMAP_SIZE);
	if(ccm3310_dev->mmap_addr){
		for(i=0;i<CCM3310_MMAP_SIZE/PAGE_SIZE;i++)
			ClearPageReserved(virt_to_page(ccm3310_dev->mmap_addr + i*PAGE_SIZE));
		free_pages((unsigned long)ccm3310_dev->mmap_addr, order);
	}
	ccm3310_dev->mmapped = CCM3310_NOT_MAPPED;
#endif
	
	if(NULL != ccm3310_dev->buffer){
		kfree(ccm3310_dev->buffer);
		ccm3310_dev->buffer = NULL;
	}
	if(NULL != ccm3310_dev){
		kfree(ccm3310_dev);
		ccm3310_dev = NULL;
	}

	spi_remove_attribute(&(spi->dev));

	CCM3310_SPI_TRACE_OUT;
	return 0;
}

static struct of_device_id ccm3310_spi_table[] = {
	{.compatible = "ccm3310,spi_ccm3310",}, 
	{ },
};
//MODULE_DEVICE_TABLE(of, ccm3310_spi_table);

struct spi_device_id spi_id_table_ccm3310 = {"spi_ccm3310", 0};
#if 1
static struct spi_board_info spi_board_devs[] __initdata = {
	[1] = {
	.modalias = "spi_ccm3310",
	.bus_num = 1,
	.chip_select = 0,
	.mode = SPI_MODE_3,
	}
};
#endif
static struct spi_driver ccm3310_spi_driver = {
	.driver = {
		.name        = "spi_ccm3310",
		.owner        = THIS_MODULE,
		.bus = &spi_bus_type,
		.of_match_table = ccm3310_spi_table,
	},

	.probe        = ccm3310_spi_probe,
	.remove        = ccm3310_spi_remove,
	.id_table   = &spi_id_table_ccm3310,
};
#if 1
static int __init ccm3310_spi_init(void)
{
	int rc = 0;

	spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));

	rc = spi_register_driver(&ccm3310_spi_driver);
	if (rc < 0)
	{
		CCM3310_SPI_ERR("ccm3310_spi register fail : %d\n", rc);
		return rc ;
	}

	return rc ;
}

static void __exit ccm3310_spi_exit(void)
{
	if(ccm3310_miscdev.this_device)
		misc_deregister(&ccm3310_miscdev);

	spi_unregister_driver(&ccm3310_spi_driver);

	return;
}
#endif
//module_spi_driver(ccm3310_spi_driver);

module_init(ccm3310_spi_init);
module_exit(ccm3310_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CCORE CCM3310S-T40 Software");
MODULE_DESCRIPTION("spi ccm3310 driver ");
MODULE_VERSION("1.0");

