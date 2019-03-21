#ifndef __CCM3310_SPI_DRV__
#define __CCM3310_SPI_DRV__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
//#include <mt-plat/mt_gpio.h>

//#include <mt_spi.h>
//#include "mt_spi_hal.h"

#include <linux/miscdevice.h>
#include <asm/ioctl.h>

#include <linux/sched.h>


#define CCM3310_SPI_USE_MMAP

#define CCM3310_DEBUG

#ifdef CCM3310_DEBUG
#define SPI_MSG_DUMP
#define CCM3310_SPI_TRACE  (printk(KERN_ALERT"["CCM3310_NAME"]:%5d: <%s> IN\n", __LINE__, __func__))
#define CCM3310_SPI_TRACE_OUT  (printk(KERN_ALERT"["CCM3310_NAME"]:%5d: <%s> OUT\n", __LINE__, __func__))
#define CCM3310_SPI_INFO(fmt, args...)  (printk(KERN_INFO "["CCM3310_NAME"]:%5d: <%s> " fmt, __LINE__, __func__,##args))
#define CCM3310_SPI_ARRAY_WITH_PREFIX(prefix, buffer, length)   \
	do{                                                 \
		CCM3310_SPI_INFO("%s\n", prefix);                    \
		print_hex_dump(KERN_ALERT, "["CCM3310_NAME"]",       \
			DUMP_PREFIX_OFFSET, 16, 1,                  \
			buffer,                                     \
			min_t(size_t, length, 1024),                \
			true);                                      \
	}while(0)
#else 
#define CCM3310_SPI_TRACE
#define CCM3310_SPI_TRACE_OUT
#define CCM3310_SPI_INFO(fmt, args...)
#define CCM3310_SPI_ARRAY_WITH_PREFIX(prefix, buffer, length)    \
	do{                                                 	\
	}while(0)
#endif
#define CCM3310_SPI_ERR(fmt, args...)  (printk(KERN_ALERT "["CCM3310_NAME"]:%5d: <%s> " fmt, __LINE__, __func__,##args))

#define    GPIO_SPI_RB_GPIO           	    26//(65|0x80000000)	//EINT4
#define    GPIO_SPI_WAKEUP_GPIO             27//(66|0x80000000)	//EINT3

#define    GPIO_TEST_THREE_GPIO             (112|0x80000000)
#define    GPIO_TEST_FOUR_GPIO         	    (109|0x80000000)

#define    GPIO_SPI_CS_PIN                  (237|0x80000000)
#define    GPIO_SPI_SCK_PIN                 (234|0x80000000)
#define    GPIO_SPI_MOSI_PIN                (236|0x80000000)
#define    GPIO_SPI_MISO_PIN                (235|0x80000000)

#define    MAX_PACKET_LEN	1024			//max packet len on mtk6572
#define    MAX_TRANSFER_LEN	(256*MAX_PACKET_LEN)	//max transfer len on mtk6572

#define    XFER_BUF_SIZE	256*1024
#define    BITS_PER_WORD	8
#define    CCM3310_NAME		"ccm3310_spi"
#define    CCM3310_DESC		"ccm3310 chip driver"
#define    CCM3310_VERSION	"1.0"

#define CCM3310_IOCTL_MAGIC 0xa5
#define CCM3310_CHIP_CON_NEW 213

#define DMA_TRANSFER 1

struct mt_chip_conf {
	int setuptime;
	int holdtime;
	int high_time;
	int low_time;
	int cs_idletime;
	int ulthgh_thrsh;
	int pause;
	int deassert;
	int cpol;
	int cpha;
	int rx_mlsb;
	int tx_mlsb;
	int tx_endian;
	int rx_endian;
	int com_mod;
	int finish_intr;
	int ulthigh;
	int tckdly;
};

/*spi config*/
static struct mt_chip_conf ccm3310_spi_conf=
{
	.setuptime = 4, 
	.holdtime = 4,  
	.high_time = 2,
	.low_time =  2,
	//de-assert mode
	/*
	.cs_idletime = 30, 
	.pause = 0,
	.deassert = 1,
	*/

	//pause mode
	///*		
	.cs_idletime = 1,
	.pause = 1,
	.deassert = 0,
	//*/

	.cpol = 1,
	.cpha = 1,

	.rx_mlsb = 1,  
	.tx_mlsb = 1,

	.tx_endian = 0, 
	.rx_endian = 0,

	.com_mod = DMA_TRANSFER,   //DMA_TRANSFER FIFO_TRANSFER
	
	.finish_intr = 1,
	.ulthigh = 0,
	.tckdly = 2,
};
struct veb_ccm3310_data {
	struct spi_device    *spi;
	struct miscdevice 	*miscdev;
	struct mutex        dev_lock;
	u8            *buffer;

#define CCM3310_MMAP_SIZE       0x200000
	struct mutex 		mmap_lock;
	void 				*mmap_addr;
#define CCM3310_ALREADY_MAPPED	0x5a
#define CCM3310_NOT_MAPPED		0	
	u8					mmapped;
};

#define CRC8 1
#define CRC16 2
#define CRC32 3

typedef struct
{
    int len;
    int iCrc;
    unsigned char *recv;
    unsigned char b_con;
}ccm3310_duplex_con;



#endif
