/*
 * Cryptographic API.
 *
 * Support for SFAX8 CIPHER HW acceleration.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Some ideas are from amtel-aes.c driver.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <sf16a18.h>
#include "regs.h"
#include "dma.h"

#define AES_KEY_SIZE_128		(0 << 13)
#define AES_KEY_SIZE_192		(1 << 13)
#define AES_KEY_SIZE_256		(2 << 13)

#define CFB8_BLOCK_SIZE		1
#define CFB16_BLOCK_SIZE	2
#define CFB32_BLOCK_SIZE	4
#define CFB64_BLOCK_SIZE	8

/* AES flags */
#define CIPHER_FLAGS_MODE_MASK	0x03ff
#define CIPHER_FLAGS_ECB		BIT(1)
#define CIPHER_FLAGS_CBC		BIT(2)
#define CIPHER_FLAGS_CFB		BIT(3)
#define CIPHER_FLAGS_OFB		BIT(4)
#define CIPHER_FLAGS_CTR		BIT(5)
#define CIPHER_FLAGS_ENCRYPT	BIT(6)
#define CIPHER_FLAGS_AES		(0x0 << 7)
#define CIPHER_FLAGS_DES		(0x1 << 7)
#define CIPHER_FLAGS_TDES		(0x2 << 7)
#define CIPHER_FLAGS_ARC4		(0x3 << 7)
#define CIPHER_FLAGS_MASK		(0x3 << 7)
#define CIPHER_ADDR_OFFSET		(0x400)

#define CIPHER_FLAGS_RC4_KEY_SHIFT	(21)
#define CIPHER_FLAGS_INIT		BIT(28)
#define CIPHER_FLAGS_BUSY		BIT(29)
#define CIPHER_FLAGS_FAST		BIT(30)
#define AES_BLKC_ENABLE		BIT(0)
#define CIPHER_FLAGS_DMA		BIT(0)
#define SFAX8_AES_QUEUE_LENGTH	50

#define BLKC_STATUS_IN_READY	(BIT(25) | BIT(7) | BIT(28))

#define CIPHER_OUT_READY_INT	(BIT(7) | BIT(12))
#define SFAX8_AES_DMA_THRESHOLD		16
#define SFAX8_DES_DMA_THRESHOLD		8
#define SFAX8_ARC4_DMA_THRESHOLD	4
#define BLKC_DEFAULT_MASK		(0xFFFF)
#define BLKC_CIPHER_OUT_READY_INT	(0xFFFF ^ CIPHER_OUT_READY_INT)
#define BLKC_ENABLE			BIT(0)

struct sfax8_cipher_dev;

struct sfax8_cipher_ctx {
	struct sfax8_cipher_dev *dd;

	int		keylen;
	u32		key[AES_KEYSIZE_256 / sizeof(u32)];

	u16		block_size;
};

struct sfax8_cipher_reqctx {
	unsigned long mode;
};
/*
struct sfax8_cipher_dma {
	struct dma_chan			*chan;
	struct dma_slave_config dma_conf;
};
*/
struct sfax8_cipher_dev {
	struct list_head	list;
	unsigned long		phys_base;
	void __iomem		*io_base;

	struct sfax8_cipher_ctx	*ctx;
	struct device		*dev;
	struct clk		*iclk;
	struct clk		*bus_clk;
	int	irq;
	unsigned long		flags;
	int	err;

	spinlock_t		lock;
	struct crypto_queue	queue;

	struct tasklet_struct	done_task;
	struct tasklet_struct	queue_task;

	struct ablkcipher_request	*req;
	size_t	total;

	struct scatterlist	*in_sg;
	unsigned int		nb_in_sg;
	size_t				in_offset;
	struct scatterlist	*out_sg;
	unsigned int		nb_out_sg;
	size_t				out_offset;

	size_t	bufcnt;
	size_t	buflen;
	size_t	dma_size;

	void	*buf_in;
	int		dma_in;
	dma_addr_t	dma_addr_in;
//  struct sfax8_cipher_dma	dma_lch_in;

	void	*buf_out;
	int		dma_out;
	dma_addr_t	dma_addr_out;
//	struct sfax8_cipher_dma	dma_lch_out;

};

struct sfax8_cipher_drv {
	struct list_head	dev_list;
	spinlock_t		lock;
};

static struct sfax8_cipher_drv sfax8_cipher = {
	.dev_list = LIST_HEAD_INIT(sfax8_cipher.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(sfax8_cipher.lock),
};

static int sfax8_cipher_sg_length(struct ablkcipher_request *req,
			struct scatterlist *sg)
{
	unsigned int total = req->nbytes;
	int sg_nb;
	unsigned int len;
	struct scatterlist *sg_list;

	sg_nb = 0;
	sg_list = sg;
	total = req->nbytes;

	while (total) {
		len = min(sg_list->length, total);

		sg_nb++;
		total -= len;

		sg_list = sg_next(sg_list);
		if (!sg_list)
			total = 0;
	}

	return sg_nb;
}

static int sfax8_cipher_sg_copy(struct scatterlist **sg, size_t *offset,
			void *buf, size_t buflen, size_t total, int out)
{
	unsigned int count, off = 0;

	while (buflen && total) {
		count = min((*sg)->length - *offset, total);
		count = min(count, buflen);

		if (!count)
			return off;

		scatterwalk_map_and_copy(buf + off, *sg, *offset, count, out);

		off += count;
		buflen -= count;
		*offset += count;
		total -= count;

		if (*offset == (*sg)->length) {
			*sg = sg_next(*sg);
			if (*sg)
				*offset = 0;
			else
				total = 0;
		}
	}

	return off;
}

static inline u32 sfax8_cipher_read(struct sfax8_cipher_dev *dd, u32 offset)
{
	return readl_relaxed(dd->io_base  + offset - CIPHER_ADDR_OFFSET);
}

static inline void sfax8_cipher_write(struct sfax8_cipher_dev *dd,
					u32 offset, u32 value)
{
	writel_relaxed(value, dd->io_base + offset - CIPHER_ADDR_OFFSET);
}

static void sfax8_cipher_read_n(struct sfax8_cipher_dev *dd, u32 offset,
					u32 *value, int count)
{
	for (; count--; value++, offset += 4)
		*value = sfax8_cipher_read(dd, offset);
}

static void sfax8_cipher_write_n(struct sfax8_cipher_dev *dd, u32 offset,
					u32 *value, int count)
{
	for (; count--; value++, offset += 4)
		sfax8_cipher_write(dd, offset, *value);
}

static struct sfax8_cipher_dev *sfax8_cipher_find_dev(struct sfax8_cipher_ctx *ctx)
{
	struct sfax8_cipher_dev *cipher_dd = NULL;
	struct sfax8_cipher_dev *tmp;

	spin_lock_bh(&sfax8_cipher.lock);
	if (!ctx->dd) {
		list_for_each_entry(tmp, &sfax8_cipher.dev_list, list) {
			cipher_dd = tmp;
			break;
		}
		ctx->dd = cipher_dd;
	} else {
		cipher_dd = ctx->dd;
	}

	spin_unlock_bh(&sfax8_cipher.lock);

	return cipher_dd;
}

static int sfax8_cipher_hw_init(struct sfax8_cipher_dev *dd)
{
	u32 tmp = 0;
	tmp = sfax8_cipher_read(dd, CRYPTO_CG_CFG);
	tmp |= BLKC_CLK;
	sfax8_cipher_write(dd, CRYPTO_CG_CFG, tmp);
    sfax8_cipher_write(dd, CRYPTO_BLKC_FIFO_MODE_EN, DISABLE);
	sfax8_cipher_write(dd, CRYPTO_BLKC_CONTROL, 0x1);

	if (!(dd->flags & CIPHER_FLAGS_INIT)) {
		dd->flags |= CIPHER_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}



static void sfax8_cipher_finish_req(struct sfax8_cipher_dev *dd, int err)
{
	struct ablkcipher_request *req = dd->req;
	//printk("%s called.\n",__func__);

//	clk_disable_unprepare(dd->iclk);
	dd->flags &= ~CIPHER_FLAGS_BUSY;

	req->base.complete(&req->base, err);
}

static int sfax8_cipher_crypt_dma(struct sfax8_cipher_dev *dd,
		dma_addr_t dma_addr_in, dma_addr_t dma_addr_out, int length)
{
//	struct scatterlist sg[2];
	//struct dma_async_tx_descriptor	*in_desc, *out_desc;
	int tmp;
	dd->dma_size = length;

	if (!(dd->flags & CIPHER_FLAGS_FAST)) {
		dma_sync_single_for_device(dd->dev, dma_addr_in, length,
					   DMA_TO_DEVICE);
	}
/*
	if (dd->flags & CIPHER_FLAGS_CFB8) {
		dd->dma_lch_in.dma_conf.dst_addr_width =
			DMA_SLAVE_BUSWIDTH_1_BYTE;
		dd->dma_lch_out.dma_conf.src_addr_width =
			DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (dd->flags & CIPHER_FLAGS_CFB16) {
		dd->dma_lch_in.dma_conf.dst_addr_width =
			DMA_SLAVE_BUSWIDTH_2_BYTES;
		dd->dma_lch_out.dma_conf.src_addr_width =
			DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else {
		dd->dma_lch_in.dma_conf.dst_addr_width =
			DMA_SLAVE_BUSWIDTH_4_BYTES;
		dd->dma_lch_out.dma_conf.src_addr_width =
			DMA_SLAVE_BUSWIDTH_4_BYTES;
	}

	if (dd->flags & (CIPHER_FLAGS_CFB8 | CIPHER_FLAGS_CFB16 |
			CIPHER_FLAGS_CFB32 | CIPHER_FLAGS_CFB64)) {
		dd->dma_lch_in.dma_conf.src_maxburst = 1;
		dd->dma_lch_in.dma_conf.dst_maxburst = 1;
		dd->dma_lch_out.dma_conf.src_maxburst = 1;
		dd->dma_lch_out.dma_conf.dst_maxburst = 1;
	} else {
		dd->dma_lch_in.dma_conf.src_maxburst = dd->caps.max_burst_size;
		dd->dma_lch_in.dma_conf.dst_maxburst = dd->caps.max_burst_size;
		dd->dma_lch_out.dma_conf.src_maxburst = dd->caps.max_burst_size;
		dd->dma_lch_out.dma_conf.dst_maxburst = dd->caps.max_burst_size;
	}

	dmaengine_slave_config(dd->dma_lch_in.chan, &dd->dma_lch_in.dma_conf);
	dmaengine_slave_config(dd->dma_lch_out.chan, &dd->dma_lch_out.dma_conf);
*/
	dd->flags |= CIPHER_FLAGS_DMA;

	sfax8_cipher_write(dd, CRYPTO_BRDMA_SDMA_CTRL0,DMA_BLKC_BURST_LEN << 16 | 1);	
	sfax8_cipher_write(dd, CRYPTO_BTDMA_SDMA_CTRL0,DMA_BLKC_BURST_LEN << 16 | 1);	

	sfax8_cipher_write(dd, CRYPTO_BRDMA_SDMA_CTRL1,length);	
	sfax8_cipher_write(dd, CRYPTO_BTDMA_SDMA_CTRL1,length);	

	sfax8_cipher_write(dd, CRYPTO_BRDMA_SDMA_ADDR0,sfax8_addr_to_dma_addr(dma_addr_in));	
	sfax8_cipher_write(dd, CRYPTO_BTDMA_SDMA_ADDR0,sfax8_addr_to_dma_addr(dma_addr_out));	

	sfax8_cipher_write(dd, CRYPTO_BRDMA_INT_MASK, DMA_DEFAULT_MASK);	
	sfax8_cipher_write(dd, CRYPTO_BTDMA_INT_MASK, DMA_DONE_INT);	


	tmp  = sfax8_cipher_read(dd, CRYPTO_BLKC_CONTROL);
	tmp |= AES_BLKC_ENABLE;
	sfax8_cipher_write(dd, CRYPTO_BLKC_CONTROL, tmp);

	/*wait for the blkc in-ready state*/
	udelay(5); 

	tmp  = sfax8_cipher_read(dd, CRYPTO_BLKC_STATUS);
	//printk("%s tmp is %x\n", __func__, tmp);
	if(tmp & BLKC_STATUS_IN_READY)
	{

		tmp = sfax8_cipher_read(dd, CRYPTO_BRDMA_ADMA_CFG);
		tmp |= DMA_EN_ADMA;
		sfax8_cipher_write(dd, CRYPTO_BRDMA_ADMA_CFG, tmp);
		sfax8_cipher_write(dd, CRYPTO_BRDMA_START, DMA_START);	

		tmp = sfax8_cipher_read(dd, CRYPTO_BTDMA_ADMA_CFG);
		tmp |= DMA_EN_ADMA;
		sfax8_cipher_write(dd, CRYPTO_BTDMA_ADMA_CFG, tmp);
		sfax8_cipher_write(dd, CRYPTO_BTDMA_START, DMA_START);	
	}else
		printk(KERN_ERR "%s some error occurred.\n", __func__);
/*
	sg_init_table(&sg[0], 1);
	sg_dma_address(&sg[0]) = dma_addr_in;
	sg_dma_len(&sg[0]) = length;

	sg_init_table(&sg[1], 1);
	sg_dma_address(&sg[1]) = dma_addr_out;
	sg_dma_len(&sg[1]) = length;

	in_desc = dmaengine_prep_slave_sg(dd->dma_lch_in.chan, &sg[0],
				1, DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT  |  DMA_CTRL_ACK);
	if (!in_desc)
		return -EINVAL;

	out_desc = dmaengine_prep_slave_sg(dd->dma_lch_out.chan, &sg[1],
				1, DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!out_desc)
		return -EINVAL;

	out_desc->callback = sfax8_cipher_dma_callback;
	out_desc->callback_param = dd;

	dmaengine_submit(out_desc);
	dma_async_issue_pending(dd->dma_lch_out.chan);

	dmaengine_submit(in_desc);
	dma_async_issue_pending(dd->dma_lch_in.chan);
*/
	return 0;
}

static int sfax8_cipher_crypt_cpu_start(struct sfax8_cipher_dev *dd)
{
	u32 tmp = 0;
	dd->flags &= ~CIPHER_FLAGS_DMA;
	//printk("%s called.\n",__func__);
	/* use cache buffers */
	dd->nb_in_sg = sfax8_cipher_sg_length(dd->req, dd->in_sg);
	if (!dd->nb_in_sg)
		return -EINVAL;

	dd->nb_out_sg = sfax8_cipher_sg_length(dd->req, dd->out_sg);
	if (!dd->nb_out_sg)
		return -EINVAL;

	dd->bufcnt = sg_copy_to_buffer(dd->in_sg, dd->nb_in_sg,
					dd->buf_in, dd->total);

	if (!dd->bufcnt)
		return -EINVAL;

	sfax8_cipher_write(dd, CRYPTO_BLKC_INT_MASK, BLKC_CIPHER_OUT_READY_INT);

	dd->total -= dd->bufcnt;


	tmp  = sfax8_cipher_read(dd, CRYPTO_BLKC_CONTROL);
	tmp |= AES_BLKC_ENABLE;
	sfax8_cipher_write(dd, CRYPTO_BLKC_CONTROL, tmp);

	/*wait for the blkc in-ready state*/
	udelay(5); 

	tmp  = sfax8_cipher_read(dd, CRYPTO_BLKC_STATUS);
	//printk("%s tmp is %x\n", __func__, tmp);
	if(tmp & BLKC_STATUS_IN_READY){
		if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_AES)
			sfax8_cipher_write_n(dd, CRYPTO_AES_INDATA(0), (u32 *) dd->buf_in,
					dd->bufcnt >> 2);

		if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_DES || 
				(dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_TDES)
			sfax8_cipher_write_n(dd, CRYPTO_TDES_INDATA(0), (u32 *) dd->buf_in,
					dd->bufcnt >> 2);
		if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_ARC4)
			sfax8_cipher_write_n(dd, CRYPTO_RC4_INDATA, (u32 *) dd->buf_in,
					dd->bufcnt >> 2);
		
	}else
		printk(KERN_ERR "%s:some error accourred.\n", __func__);
	return 0;
}

static int sfax8_cipher_crypt_dma_start(struct sfax8_cipher_dev *dd)
{
	int err, fast = 0, in, out;
	size_t count;
	dma_addr_t addr_in, addr_out;

	if ((!dd->in_offset) && (!dd->out_offset)) {
		/* check for alignment */
		in = IS_ALIGNED((u32)dd->in_sg->offset, sizeof(u32)) &&
			IS_ALIGNED(dd->in_sg->length, dd->ctx->block_size);
		out = IS_ALIGNED((u32)dd->out_sg->offset, sizeof(u32)) &&
			IS_ALIGNED(dd->out_sg->length, dd->ctx->block_size);
		fast = in && out;

		if (sg_dma_len(dd->in_sg) != sg_dma_len(dd->out_sg))
			fast = 0;
	}


	if (fast)  {
		count = min(dd->total, sg_dma_len(dd->in_sg));
		count = min(count, sg_dma_len(dd->out_sg));

		err = dma_map_sg(dd->dev, dd->in_sg, 1, DMA_TO_DEVICE);
		if (!err) {
			dev_err(dd->dev, "dma_map_sg() error\n");
			return -EINVAL;
		}

		err = dma_map_sg(dd->dev, dd->out_sg, 1,
				DMA_FROM_DEVICE);
		if (!err) {
			dev_err(dd->dev, "dma_map_sg() error\n");
			dma_unmap_sg(dd->dev, dd->in_sg, 1,
				DMA_TO_DEVICE);
			return -EINVAL;
		}

		addr_in = sg_dma_address(dd->in_sg);
		addr_out = sg_dma_address(dd->out_sg);

		dd->flags |= CIPHER_FLAGS_FAST;

	} else {
		/* use cache buffers */
		count = sfax8_cipher_sg_copy(&dd->in_sg, &dd->in_offset,
				dd->buf_in, dd->buflen, dd->total, 0);

		addr_in = dd->dma_addr_in;
		addr_out = dd->dma_addr_out;

		dd->flags &= ~CIPHER_FLAGS_FAST;
	}

	dd->total -= count;

	err = sfax8_cipher_crypt_dma(dd, addr_in, addr_out, count);

	if (err && (dd->flags & CIPHER_FLAGS_FAST)) {
		dma_unmap_sg(dd->dev, dd->in_sg, 1, DMA_TO_DEVICE);
		dma_unmap_sg(dd->dev, dd->out_sg, 1, DMA_TO_DEVICE);
	}

	return err;
}

static int sfax8_cipher_write_ctrl(struct sfax8_cipher_dev *dd)
{
	int err, threshold;
	u32 valcr = 0, valmr = 0;

	u32 data[4] = {0};
	err = sfax8_cipher_hw_init(dd);

	if (err)
		return err;

	if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_AES){
		if (dd->ctx->keylen == AES_KEYSIZE_128)
			valmr |= AES_KEY_SIZE_128;
		else if (dd->ctx->keylen == AES_KEYSIZE_192)
			valmr |= AES_KEY_SIZE_192;
		else
			valmr |= AES_KEY_SIZE_256;
	}

	valmr |= (dd->flags & CIPHER_FLAGS_MODE_MASK);


	sfax8_cipher_write(dd, CRYPTO_BLKC_CONTROL, valmr);

	if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_TDES || 
			(dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_DES )
		threshold = SFAX8_DES_DMA_THRESHOLD;
	else if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_AES)
		threshold = SFAX8_AES_DMA_THRESHOLD;
	else if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_ARC4)
		threshold = SFAX8_ARC4_DMA_THRESHOLD;
	if (dd->total > threshold) {
		valcr |= CIPHER_FLAGS_DMA;
	}

	sfax8_cipher_write(dd, CRYPTO_BLKC_FIFO_MODE_EN, valcr);
	
	if (((dd->flags & CIPHER_FLAGS_CBC) || (dd->flags & CIPHER_FLAGS_CFB) ||
	   (dd->flags & CIPHER_FLAGS_OFB) || (dd->flags & CIPHER_FLAGS_CTR))) {
		if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_AES)
			sfax8_cipher_write_n(dd, CRYPTO_AES_IVDATA(0), data, 4);
		else if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_DES ||
				(dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_TDES)
			sfax8_cipher_write_n(dd, CRYPTO_TDES_IVDATA1, data, 2);

	}
	/*set hw key*/
	if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_AES)
		sfax8_cipher_write_n(dd, CRYPTO_AES_KEYDATA(0), dd->ctx->key,
						dd->ctx->keylen >> 2);
	else if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_DES ||
			(dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_TDES)
		sfax8_cipher_write_n(dd, CRYPTO_TDES_KEYDATA(0), dd->ctx->key,
						dd->ctx->keylen >> 2);
	else if((dd->flags & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_ARC4)
		sfax8_cipher_write_n(dd, CRYPTO_RC4_KEYDATA(0), dd->ctx->key,
						dd->ctx->keylen >> 2);

	
	return 0;
}

static int sfax8_cipher_handle_queue(struct sfax8_cipher_dev *dd,
			       struct ablkcipher_request *req)
{
	struct crypto_async_request *async_req, *backlog;
	struct sfax8_cipher_ctx *ctx;
	struct sfax8_cipher_reqctx *rctx;
	unsigned long flags;
	int err, ret = 0;
	u32 threshold = 0;

	//printk("%s called.\n",__func__);
	spin_lock_irqsave(&dd->lock, flags);
	if (req)
		ret = ablkcipher_enqueue_request(&dd->queue, req);
	if (dd->flags & CIPHER_FLAGS_BUSY) {
		spin_unlock_irqrestore(&dd->lock, flags);
		return ret;
	}
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (async_req)
		dd->flags |= CIPHER_FLAGS_BUSY;
	spin_unlock_irqrestore(&dd->lock, flags);

	if (!async_req)
		return ret;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);

	/* assign new request to device */
	dd->req = req;
	dd->total = req->nbytes;
	dd->in_offset = 0;
	dd->in_sg = req->src;
	dd->out_offset = 0;
	dd->out_sg = req->dst;

	rctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	rctx->mode &= CIPHER_FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~CIPHER_FLAGS_MODE_MASK) | rctx->mode;
	dd->ctx = ctx;
	ctx->dd = dd;

	err = sfax8_cipher_write_ctrl(dd);
	if (!err) {
		if((rctx->mode & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_TDES || 
			(rctx->mode & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_DES )
			threshold = SFAX8_DES_DMA_THRESHOLD;
		else if((rctx->mode & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_AES)
			threshold = SFAX8_AES_DMA_THRESHOLD;
		else if((rctx->mode & CIPHER_FLAGS_MASK) == CIPHER_FLAGS_ARC4)
			threshold = SFAX8_ARC4_DMA_THRESHOLD;
		if (dd->total > threshold)
			err = sfax8_cipher_crypt_dma_start(dd);
		else
			err = sfax8_cipher_crypt_cpu_start(dd);
	}
	if (err) {
		/* aes_task will not finish it, so do it here */
		sfax8_cipher_finish_req(dd, err);
		tasklet_schedule(&dd->queue_task);
	}

	return ret;
}

static int sfax8_cipher_crypt_dma_stop(struct sfax8_cipher_dev *dd)
{
	int err = -EINVAL;
	size_t count;
	if (dd->flags & CIPHER_FLAGS_DMA) {
		err = 0;
		if  (dd->flags & CIPHER_FLAGS_FAST) {
			dma_unmap_sg(dd->dev, dd->out_sg, 1, DMA_FROM_DEVICE);
			dma_unmap_sg(dd->dev, dd->in_sg, 1, DMA_TO_DEVICE);
		} else {
			dma_sync_single_for_device(dd->dev, dd->dma_addr_out,
				dd->dma_size, DMA_FROM_DEVICE);

			/* copy data */
			count = sfax8_cipher_sg_copy(&dd->out_sg, &dd->out_offset,
				dd->buf_out, dd->buflen, dd->dma_size, 1);
			if (count != dd->dma_size) {
				err = -EINVAL;
				pr_err("not all data converted: %u\n", count);
			}
		}
	}

	return err;
}


static int sfax8_cipher_buff_init(struct sfax8_cipher_dev *dd)
{
	int err = -ENOMEM;

	dd->buf_in = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buf_out = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buflen = PAGE_SIZE;
	dd->buflen &= ~(AES_BLOCK_SIZE - 1);

	if (!dd->buf_in || !dd->buf_out) {
		dev_err(dd->dev, "unable to alloc pages.\n");
		goto err_alloc;
	}

	/* MAP here */
	dd->dma_addr_in = dma_map_single(dd->dev, dd->buf_in,
					dd->buflen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_in)) {
		dev_err(dd->dev, "dma %d bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_in;
	}

	dd->dma_addr_out = dma_map_single(dd->dev, dd->buf_out,
					dd->buflen, DMA_FROM_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_out)) {
		dev_err(dd->dev, "dma %d bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_out;
	}

	return 0;

err_map_out:
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
		DMA_TO_DEVICE);
err_map_in:
	free_page((unsigned long)dd->buf_out);
	free_page((unsigned long)dd->buf_in);
err_alloc:
	if (err)
		pr_err("error: %d\n", err);
	return err;
}

static void sfax8_cipher_buff_cleanup(struct sfax8_cipher_dev *dd)
{
	dma_unmap_single(dd->dev, dd->dma_addr_out, dd->buflen,
			 DMA_FROM_DEVICE);
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
		DMA_TO_DEVICE);
	free_page((unsigned long)dd->buf_out);
	free_page((unsigned long)dd->buf_in);
}

static int sfax8_cipher_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct sfax8_cipher_ctx *ctx = crypto_ablkcipher_ctx(
			crypto_ablkcipher_reqtfm(req));
	struct sfax8_cipher_reqctx *rctx = ablkcipher_request_ctx(req);
	struct sfax8_cipher_dev *dd;

	if (!IS_ALIGNED(req->nbytes, AES_BLOCK_SIZE)) {
		pr_err("request size is not exact amount of AES blocks\n");
		return -EINVAL;
	}
	
	switch(mode & 0x180){
		case CIPHER_FLAGS_AES:
			ctx->block_size = AES_BLOCK_SIZE;
			break;
		case CIPHER_FLAGS_DES:
			ctx->block_size = DES_BLOCK_SIZE;
			break;
		case CIPHER_FLAGS_TDES:
			ctx->block_size = DES3_EDE_BLOCK_SIZE;
			break;
/*		case ARC4_FLAGS_TDES:
			ctx->block_size = ARC4_BLOCK_SIZE;
			break;
*/
		default:
			printk("%s: Unknow mode!\n", __func__);
			break;
	}

	dd = sfax8_cipher_find_dev(ctx);
	if (!dd)
		return -ENODEV;

	rctx->mode = mode;
/*
	if(mode & (CIPHER_FLAGS_CBC | CIPHER_FLAGS_CFB | CIPHER_FLAGS_OFB)){
		dd->req->info = kmalloc(16, GFP_KERNEL);
		if(!dd->req->info){
			pr_err("%s: alloc mem failed!\n", __func__);
			return -ENOMEM;
		}
		memset(dd->req->info, 0, 16);
		//printk("%s, dd->req->info set to 0\n", __func__);
	}	
*/
	return sfax8_cipher_handle_queue(dd, req);
}
/*
static bool sfax8_cipher_filter(struct dma_chan *chan, void *slave)
{
	struct at_dma_slave	*sl = slave;

	if (sl && sl->dma_dev == chan->device->dev) {
		chan->private = sl;
		return true;
	} else {
		return false;
	}
}
*/
/*
static int sfax8_cipher_dma_init(struct sfax8_cipher_dev *dd,
	struct crypto_platform_data *pdata)
{
	int err = -ENOMEM;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	dd->dma_lch_in.chan = dma_request_slave_channel_compat(mask,
			sfax8_cipher_filter, &pdata->dma_slave->rxdata, dd->dev, "tx");
	if (!dd->dma_lch_in.chan)
		goto err_dma_in;

	dd->dma_lch_in.dma_conf.direction = DMA_MEM_TO_DEV;
	dd->dma_lch_in.dma_conf.dst_addr = dd->phys_base +
		AES_IDATAR(0);
	dd->dma_lch_in.dma_conf.src_maxburst = dd->caps.max_burst_size;
	dd->dma_lch_in.dma_conf.src_addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
	dd->dma_lch_in.dma_conf.dst_maxburst = dd->caps.max_burst_size;
	dd->dma_lch_in.dma_conf.dst_addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
	dd->dma_lch_in.dma_conf.device_fc = false;

	dd->dma_lch_out.chan = dma_request_slave_channel_compat(mask,
			sfax8_cipher_filter, &pdata->dma_slave->txdata, dd->dev, "rx");
	if (!dd->dma_lch_out.chan)
		goto err_dma_out;

	dd->dma_lch_out.dma_conf.direction = DMA_DEV_TO_MEM;
	dd->dma_lch_out.dma_conf.src_addr = dd->phys_base +
		AES_ODATAR(0);
	dd->dma_lch_out.dma_conf.src_maxburst = dd->caps.max_burst_size;
	dd->dma_lch_out.dma_conf.src_addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
	dd->dma_lch_out.dma_conf.dst_maxburst = dd->caps.max_burst_size;
	dd->dma_lch_out.dma_conf.dst_addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
	dd->dma_lch_out.dma_conf.device_fc = false;

	return 0;

err_dma_out:
	dma_release_channel(dd->dma_lch_in.chan);
err_dma_in:
	dev_warn(dd->dev, "no DMA channel available\n");
	return err;
}

static void sfax8_cipher_dma_cleanup(struct sfax8_cipher_dev *dd)
{
	dma_release_channel(dd->dma_lch_in.chan);
	dma_release_channel(dd->dma_lch_out.chan);
}
*/
static int sfax8_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			   unsigned int keylen)
{
	struct sfax8_cipher_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
		   keylen != AES_KEYSIZE_256) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int sfax8_tdes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			   unsigned int keylen)
{
	struct sfax8_cipher_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != DES_KEY_SIZE && keylen != DES3_EDE_KEY_SIZE) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int sfax8_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_ECB | CIPHER_FLAGS_AES);
}

static int sfax8_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ECB | CIPHER_FLAGS_AES);
}

static int sfax8_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CBC | CIPHER_FLAGS_AES);
}

static int sfax8_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CBC | CIPHER_FLAGS_AES);
}

static int sfax8_aes_ofb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_OFB | CIPHER_FLAGS_AES);
}

static int sfax8_aes_ofb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_OFB | CIPHER_FLAGS_AES);
}

static int sfax8_aes_cfb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CFB | CIPHER_FLAGS_AES);
}

static int sfax8_aes_cfb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CFB | CIPHER_FLAGS_CFB | CIPHER_FLAGS_AES);
}

static int sfax8_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CTR | CIPHER_FLAGS_AES);
}

static int sfax8_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CTR | CIPHER_FLAGS_AES);
}

static int sfax8_aes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct sfax8_cipher_reqctx);

	return 0;
}

static void sfax8_aes_cra_exit(struct crypto_tfm *tfm)
{
}


static int sfax8_des_ecb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_ECB | CIPHER_FLAGS_DES);
}

static int sfax8_des_ecb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ECB | CIPHER_FLAGS_DES);
}

static int sfax8_des_cbc_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CBC | CIPHER_FLAGS_DES);
}

static int sfax8_des_cbc_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CBC | CIPHER_FLAGS_DES);
}

static int sfax8_des_ofb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_OFB | CIPHER_FLAGS_DES);
}

static int sfax8_des_ofb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_OFB | CIPHER_FLAGS_DES);
}

static int sfax8_des_cfb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CFB | CIPHER_FLAGS_DES);
}

static int sfax8_des_cfb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CFB | CIPHER_FLAGS_CFB | CIPHER_FLAGS_DES);
}

static int sfax8_des_ctr_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CTR | CIPHER_FLAGS_DES);
}

static int sfax8_des_ctr_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CTR | CIPHER_FLAGS_DES);
}

static int sfax8_des3_ede_ecb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_ECB | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_ecb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ECB | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_cbc_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CBC | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_cbc_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CBC | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_ofb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_OFB | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_ofb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_OFB | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_cfb_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CFB | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_cfb_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CFB | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_ctr_encrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_ENCRYPT | CIPHER_FLAGS_CTR | CIPHER_FLAGS_TDES);
}

static int sfax8_des3_ede_ctr_decrypt(struct ablkcipher_request *req)
{
	return sfax8_cipher_crypt(req,
		CIPHER_FLAGS_CTR | CIPHER_FLAGS_TDES);
}

static int sfax8_des_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct sfax8_cipher_reqctx);

	return 0;
}

static void sfax8_des_cra_exit(struct crypto_tfm *tfm)
{
}

static struct crypto_alg aes_algs[] = {
{
	.cra_name		= "ecb(aes)",
	.cra_driver_name	= "sfax8-ecb-aes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 0xf,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_aes_cra_init,
	.cra_exit		= sfax8_aes_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.setkey		= sfax8_aes_setkey,
		.encrypt	= sfax8_aes_ecb_encrypt,
		.decrypt	= sfax8_aes_ecb_decrypt,
	}
},
{
	.cra_name		= "cbc(aes)",
	.cra_driver_name	= "sfax8-cbc-aes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 0xf,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_aes_cra_init,
	.cra_exit		= sfax8_aes_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.setkey		= sfax8_aes_setkey,
		.encrypt	= sfax8_aes_cbc_encrypt,
		.decrypt	= sfax8_aes_cbc_decrypt,
	}
},
{
	.cra_name		= "ofb(aes)",
	.cra_driver_name	= "sfax8-ofb-aes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 0xf,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_aes_cra_init,
	.cra_exit		= sfax8_aes_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.setkey		= sfax8_aes_setkey,
		.encrypt	= sfax8_aes_ofb_encrypt,
		.decrypt	= sfax8_aes_ofb_decrypt,
	}
},
{
	.cra_name		= "cfb(aes)",
	.cra_driver_name	= "sfax8-cfb-aes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 0xf,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_aes_cra_init,
	.cra_exit		= sfax8_aes_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.setkey		= sfax8_aes_setkey,
		.encrypt	= sfax8_aes_cfb_encrypt,
		.decrypt	= sfax8_aes_cfb_decrypt,
	}
},

{
	.cra_name		= "ctr(aes)",
	.cra_driver_name	= "sfax8-ctr-aes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= AES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 0xf,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_aes_cra_init,
	.cra_exit		= sfax8_aes_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.setkey		= sfax8_aes_setkey,
		.encrypt	= sfax8_aes_ctr_encrypt,
		.decrypt	= sfax8_aes_ctr_decrypt,
	}
},
{
	.cra_name		= "ecb(des)",
	.cra_driver_name	= "sfax8-ecb-des",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES_KEY_SIZE,
		.max_keysize	= DES_KEY_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des_ecb_encrypt,
		.decrypt	= sfax8_des_ecb_decrypt,
	}
},
{
	.cra_name		= "cbc(des)",
	.cra_driver_name	= "sfax8-cbc-des",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES_KEY_SIZE,
		.max_keysize	= DES_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des_cbc_encrypt,
		.decrypt	= sfax8_des_cbc_decrypt,
	}
},
{
	.cra_name		= "ofb(des)",
	.cra_driver_name	= "sfax8-ofb-des",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES_KEY_SIZE,
		.max_keysize	= DES_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des_ofb_encrypt,
		.decrypt	= sfax8_des_ofb_decrypt,
	}
},
{
	.cra_name		= "cfb(des)",
	.cra_driver_name	= "sfax8-cfb-des",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES_KEY_SIZE,
		.max_keysize	= DES_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des_cfb_encrypt,
		.decrypt	= sfax8_des_cfb_decrypt,
	}
},

{
	.cra_name		= "ctr(des)",
	.cra_driver_name	= "sfax8-ctr-des",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES_KEY_SIZE,
		.max_keysize	= DES_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des_ctr_encrypt,
		.decrypt	= sfax8_des_ctr_decrypt,
	}
},
{
	.cra_name		= "ecb(des3_ede)",
	.cra_driver_name	= "sfax8-ecb-tdes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES3_EDE_KEY_SIZE,
		.max_keysize	= DES3_EDE_KEY_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des3_ede_ecb_encrypt,
		.decrypt	= sfax8_des3_ede_ecb_decrypt,
	}
},
{
	.cra_name		= "cbc(des3_ede)",
	.cra_driver_name	= "sfax8-cbc-tdes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES3_EDE_KEY_SIZE,
		.max_keysize	= DES3_EDE_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des3_ede_cbc_encrypt,
		.decrypt	= sfax8_des3_ede_cbc_decrypt,
	}
},
{
	.cra_name		= "ofb(des3_ede)",
	.cra_driver_name	= "sfax8-ofb-tdes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES3_EDE_KEY_SIZE,
		.max_keysize	= DES3_EDE_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des3_ede_ofb_encrypt,
		.decrypt	= sfax8_des3_ede_ofb_decrypt,
	}
},
{
	.cra_name		= "cfb(des3_ede)",
	.cra_driver_name	= "sfax8-cfb-tdes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES3_EDE_KEY_SIZE,
		.max_keysize	= DES3_EDE_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des3_ede_cfb_encrypt,
		.decrypt	= sfax8_des3_ede_cfb_decrypt,
	}
},
{
	.cra_name		= "ctr(des3_ede)",
	.cra_driver_name	= "sfax8-ctr-tdes",
	.cra_priority		= 100,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= DES_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct sfax8_cipher_ctx),
	.cra_alignmask		= 3,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_init		= sfax8_des_cra_init,
	.cra_exit		= sfax8_des_cra_exit,
	.cra_u.ablkcipher = {
		.min_keysize	= DES3_EDE_KEY_SIZE,
		.max_keysize	= DES3_EDE_KEY_SIZE,
		.ivsize		= DES_BLOCK_SIZE,
		.setkey		= sfax8_tdes_setkey,
		.encrypt	= sfax8_des3_ede_ctr_encrypt,
		.decrypt	= sfax8_des3_ede_ctr_decrypt,
	}
},
};


static void sfax8_cipher_queue_task(unsigned long data)
{
	struct sfax8_cipher_dev *dd = (struct sfax8_cipher_dev *)data;
	//printk("%s called.\n",__func__);

	sfax8_cipher_handle_queue(dd, NULL);
}

static void sfax8_cipher_done_task(unsigned long data)
{
	struct sfax8_cipher_dev *dd = (struct sfax8_cipher_dev *) data;
	int err;
	//printk("%s called.\n",__func__);
	if (!(dd->flags & CIPHER_FLAGS_DMA)) {
		sfax8_cipher_read_n(dd, CRYPTO_AES_OUTDATA(0), (u32 *) dd->buf_out,
				dd->bufcnt >> 2);

		if (sg_copy_from_buffer(dd->out_sg, dd->nb_out_sg,
			dd->buf_out, dd->bufcnt))
			err = 0;
		else
			err = -EINVAL;

		goto cpu_end;
	}

	err = sfax8_cipher_crypt_dma_stop(dd);

	err = dd->err ? : err;

	if (dd->total && !err) {
		if (dd->flags & CIPHER_FLAGS_FAST) {
			dd->in_sg = sg_next(dd->in_sg);
			dd->out_sg = sg_next(dd->out_sg);
			if (!dd->in_sg || !dd->out_sg)
				err = -EINVAL;
		}
		if (!err)
			err = sfax8_cipher_crypt_dma_start(dd);
		if (!err)
			return; /* DMA started. Not fininishing. */
	}

cpu_end:
	sfax8_cipher_finish_req(dd, err);
	sfax8_cipher_handle_queue(dd, NULL);
}

static irqreturn_t sfax8_cipher_irq(int irq, void *dev_id)
{
	struct sfax8_cipher_dev *cipher_dd = dev_id;
	u32 reg;
	reg = sfax8_cipher_read(cipher_dd, CRYPTO_BLKC_INT_STAT);
	if (reg & CIPHER_OUT_READY_INT) {
		/*clear interrupt*/
		sfax8_cipher_write(cipher_dd, CRYPTO_BLKC_INT_STAT, reg);
		/*set mask*/
		sfax8_cipher_write(cipher_dd, CRYPTO_BLKC_INT_MASK, BLKC_DEFAULT_MASK);
		if (CIPHER_FLAGS_BUSY & cipher_dd->flags){
			tasklet_schedule(&cipher_dd->done_task);
			return IRQ_HANDLED;
		}else
			dev_warn(cipher_dd->dev, "AES interrupt when no active requests.\n");
	}else{
		reg = sfax8_cipher_read(cipher_dd, CRYPTO_BTDMA_INT_STAT);
		sfax8_cipher_write(cipher_dd, CRYPTO_BTDMA_INT_STAT, reg);
		sfax8_cipher_write(cipher_dd, CRYPTO_BTDMA_INT_MASK, DMA_DEFAULT_MASK);
		if(reg & DMA_SDMA_FINISH){
			tasklet_schedule(&cipher_dd->done_task);
			return IRQ_HANDLED;
		}
	}

	return IRQ_NONE;
}

static void sfax8_cipher_unregister_algs(struct sfax8_cipher_dev *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aes_algs); i++)
		crypto_unregister_alg(&aes_algs[i]);
}

static int sfax8_cipher_register_algs(struct sfax8_cipher_dev *dd)
{
	int err, i, j;

	for (i = 0; i < ARRAY_SIZE(aes_algs); i++) {
		err = crypto_register_alg(&aes_algs[i]);
		if (err)
			goto err_aes_algs;
	}
	return 0;

err_aes_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&aes_algs[j]);

	return err;
}


#if defined(CONFIG_OF)
static const struct of_device_id sfax8_cipher_dt_ids[] = {
	{ .compatible = "siflower,sfax8-cipher" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sfax8_cipher_dt_ids);
#endif

static int sfax8_cipher_probe(struct platform_device *pdev)
{
	struct sfax8_cipher_dev *cipher_dd;
	struct device *dev = &pdev->dev;
	struct resource *aes_res;
	unsigned long aes_phys_size;
	int err;

	cipher_dd = kzalloc(sizeof(struct sfax8_cipher_dev), GFP_KERNEL);
	if (cipher_dd == NULL) {
		dev_err(dev, "unable to alloc data struct.\n");
		err = -ENOMEM;
		goto cipher_dd_err;
	}
	cipher_dd->iclk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(cipher_dd->iclk)) {
		dev_err(dev, "clock intialization failed.\n");
		err = PTR_ERR(cipher_dd->iclk);
		goto aes_io_err;
	}

	clk_prepare_enable(cipher_dd->iclk);

	cipher_dd->bus_clk = of_clk_get((&pdev->dev)->of_node, 1);
	if (IS_ERR(cipher_dd->bus_clk)) {
		dev_err(dev, "clock intialization failed.\n");
		err = PTR_ERR(cipher_dd->bus_clk);
		goto aes_io_err;
	}
	clk_prepare_enable(cipher_dd->bus_clk);
	if(hold_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;
	if(release_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;

	cipher_dd->dev = dev;

	platform_set_drvdata(pdev, cipher_dd);

	INIT_LIST_HEAD(&cipher_dd->list);

	tasklet_init(&cipher_dd->done_task, sfax8_cipher_done_task,
					(unsigned long)cipher_dd);
	tasklet_init(&cipher_dd->queue_task, sfax8_cipher_queue_task,
					(unsigned long)cipher_dd);

	crypto_init_queue(&cipher_dd->queue, SFAX8_AES_QUEUE_LENGTH);

	cipher_dd->irq = -1;

	/* Get the base address */
	aes_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aes_res) {
		dev_err(dev, "no MEM resource info\n");
		err = -ENODEV;
		goto res_err;
	}
	cipher_dd->phys_base = aes_res->start;
	aes_phys_size = resource_size(aes_res);

	/* Get the IRQ */
	cipher_dd->irq = platform_get_irq(pdev,  0);
	if (cipher_dd->irq < 0) {
		dev_err(dev, "no IRQ resource info\n");
		err = cipher_dd->irq;
		goto aes_irq_err;
	}

	err = request_irq(cipher_dd->irq, sfax8_cipher_irq, IRQF_SHARED, "sfax8-cipher",
						cipher_dd);
	if (err) {
		dev_err(dev, "unable to request aes irq.\n");
		goto aes_irq_err;
	}

	cipher_dd->io_base = ioremap(cipher_dd->phys_base, aes_phys_size);
	if (!cipher_dd->io_base) {
		dev_err(dev, "can't ioremap\n");
		err = -ENOMEM;
		goto aes_io_err;
	}


	sfax8_cipher_hw_init(cipher_dd);
//	sfax8_cipher_get_cap(cipher_dd);


	err = sfax8_cipher_buff_init(cipher_dd);
	if (err)
		goto err_aes_buff;
/*
	err = sfax8_cipher_dma_init(cipher_dd, pdata);
	if (err)
		goto err_aes_dma;
*/
	spin_lock(&sfax8_cipher.lock);
	list_add_tail(&cipher_dd->list, &sfax8_cipher.dev_list);
	spin_unlock(&sfax8_cipher.lock);

	err = sfax8_cipher_register_algs(cipher_dd);
	if (err)
		goto err_algs;

	return 0;

err_algs:
	spin_lock(&sfax8_cipher.lock);
	list_del(&cipher_dd->list);
	spin_unlock(&sfax8_cipher.lock);
	sfax8_cipher_buff_cleanup(cipher_dd);
err_aes_buff:
	iounmap(cipher_dd->io_base);
aes_io_err:
	free_irq(cipher_dd->irq, cipher_dd);
aes_irq_err:
res_err:
	tasklet_kill(&cipher_dd->done_task);
	tasklet_kill(&cipher_dd->queue_task);
	kfree(cipher_dd);
	cipher_dd = NULL;
cipher_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int sfax8_cipher_remove(struct platform_device *pdev)
{
	static struct sfax8_cipher_dev *cipher_dd;

	cipher_dd = platform_get_drvdata(pdev);
	if (!cipher_dd)
		return -ENODEV;
	spin_lock(&sfax8_cipher.lock);
	list_del(&cipher_dd->list);
	spin_unlock(&sfax8_cipher.lock);

	sfax8_cipher_unregister_algs(cipher_dd);

	tasklet_kill(&cipher_dd->done_task);
	tasklet_kill(&cipher_dd->queue_task);

	iounmap(cipher_dd->io_base);

	clk_put(cipher_dd->iclk);

	if (cipher_dd->irq > 0)
		free_irq(cipher_dd->irq, cipher_dd);

	kfree(cipher_dd);
	cipher_dd = NULL;

	clk_disable_unprepare(cipher_dd->iclk);
	clk_disable_unprepare(cipher_dd->bus_clk);
	hold_reset(SF_CRYPTO_SOFT_RESET);

	return 0;
}

static struct platform_driver sfax8_cipher_driver = {
	.probe		= sfax8_cipher_probe,
	.remove		= sfax8_cipher_remove,
	.driver		= {
		.name	= "sfax8_cipher",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sfax8_cipher_dt_ids),
	},
};

module_platform_driver(sfax8_cipher_driver);

MODULE_DESCRIPTION("SIFLOWER AES/DES/TDES/ARC4 hw acceleration support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("chang.li@siflower.com.cn");
