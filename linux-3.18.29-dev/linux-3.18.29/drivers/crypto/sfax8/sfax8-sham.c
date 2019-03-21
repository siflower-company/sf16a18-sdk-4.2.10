/*
 * Cryptographic API.
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Some ideas are from atmel-hash.c drivers.
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
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <linux/string.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <crypto/internal/hash.h>
#include <sf16a18.h>
#include "dma.h"
#include "regs.h"


#define MD5_DIGEST_SIZE			16
#define SFAX8_SHAM_DMA_THRESHOLD	56
/* SHAM flags */
#define SHAM_FLAGS_BUSY			BIT(0)
#define	SHAM_FLAGS_FINAL			BIT(1)
#define SHAM_FLAGS_DMA_ACTIVE	BIT(2)
#define SHAM_FLAGS_OUTPUT_READY	BIT(3)
#define SHAM_FLAGS_INIT			BIT(4)
#define SHAM_FLAGS_CPU			BIT(5)
#define SHAM_FLAGS_DMA_READY		BIT(6)

#define SHAM_FLAGS_FINUP			BIT(16)
#define SHAM_FLAGS_SG				BIT(17)

#define SHAM_FLAGS_MD5				BIT(18)
#define SHAM_FLAGS_SHAM1			BIT(19)
#define SHAM_FLAGS_SHAM224			BIT(20)
#define SHAM_FLAGS_SHAM256			BIT(21)
#define SHAM_FLAGS_HMAC				BIT(22)

#define SHAM_FLAGS_ERROR			BIT(23)
#define SHAM_FLAGS_PAD				BIT(24)

#define SHAM_ALGO_MD5			(0 << 3)
#define SHAM_ALGO_SHA1			(1 << 3)
#define SHAM_ALGO_SHA224		(2 << 3)
#define SHAM_ALGO_SHA256		(3 << 3)
#define SHAM_ALGO_HMAC			(1 << 5)
#define SHAM_OP_UPDATE	1
#define SHAM_OP_FINAL	2

#define SHAM_BUFFER_LEN		PAGE_SIZE

#define SHAM_DMA_THRESHOLD		56
#define SHAM_HASH_CLEAR			(1 << 1) 
#define SHAM_HASH_START			(1 << 2) 
#define SHAM_HASH_ENABLE		(1 << 7) 
#define SHAM_HASH_MESSAGE_DONE	(1 << 0) 
#define SHAM_ADDR_OFFSET		(0x800)

struct sfax8_sham_dev;

struct sfax8_sham_reqctx {
	struct sfax8_sham_dev	*dd;
	unsigned long	flags;
	unsigned long	op;

	u8	digest[SHA512_DIGEST_SIZE] __aligned(sizeof(u32));
	u64	digcnt[2];
	size_t	bufcnt;
	size_t	buflen;
	dma_addr_t	dma_addr;

	/* walk state */
	struct scatterlist	*sg;
	unsigned int	offset;	/* offset in current sg */
	unsigned int	total;	/* total request */

	size_t block_size;

	u8	buffer[0] __aligned(sizeof(u32));
};

struct sfax8_sham_ctx {
	struct sfax8_sham_dev	*dd;

	unsigned long		flags;

	/* fallback stuff */
	//struct crypto_shash	*fallback;
	u8 *key;
	u32 keylen;

};

#define SF_SHAM_QUEUE_LENGTH	50
/*
struct sfax8_sham_dma {
	struct dma_chan			*chan;
	struct dma_slave_config dma_conf;
};
*/
struct sfax8_sham_dev {
	struct list_head	list;
	unsigned long		phys_base;
	struct device		*dev;
	struct clk			*iclk;
	struct clk			*bus_clk;
	int					irq;
	void __iomem		*io_base;

	spinlock_t		lock;
	int			err;
	struct tasklet_struct	done_task;

	unsigned long		flags;
	struct crypto_queue	queue;
	struct ahash_request	*req;

	//struct sfax8_sham_dma	dma_lch_in;

};

struct sfax8_sham_drv {
	struct list_head	dev_list;
	spinlock_t		lock;
};

static struct sfax8_sham_drv sfax8_sham = {
	.dev_list = LIST_HEAD_INIT(sfax8_sham.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(sfax8_sham.lock),
};

static inline u32 sfax8_sham_read(struct sfax8_sham_dev *dd, u32 offset)
{
	return readl_relaxed(dd->io_base + offset - SHAM_ADDR_OFFSET);
}

static inline void sfax8_sham_write(struct sfax8_sham_dev *dd,
					u32 offset, u32 value)
{
	writel_relaxed(value, dd->io_base + offset - SHAM_ADDR_OFFSET);
}
static int sfax8_sham_set_key(struct sfax8_sham_dev *dd, const u8 *key, u32 keylen)
{
	u32 i = 0;
	u32 len32;
	u32 mask = 0;
	const u32 *buffer = (const u32 *)key;
	if(!dd)
		return -ENODEV;

	if(keylen % 4)
		mask = ((1 << ((keylen % 4) * 8)) - 1);
	len32 = DIV_ROUND_UP(keylen, sizeof(u32));
	/*
	for(i = 0; i < keylen; i++){
		keyword |= key[i] << ((3 - i % 4) * 8);
		if( (i + 1) % 4 == 0 || i == (keylen - 1)){
			sfax8_sham_write(dd, CRYPTO_HASH_HMAC_KEY(i / 4 * 4),
						 keyword);	
			keyword = 0;
		}
	}
	*/
	for (i = 0; i < len32; i++){
		if(i + 1 == len32 && mask)
			sfax8_sham_write(dd, CRYPTO_HASH_HMAC_KEY(i % 16), buffer[i] & mask);
		else
			sfax8_sham_write(dd, CRYPTO_HASH_HMAC_KEY(i % 16), buffer[i]);
	}
	return 0;
}

static size_t sfax8_sham_append_sg(struct sfax8_sham_reqctx *ctx)
{
	size_t count;
	while ((ctx->bufcnt < ctx->buflen) && ctx->total) {
		count = min(ctx->sg->length - ctx->offset, ctx->total);
		count = min(count, ctx->buflen - ctx->bufcnt);

		if (count <= 0)
			break;

		scatterwalk_map_and_copy(ctx->buffer + ctx->bufcnt, ctx->sg,
			ctx->offset, count, 0);

		ctx->bufcnt += count;
		ctx->offset += count;
		ctx->total -= count;

		if (ctx->offset == ctx->sg->length) {
			ctx->sg = sg_next(ctx->sg);
			if (ctx->sg)
				ctx->offset = 0;
			else
				ctx->total = 0;
		}
	}

	return 0;
}

/*
 * The purpose of this padding is to ensure that the padded message is a
 * multiple of 512 bits (SHAM1/SHAM224/SHAM256) or 1024 bits (SHAM384/SHAM512).
 * The bit "1" is appended at the end of the message followed by
 * "padlen-1" zero bits. Then a 64 bits block (SHAM1/SHAM224/SHAM256) or
 * 128 bits block (SHAM384/SHAM512) equals to the message length in bits
 * is appended.
 *
 * For SHAM1/SHAM224/SHAM256, padlen is calculated as followed:
 *  - if message length < 56 bytes then padlen = 56 - message length
 *  - else padlen = 64 + 56 - message length
 *
 * For SHAM384/SHAM512, padlen is calculated as followed:
 *  - if message length < 112 bytes then padlen = 112 - message length
 *  - else padlen = 128 + 112 - message length
 */
/*
static void sfax8_sham_fill_padding(struct sfax8_sham_reqctx *ctx, int length)
{
	unsigned int index, padlen;
	u64 bits[2];
	u64 size[2];

	size[0] = ctx->digcnt[0];
	size[1] = ctx->digcnt[1];

	size[0] += ctx->bufcnt;
	if (size[0] < ctx->bufcnt)
		size[1]++;

	size[0] += length;
	printk("%s:length = %d, size0 = %llu\n", __func__, length, size[0]);
	if (size[0]  < length)
		size[1]++;

	//bits[1] = cpu_to_le64(size[0] << 3);
	//bits[0] = cpu_to_le64(size[1] << 3 | size[0] >> 61);
	bits[1] = (size[0] << 3);
	bits[0] = (size[1] << 3 | size[0] >> 61);
    printk("%s:bits1 = %llu, bits0 = %llu\n", __func__, bits[1], bits[0]);
	index = ctx->bufcnt & 0x3f;
    //if(index != 64){
	padlen = (index < 56) ? (56 - index) : ((64+56) - index);
	*(ctx->buffer + ctx->bufcnt) = 0x80;
	memset(ctx->buffer + ctx->bufcnt + 1, 0, padlen-1);
	memcpy(ctx->buffer + ctx->bufcnt + padlen, &bits[1], 8);
	ctx->bufcnt += padlen + 8;
    //}
	ctx->flags |= SHAM_FLAGS_PAD;
}
*/
static int sfax8_sham_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm  = crypto_ahash_reqtfm(req);
	struct sfax8_sham_ctx *tctx = crypto_ahash_ctx(tfm);
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	struct sfax8_sham_dev *dd = NULL;
	struct sfax8_sham_dev *tmp;

	/*
	if(!req->nbytes){
		printk("req->nbytes = 0\n");
		return -EINVAL;
	}
	*/
	spin_lock_bh(&sfax8_sham.lock);
	if (!tctx->dd) {
		list_for_each_entry(tmp, &sfax8_sham.dev_list, list) {
			dd = tmp;
			break;
		}
		tctx->dd = dd;
	} else {
		dd = tctx->dd;
	}

	spin_unlock_bh(&sfax8_sham.lock);

	ctx->dd = dd;

	ctx->flags = 0;

	dev_dbg(dd->dev, "init: digest size: %d\n",
		crypto_ahash_digestsize(tfm));

	switch (crypto_ahash_digestsize(tfm)) {
	case SHA1_DIGEST_SIZE:
		ctx->flags |= SHAM_FLAGS_SHAM1;
		ctx->block_size = SHA1_BLOCK_SIZE;
		break;
	case SHA224_DIGEST_SIZE:
		ctx->flags |= SHAM_FLAGS_SHAM224;
		ctx->block_size = SHA224_BLOCK_SIZE;
		break;
	case SHA256_DIGEST_SIZE:
		ctx->flags |= SHAM_FLAGS_SHAM256;
		ctx->block_size = SHA256_BLOCK_SIZE;
		break;
	case MD5_DIGEST_SIZE:
		ctx->flags |= SHAM_FLAGS_MD5;
		ctx->block_size = SHA1_BLOCK_SIZE;
		break;
	default:
		return -EINVAL;
		break;
	}
	if(tctx->flags & SHAM_FLAGS_HMAC)
		ctx->flags |=  SHAM_FLAGS_HMAC;


	/*
	case SHAM384_DIGEST_SIZE:
		ctx->flags |= SHAM_FLAGS_SHAM384;
		ctx->block_size = SHAM384_BLOCK_SIZE;
		break;
	case SHAM512_DIGEST_SIZE:
		ctx->flags |= SHAM_FLAGS_SHAM512;
		ctx->block_size = SHAM512_BLOCK_SIZE;
		break;
	*/

	ctx->bufcnt = 0;
	ctx->digcnt[0] = 0;
	ctx->digcnt[1] = 0;
	ctx->buflen = SHAM_BUFFER_LEN;
	ctx->offset = 0;

	return 0;
}

static void sfax8_sham_write_ctrl(struct sfax8_sham_dev *dd, int dma, size_t size)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(dd->req);
	struct sfax8_sham_ctx *tctx = crypto_ahash_ctx(tfm);

	u32 tmp = 0, valmr = 0;
	u64 data_size = 0;
	if (likely(dma)) {
	/*
		if (!dd->caps.has_dma)
			sfax8_sham_write(dd, SHAM_IER, SHAM_INT_TXBUFE);
		valmr = SHA_MR_MODE_PDC;
		if (dd->caps.has_dualbuff)
			valmr |= SHAM_MR_DUALBUFF;
	*/
		sfax8_sham_write(dd, CRYPTO_HASH_FIFO_MODE_EN, ENABLE);
	} else {
		sfax8_sham_write(dd, CRYPTO_HASH_FIFO_MODE_EN, DISABLE);
	}
	/*set mask*/
	sfax8_sham_write(dd, CRYPTO_HASH_INT_MASK, 0x3E);

	if (ctx->flags & SHAM_FLAGS_SHAM1)
		valmr |= SHAM_ALGO_SHA1;
	else if (ctx->flags & SHAM_FLAGS_SHAM224)
		valmr |= SHAM_ALGO_SHA224;
	else if (ctx->flags & SHAM_FLAGS_SHAM256)
		valmr |= SHAM_ALGO_SHA256;
	else if (ctx->flags & SHAM_FLAGS_MD5)
		valmr |= SHAM_ALGO_MD5;
	
	if(ctx->flags & SHAM_FLAGS_HMAC)
		valmr |= SHAM_ALGO_HMAC;
	/* Setting CR_FIRST only for the first iteration */
	/*
	if (!(ctx->digcnt[0] || ctx->digcnt[1]))
		valcr = SHA_CR_FIRST;
	*/
	//sfax8_sham_write(dd, SHAM_CR, valcr);
	tmp = sfax8_sham_read(dd, CRYPTO_HASH_CONTROL);
	tmp |= valmr;
	sfax8_sham_write(dd, CRYPTO_HASH_CONTROL, tmp);

	data_size = size *  8;
	sfax8_sham_write(dd, CRYPTO_HASH_MSG_SIZE_LOW, (u32)data_size);
	sfax8_sham_write(dd, CRYPTO_HASH_MSG_SIZE_HIGH, (u32)(data_size >> 32));


	if(tctx->flags & SHAM_FLAGS_HMAC){
		sfax8_sham_set_key(tctx->dd, tctx->key, tctx->keylen);
	}
}

static int sfax8_sham_xmit_cpu(struct sfax8_sham_dev *dd, const u8 *buf,
			      size_t length, int final)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);
	int count, len32;
	const u32 *buffer = (const u32 *)buf;
	u32 tmp = 0;

	dev_dbg(dd->dev, "xmit_cpu: digcnt: 0x%llx 0x%llx, length: %d, final: %d\n",
		ctx->digcnt[1], ctx->digcnt[0], length, final);

	sfax8_sham_write_ctrl(dd, 0, length);

	/* should be non-zero before next lines to disable clocks later */
	ctx->digcnt[0] += length;
	if (ctx->digcnt[0] < length)
		ctx->digcnt[1]++;

	if (final)
		dd->flags |= SHAM_FLAGS_FINAL; /* catch last interrupt */

	len32 = DIV_ROUND_UP(length, sizeof(u32));

	dd->flags |= SHAM_FLAGS_CPU;

	tmp = sfax8_sham_read(dd, CRYPTO_HASH_CONTROL);
	tmp += SHAM_HASH_START;
	sfax8_sham_write(dd, CRYPTO_HASH_CONTROL, tmp);
	
	for (count = 0; count < len32; count++){
		sfax8_sham_write(dd, CRYPTO_HASH_DATA_IN(count % 8), buffer[count]);
	}

	/*
	for (count = 0; count < length; count++){
        tmp = count? tmp: 0;
		tmp |= buf[count] << ( (count % 4) * 8);
        printk("tmp = 0x%x", tmp);
		if(count == (length - 1) || (count % 4) == 3){
              
              sfax8_sham_write(dd, CRYPTO_HASH_DATA_IN(count / 4), tmp);
        }
	}
	*/
	return -EINPROGRESS;
}

static int sfax8_sham_xmit_dma(struct sfax8_sham_dev *dd, dma_addr_t dma_addr1,
		size_t length1, dma_addr_t dma_addr2, size_t length2, int final)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);
	u32 tmp = 0;
//	struct dma_async_tx_descriptor	*in_desc;
//	struct scatterlist sg[2];

/*
	dev_dbg(dd->dev, "xmit_dma: digcnt: 0x%llx 0x%llx, length: %d, final: %d\n",
		ctx->digcnt[1], ctx->digcnt[0], length1, final);

	if (ctx->flags & (SHAM_FLAGS_SHAM1 | SHAM_FLAGS_SHAM224 |
			SHAM_FLAGS_SHAM256)) {
		dd->dma_lch_in.dma_conf.src_maxburst = 16;
		dd->dma_lch_in.dma_conf.dst_maxburst = 16;
	} else {
		dd->dma_lch_in.dma_conf.src_maxburst = 32;
		dd->dma_lch_in.dma_conf.dst_maxburst = 32;
	}

	dmaengine_slave_config(dd->dma_lch_in.chan, &dd->dma_lch_in.dma_conf);

	if (length2) {
		sg_init_table(sg, 2);
		sg_dma_address(&sg[0]) = dma_addr1;
		sg_dma_len(&sg[0]) = length1;
		sg_dma_address(&sg[1]) = dma_addr2;
		sg_dma_len(&sg[1]) = length2;
		in_desc = dmaengine_prep_slave_sg(dd->dma_lch_in.chan, sg, 2,
			DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	} else {
		sg_init_table(sg, 1);
		sg_dma_address(&sg[0]) = dma_addr1;
		sg_dma_len(&sg[0]) = length1;
		in_desc = dmaengine_prep_slave_sg(dd->dma_lch_in.chan, sg, 1,
			DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	}
	if (!in_desc)
		return -EINVAL;

	in_desc->callback = sfax8_sham_dma_callback;
	in_desc->callback_param = dd;

	sfax8_sham_write_ctrl(dd, 1);
*/
	/* should be non-zero before next lines to disable clocks later */
    sfax8_sham_write_ctrl(dd, 1, length1);
	//sfax8_sham_write(dd, CRYPTO_HRDMA_ADMA_CFG, DMA_EN_BUF_BOUND);
	sfax8_sham_write(dd, CRYPTO_HRDMA_SDMA_CTRL0, DMA_HASH_BURST_LEN << 16 | 1);	
	sfax8_sham_write(dd, CRYPTO_HRDMA_SDMA_CTRL1, length1);	
	sfax8_sham_write(dd, CRYPTO_HRDMA_SDMA_ADDR0, 
						 sfax8_addr_to_dma_addr(dma_addr1));	
	ctx->digcnt[0] += length1;
	if (ctx->digcnt[0] < length1)
		ctx->digcnt[1]++;

	if (final)
		dd->flags |= SHAM_FLAGS_FINAL; /* catch last interrupt */

	dd->flags |=  SHAM_FLAGS_DMA_ACTIVE;

	/* Start DMA transfer */
	/*
	dmaengine_submit(in_desc);
	dma_async_issue_pending(dd->dma_lch_in.chan);
	*/

	tmp =  sfax8_sham_read(dd, CRYPTO_HASH_CONTROL);
	tmp |= SHAM_HASH_START;
	sfax8_sham_write(dd, CRYPTO_HASH_CONTROL, tmp);
	
	tmp = sfax8_sham_read(dd, CRYPTO_HRDMA_ADMA_CFG);
	tmp |= DMA_EN_ADMA; 
	sfax8_sham_write(dd, CRYPTO_HRDMA_ADMA_CFG, tmp);

	sfax8_sham_write(dd, CRYPTO_HRDMA_START, 1);
	return -EINPROGRESS;
}

static int sfax8_sham_xmit_start(struct sfax8_sham_dev *dd, dma_addr_t dma_addr1,
		size_t length1, dma_addr_t dma_addr2, size_t length2, int final)
{
	/*
	if (dd->caps.has_dma)
		return sfax8_sham_xmit_dma(dd, dma_addr1, length1,
				dma_addr2, length2, final);
	else
		return sfax8_sham_xmit_pdc(dd, dma_addr1, length1,
				dma_addr2, length2, final);
	*/
	return sfax8_sham_xmit_dma(dd, dma_addr1, length1,
				dma_addr2, length2, final);
}

static int sfax8_sham_update_cpu(struct sfax8_sham_dev *dd)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);
	int bufcnt;

	sfax8_sham_append_sg(ctx);
	//sfax8_sham_fill_padding(ctx, 0);
	bufcnt = ctx->bufcnt;
	ctx->bufcnt = 0;

	return sfax8_sham_xmit_cpu(dd, ctx->buffer, bufcnt, 1);
}

static int sfax8_sham_xmit_dma_map(struct sfax8_sham_dev *dd,
					struct sfax8_sham_reqctx *ctx,
					size_t length, int final)
{
	ctx->dma_addr = dma_map_single(dd->dev, ctx->buffer,
				ctx->buflen + ctx->block_size, DMA_TO_DEVICE);
	//printk("%s: dma_addr = 0x%x, length = %d\n", ctx->dma_addr, length);
	if (dma_mapping_error(dd->dev, ctx->dma_addr)) {
		dev_err(dd->dev, "dma %u bytes error\n", ctx->buflen +
				ctx->block_size);
		return -EINVAL;
	}

	ctx->flags &= ~SHAM_FLAGS_SG;

	/* next call does not fail... so no unmap in the case of error */
	return sfax8_sham_xmit_start(dd, ctx->dma_addr, length, 0, 0, final);
}

static int sfax8_sham_update_dma_slow(struct sfax8_sham_dev *dd)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);
	unsigned int final;
	size_t count;

	sfax8_sham_append_sg(ctx);

	final = (ctx->flags & SHAM_FLAGS_FINUP) && !ctx->total;

	dev_dbg(dd->dev, "slow: bufcnt: %u, digcnt: 0x%llx 0x%llx, final: %d\n",
		 ctx->bufcnt, ctx->digcnt[1], ctx->digcnt[0], final);
//	if (final)
//		sfax8_sham_fill_padding(ctx, 0);

	if (final || (ctx->bufcnt == ctx->buflen && ctx->total)) {
		count = ctx->bufcnt;
		ctx->bufcnt = 0;
		return sfax8_sham_xmit_dma_map(dd, ctx, count, final);
	}

	return 0;
}

static int sfax8_sham_update_dma_start(struct sfax8_sham_dev *dd)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);
	unsigned int length, final, tail;
	struct scatterlist *sg;
	unsigned int count;

	if (!ctx->total)
		return 0;

//	data_size = ctx->total * 8;
//	sfax8_sham_write(dd, CRYPTO_HASH_MSG_SIZE_LOW, (u32)data_size);
//	sfax8_sham_write(dd, CRYPTO_HASH_MSG_SIZE_HIGH, (u32)(data_size >> 32));
	if (ctx->bufcnt || ctx->offset)
		return sfax8_sham_update_dma_slow(dd);

	dev_dbg(dd->dev, "fast: digcnt: 0x%llx 0x%llx, bufcnt: %u, total: %u\n",
		ctx->digcnt[1], ctx->digcnt[0], ctx->bufcnt, ctx->total);

	sg = ctx->sg;

	if (!IS_ALIGNED(sg->offset, sizeof(u32)))
		return sfax8_sham_update_dma_slow(dd);

	if (!sg_is_last(sg) && !IS_ALIGNED(sg->length, ctx->block_size))
		/* size is not ctx->block_size aligned */
		return sfax8_sham_update_dma_slow(dd);

	//printk("%s: ctx->total : %d, sg->length : %d\n", ctx->total, sg->length);
	length = min(ctx->total, sg->length);
   // printk("%s: length : %d\n", length);
	if (sg_is_last(sg)) {
		if (!(ctx->flags & SHAM_FLAGS_FINUP)) {
			/* not last sg must be ctx->block_size aligned */
			tail = length & (ctx->block_size - 1);
			length -= tail;
		}
	}

	ctx->total -= length;
	ctx->offset = length; /* offset where to start slow */
   // printk("%s: ctx->total : %d, ctx->offset : %d\n", __func__, ctx->total, ctx->offset);
	final = (ctx->flags & SHAM_FLAGS_FINUP) && !ctx->total;

	/* Add padding */
	if (final) {
		//tail = length & (ctx->block_size - 1);
		//length -= tail;
		//ctx->total += tail;
		//ctx->offset = length; /* offset where to start slow */
		sg = ctx->sg;
		sfax8_sham_append_sg(ctx);

		//sfax8_sham_fill_padding(ctx, length);

		ctx->dma_addr = dma_map_single(dd->dev, ctx->buffer,
			ctx->buflen + ctx->block_size, DMA_TO_DEVICE);
		if (dma_mapping_error(dd->dev, ctx->dma_addr)) {
			dev_err(dd->dev, "dma %u bytes error\n",
				ctx->buflen + ctx->block_size);
			return -EINVAL;
		}

		if (length == 0) {
			ctx->flags &= ~SHAM_FLAGS_SG;
			count = ctx->bufcnt;
			ctx->bufcnt = 0;
			return sfax8_sham_xmit_start(dd, ctx->dma_addr, count, 0,
					0, final);
		} else {
			ctx->sg = sg;
			if (!dma_map_sg(dd->dev, ctx->sg, 1,
				DMA_TO_DEVICE)) {
					dev_err(dd->dev, "dma_map_sg  error\n");
					return -EINVAL;
			}

			ctx->flags |= SHAM_FLAGS_SG;

			count = ctx->bufcnt;
			ctx->bufcnt = 0;
			return sfax8_sham_xmit_start(dd, sg_dma_address(ctx->sg),
					length, ctx->dma_addr, count, final);
		}
	}

	if (!dma_map_sg(dd->dev, ctx->sg, 1, DMA_TO_DEVICE)) {
		dev_err(dd->dev, "dma_map_sg  error\n");
		return -EINVAL;
	}

	ctx->flags |= SHAM_FLAGS_SG;

	/* next call does not fail... so no unmap in the case of error */
	return sfax8_sham_xmit_start(dd, sg_dma_address(ctx->sg), length, 0,
								0, final);
}

static int sfax8_sham_update_dma_stop(struct sfax8_sham_dev *dd)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(dd->req);

	if (ctx->flags & SHAM_FLAGS_SG) {
		dma_unmap_sg(dd->dev, ctx->sg, 1, DMA_TO_DEVICE);
		if (ctx->sg->length == ctx->offset) {
			ctx->sg = sg_next(ctx->sg);
			if (ctx->sg)
				ctx->offset = 0;
		}
		if (ctx->flags & SHAM_FLAGS_PAD) {
			dma_unmap_single(dd->dev, ctx->dma_addr,
				ctx->buflen + ctx->block_size, DMA_TO_DEVICE);
		}
	} else {
		dma_unmap_single(dd->dev, ctx->dma_addr, ctx->buflen +
						ctx->block_size, DMA_TO_DEVICE);
	}

	return 0;
}

static int sfax8_sham_update_req(struct sfax8_sham_dev *dd)
{
	struct ahash_request *req = dd->req;
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	int err;

	dev_dbg(dd->dev, "update_req: total: %u, digcnt: 0x%llx 0x%llx\n",
		ctx->total, ctx->digcnt[1], ctx->digcnt[0]);

	if (ctx->flags & SHAM_FLAGS_CPU)
		err = sfax8_sham_update_cpu(dd);
	else
		err = sfax8_sham_update_dma_start(dd);

	/* wait for dma completion before can take more data */
	dev_dbg(dd->dev, "update: err: %d, digcnt: 0x%llx 0%llx\n",
			err, ctx->digcnt[1], ctx->digcnt[0]);

	return err;
}

static int sfax8_sham_final_req(struct sfax8_sham_dev *dd)
{
	struct ahash_request *req = dd->req;
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	int err = 0;
	int count;

	if (ctx->bufcnt >= SFAX8_SHAM_DMA_THRESHOLD) {
		//sfax8_sham_fill_padding(ctx, 0);
		count = ctx->bufcnt;
		ctx->bufcnt = 0;
		err = sfax8_sham_xmit_dma_map(dd, ctx, count, 1);
	}
	/* faster to handle last block with cpu */
	else {
		//sfax8_sham_fill_padding(ctx, 0);
		count = ctx->bufcnt;
		ctx->bufcnt = 0;
		err = sfax8_sham_xmit_cpu(dd, ctx->buffer, count, 1);
	}

	dev_dbg(dd->dev, "final_req: err: %d\n", err);

	return err;
}

static void sfax8_sham_copy_hash(struct ahash_request *req)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	u32 *hash = (u32 *)ctx->digest;
	int i;
	if (ctx->flags & SHAM_FLAGS_SHAM1)
		for (i = 0; i < SHA1_DIGEST_SIZE / sizeof(u32); i++)
			hash[i] = sfax8_sham_read(ctx->dd, CRYPTO_HASH_RESULT(i));
	else if (ctx->flags & SHAM_FLAGS_SHAM224)
		for (i = 0; i < SHA224_DIGEST_SIZE / sizeof(u32); i++)
			hash[i] = sfax8_sham_read(ctx->dd, CRYPTO_HASH_RESULT(i));
	else if (ctx->flags & SHAM_FLAGS_SHAM256)
		for (i = 0; i < SHA256_DIGEST_SIZE / sizeof(u32); i++)
			hash[i] = sfax8_sham_read(ctx->dd, CRYPTO_HASH_RESULT(i));
	else if (ctx->flags & SHAM_FLAGS_MD5)
		for (i = 0; i < MD5_DIGEST_SIZE / sizeof(u32); i++)
			hash[i] = sfax8_sham_read(ctx->dd, CRYPTO_HASH_RESULT(i));
}

static void sfax8_sham_copy_ready_hash(struct ahash_request *req)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);

	if (!req->result)
		return;

	if (ctx->flags & SHAM_FLAGS_SHAM1)
		memcpy(req->result, ctx->digest, SHA1_DIGEST_SIZE);
	else if (ctx->flags & SHAM_FLAGS_SHAM224)
		memcpy(req->result, ctx->digest, SHA224_DIGEST_SIZE);
	else if (ctx->flags & SHAM_FLAGS_SHAM256)
		memcpy(req->result, ctx->digest, SHA256_DIGEST_SIZE);
	else if (ctx->flags & SHAM_FLAGS_MD5)
		memcpy(req->result, ctx->digest, MD5_DIGEST_SIZE);
}

static int sfax8_sham_finish(struct ahash_request *req)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	struct sfax8_sham_dev *dd = ctx->dd;
	int err = 0;

//	if (ctx->digcnt[0] || ctx->digcnt[1])
		sfax8_sham_copy_ready_hash(req);

	dev_dbg(dd->dev, "digcnt: 0x%llx 0x%llx, bufcnt: %d\n", ctx->digcnt[1],
		ctx->digcnt[0], ctx->bufcnt);

	return err;
}

static void sfax8_sham_finish_req(struct ahash_request *req, int err)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	struct sfax8_sham_dev *dd = ctx->dd;
	u32 tmp = 0;

	if (!err) {
		sfax8_sham_copy_hash(req);
		if (SHAM_FLAGS_FINAL & dd->flags)
			err = sfax8_sham_finish(req);
	} else {
		ctx->flags |= SHAM_FLAGS_ERROR;
	}

	/* atomic operation is not needed here */
	dd->flags &= ~(SHAM_FLAGS_BUSY | SHAM_FLAGS_FINAL | SHAM_FLAGS_CPU |
			SHAM_FLAGS_DMA_READY | SHAM_FLAGS_OUTPUT_READY);

	clk_disable_unprepare(dd->iclk);
	tmp = sfax8_sham_read(dd, CRYPTO_CG_CFG);
	tmp &= ~HASH_CLK;
	sfax8_sham_write(dd, CRYPTO_CG_CFG, tmp);

	if (req->base.complete)
		req->base.complete(&req->base, err);

	/* handle new request */
	tasklet_schedule(&dd->done_task);
}

static int sfax8_sham_hw_init(struct sfax8_sham_dev *dd)
{
	u32 tmp = 0;
	clk_prepare_enable(dd->iclk);
	
	if(hold_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;
	if(release_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;

	tmp = sfax8_sham_read(dd, CRYPTO_CG_CFG);
	tmp |= HASH_CLK;
	sfax8_sham_write(dd, CRYPTO_CG_CFG, tmp);
    sfax8_sham_write(dd, CRYPTO_HASH_FIFO_MODE_EN, DISABLE);
	sfax8_sham_write(dd, CRYPTO_HASH_CONTROL, SHAM_HASH_ENABLE);

	if (!(SHAM_FLAGS_INIT & dd->flags)) {
		/*hw reset*/
		//sfax8_sham_write(dd, SHAM_CR, SHAM_CR_SWRST);
		tmp = sfax8_sham_read(dd, CRYPTO_HASH_CONTROL);
		tmp |= SHAM_HASH_CLEAR;
		sfax8_sham_write(dd, CRYPTO_HASH_CONTROL, tmp);
		dd->flags |= SHAM_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}

/*
static inline unsigned int sfax8_sham_get_version(struct sfax8_sham_dev *dd)
{
	return sfax8_sham_read(dd, SHAM_HW_VERSION) & 0x00000fff;
}
*/
/*

static void sfax8_sham_hw_version_init(struct sfax8_sham_dev *dd)
{
	sfax8_sham_hw_init(dd);

	dd->hw_version = sfax8_sham_get_version(dd);

	dev_info(dd->dev,
			"version: 0x%x\n", dd->hw_version);

	clk_disable_unprepare(dd->iclk);
}
*/
static int sfax8_sham_handle_queue(struct sfax8_sham_dev *dd,
				  struct ahash_request *req)
{
	struct crypto_async_request *async_req, *backlog;
	struct sfax8_sham_reqctx *ctx;
	unsigned long flags;
	int err = 0, ret = 0;
	spin_lock_irqsave(&dd->lock, flags);
	if (req)
		ret = ahash_enqueue_request(&dd->queue, req);

	if (SHAM_FLAGS_BUSY & dd->flags) {
		spin_unlock_irqrestore(&dd->lock, flags);
		return ret;
	}

	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (async_req)
		dd->flags |= SHAM_FLAGS_BUSY;

	spin_unlock_irqrestore(&dd->lock, flags);
	if (!async_req)
		return ret;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ahash_request_cast(async_req);
	dd->req = req;
	ctx = ahash_request_ctx(req);

	dev_dbg(dd->dev, "handling new req, op: %lu, nbytes: %d\n",
						ctx->op, req->nbytes);

	err = sfax8_sham_hw_init(dd);

	if (err)
		goto err1;

	if (ctx->op == SHAM_OP_UPDATE) {
		err = sfax8_sham_update_req(dd);
		if (err != -EINPROGRESS && (ctx->flags & SHAM_FLAGS_FINUP))
			/* no final() after finup() */
			err = sfax8_sham_final_req(dd);
	} else if (ctx->op == SHAM_OP_FINAL) {
		err = sfax8_sham_final_req(dd);
	}

err1:
	if (err != -EINPROGRESS)
		/* done_task will not finish it, so do it here */
		sfax8_sham_finish_req(req, err);

	dev_dbg(dd->dev, "exit, err: %d\n", err);

	return ret;
}

static int sfax8_sham_enqueue(struct ahash_request *req, unsigned int op)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	struct sfax8_sham_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct sfax8_sham_dev *dd = tctx->dd;
	ctx->op = op;

	return sfax8_sham_handle_queue(dd, req);
}

static int sfax8_sham_update(struct ahash_request *req)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	/*
	if (!req->nbytes)
		return 0;
	*/
	ctx->total = req->nbytes;
	ctx->sg = req->src;
	ctx->offset = 0;

	if (ctx->flags & SHAM_FLAGS_FINUP) {
		if (ctx->bufcnt + ctx->total < SHAM_DMA_THRESHOLD)
			/* faster to use CPU for short transfers */
			ctx->flags |= SHAM_FLAGS_CPU;
	} else if (ctx->bufcnt + ctx->total < ctx->buflen) {
			sfax8_sham_append_sg(ctx);
		return 0;
	}
	return sfax8_sham_enqueue(req, SHAM_OP_UPDATE);
}

static int sfax8_sham_final(struct ahash_request *req)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
//	struct sfax8_sham_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct sfax8_sham_dev *dd = ctx->dd;

	int err = 0;

	ctx->flags |= SHAM_FLAGS_FINUP;

	if (ctx->flags & SHAM_FLAGS_ERROR)
		return 0; /* uncompleted hash is not needed */

	if (ctx->bufcnt) {
		return sfax8_sham_enqueue(req, SHAM_OP_FINAL);
	} else if (!(ctx->flags & SHAM_FLAGS_PAD)) { /* add padding */
		err = sfax8_sham_hw_init(dd);
		if (err)
			goto err1;

		dd->flags |= SHAM_FLAGS_BUSY;
		err = sfax8_sham_final_req(dd);
	} else {
		/* copy ready hash (+ finalize hmac) */
		return sfax8_sham_finish(req);
	}

err1:
	if (err != -EINPROGRESS)
		/* done_task will not finish it, so do it here */
		sfax8_sham_finish_req(req, err);

	return err;
}

static int sfax8_sham_finup(struct ahash_request *req)
{
	struct sfax8_sham_reqctx *ctx = ahash_request_ctx(req);
	int err1, err2;

	ctx->flags |= SHAM_FLAGS_FINUP;

	err1 = sfax8_sham_update(req);
	if (err1 == -EINPROGRESS || err1 == -EBUSY){
		return err1;
	}

	/*
	 * final() has to be always called to cleanup resources
	 * even if udpate() failed, except EINPROGRESS
	 */
	err2 = sfax8_sham_final(req);

	return err1 ?: err2;
}

static int sfax8_sham_digest(struct ahash_request *req)
{
	return sfax8_sham_init(req) ?: sfax8_sham_finup(req);
}
static int sfax8_sham_setkey(struct crypto_ahash *tfm, const u8 *key,
				unsigned int keylen)
{
	/*setkey to do
	*
	*
	*
	*
	*/
	struct sfax8_sham_ctx *tctx = crypto_ahash_ctx(tfm);
	/*the largest support keylen is 64*/
	if(keylen > 64){
        pr_err("%s, Keylen is over the limit of hw", __func__);
		return -EINVAL;
    }
	tctx->key = kmemdup(key, keylen, GFP_KERNEL);
	if(!tctx->key){
		pr_err("%s: Failed to allocate tctx->key\n", __func__);
		return -ENOMEM;
	}
	tctx->keylen = keylen;
		
	tctx->flags |= SHAM_FLAGS_HMAC;

	return 0;	
}

static int sfax8_sham_cra_init_alg(struct crypto_tfm *tfm, const char *alg_base)
{
	//struct sfax8_sham_ctx *tctx = crypto_tfm_ctx(tfm);
	/*
	struct hash_ctx *ctx = crypto_tfm_ctx(tfm);
	struct crypto_alg *alg = tfm->__crt_alg;
	struct thash_algo_template *hash_alg;
	*/
//	const char *alg_name = crypto_tfm_alg_name(tfm);
	/*
	hash_alg = container_of(__crypto_ahash_alg(alg),
			struct hash_algo_template, hash);
	*/
	/* Allocate a fallback and abort if it failed. */
	/*
	tctx->fallback = crypto_alloc_shash(alg_name, 0,
					    CRYPTO_ALG_NEED_FALLBACK);
	if (IS_ERR(tctx->fallback)) {
		pr_err("sfax8-hash: fallback driver '%s' could not be loaded.\n",
				alg_name);
		return PTR_ERR(tctx->fallback);
	}
	*/
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct sfax8_sham_reqctx) +
				 SHAM_BUFFER_LEN + SHA512_BLOCK_SIZE);
	/*
	if(strstr(alg_name, "hmac"))
		tctx->flags |= SHAM_FLAGS_HMAC;
	*/
	return 0;
}

static int sfax8_sham_cra_init(struct crypto_tfm *tfm)
{
	return sfax8_sham_cra_init_alg(tfm, NULL);
}

static void sfax8_sham_cra_exit(struct crypto_tfm *tfm)
{
//	struct sfax8_sham_ctx *tctx = crypto_tfm_ctx(tfm);

	//crypto_free_shash(tctx->fallback);
	//tctx->fallback = NULL;
}

static struct ahash_alg sham_algs[] = {
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.halg = {
		.digestsize	= SHA1_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "sha1",
			.cra_driver_name	= "sfax8-sha1",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA1_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.halg = {
		.digestsize	= SHA224_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "sha224",
			.cra_driver_name	= "sfax8-sham224",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA224_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.halg = {
		.digestsize	= SHA256_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "sha256",
			.cra_driver_name	= "sfax8-sha256",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA256_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.halg = {
		.digestsize	= MD5_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "md5",
			.cra_driver_name	= "sfax8-md5",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA1_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.setkey	= sfax8_sham_setkey,
	.halg = {
		.digestsize	= SHA1_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "hmac(sha1)",
			.cra_driver_name	= "sfax8-hmac-sha1",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA1_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.setkey	= sfax8_sham_setkey,
	.halg = {
		.digestsize	= SHA224_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "hmac(sha224)",
			.cra_driver_name	= "sfax8-hmac-sha224",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA224_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.setkey	= sfax8_sham_setkey,
	.halg = {
		.digestsize	= SHA256_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "hmac(sha256)",
			.cra_driver_name	= "sfax8-hmac-sha256",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA256_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
{
	.init		= sfax8_sham_init,
	.update		= sfax8_sham_update,
	.final		= sfax8_sham_final,
	.finup		= sfax8_sham_finup,
	.digest		= sfax8_sham_digest,
	.setkey	= sfax8_sham_setkey,
	.halg = {
		.digestsize	= MD5_DIGEST_SIZE,
		.statesize = sizeof(struct sfax8_sham_ctx),
		.base	= {
			.cra_name		= "hmac(md5)",
			.cra_driver_name	= "sfax8-hmac-md5",
			.cra_priority		= 100,
			.cra_flags		= CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_NEED_FALLBACK,
			.cra_blocksize		= SHA1_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct sfax8_sham_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= sfax8_sham_cra_init,
			.cra_exit		= sfax8_sham_cra_exit,
		}
	}
},
};


static void sfax8_sham_done_task(unsigned long data)
{
	struct sfax8_sham_dev *dd = (struct sfax8_sham_dev *)data;
	int err = 0;

	if (!(SHAM_FLAGS_BUSY & dd->flags)) {
		sfax8_sham_handle_queue(dd, NULL);
		return;
	}

	if (SHAM_FLAGS_CPU & dd->flags) {
		if (SHAM_FLAGS_OUTPUT_READY & dd->flags) {
			dd->flags &= ~SHAM_FLAGS_OUTPUT_READY;
			goto finish;
		}
	} else if (SHAM_FLAGS_DMA_READY & dd->flags) {
		if (SHAM_FLAGS_DMA_ACTIVE & dd->flags) {
			dd->flags &= ~SHAM_FLAGS_DMA_ACTIVE;
			sfax8_sham_update_dma_stop(dd);
			if (dd->err) {
				err = dd->err;
				goto finish;
			}
		}
		if (SHAM_FLAGS_OUTPUT_READY & dd->flags) {
			/* hash or semi-hash ready */
			dd->flags &= ~(SHAM_FLAGS_DMA_READY |
						SHAM_FLAGS_OUTPUT_READY);
			err = sfax8_sham_update_dma_start(dd);
			if (err != -EINPROGRESS)
				goto finish;
		}
	}
	return;

finish:
	/* finish curent request */
	sfax8_sham_finish_req(dd->req, err);
}

static irqreturn_t sfax8_sham_irq(int irq, void *dev_id)
{
	struct sfax8_sham_dev *sham_dd = dev_id;
	u32 reg;
	reg = sfax8_sham_read(sham_dd, CRYPTO_HASH_INT_STAT);
	// clear interrupt
	sfax8_sham_write(sham_dd, CRYPTO_HASH_INT_MASK, 0x3F);
	sfax8_sham_write(sham_dd, CRYPTO_HASH_INT_STAT, reg);
	if(reg & SHAM_HASH_MESSAGE_DONE){
		if(SHAM_FLAGS_BUSY & sham_dd->flags){ 
			sham_dd->flags |= SHAM_FLAGS_OUTPUT_READY;
			if(!(SHAM_FLAGS_CPU & sham_dd->flags))
				sham_dd->flags |= SHAM_FLAGS_DMA_READY;
			tasklet_schedule(&sham_dd->done_task);
		} else {
			dev_warn(sham_dd->dev, "SHAM interrupt when no active requests.\n");
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static void sfax8_sham_unregister_algs(struct sfax8_sham_dev *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sham_algs); i++)
		crypto_unregister_ahash(&sham_algs[i]);

}

static int sfax8_sham_register_algs(struct sfax8_sham_dev *dd)
{
	int err, i, j;

	for (i = 0; i < ARRAY_SIZE(sham_algs); i++) {
		err = crypto_register_ahash(&sham_algs[i]);
		if (err)
			goto err_sham_algs;
	}


	return 0;

err_sham_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_ahash(&sham_algs[j]);

	return err;
}


#if defined(CONFIG_OF)
static const struct of_device_id sfax8_sham_dt_ids[] = {
	{ .compatible = "siflower,sfax8-sham" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, sfax8_sham_dt_ids);
/*
static struct crypto_platform_data *sfax8_sham_of_init(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct crypto_platform_data *pdata;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "could not allocate memory for pdata\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->dma_slave = devm_kzalloc(&pdev->dev,
					sizeof(*(pdata->dma_slave)),
					GFP_KERNEL);
	if (!pdata->dma_slave) {
		dev_err(&pdev->dev, "could not allocate memory for dma_slave\n");
		return ERR_PTR(-ENOMEM);
	}

	return pdata;
}
*/
#else /* CONFIG_OF */
static inline struct crypto_platform_data *sfax8_sham_of_init(struct platform_device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif

static int sfax8_sham_probe(struct platform_device *pdev)
{
	struct sfax8_sham_dev *sham_dd;
//	struct crypto_platform_data	*pdata;
	struct device *dev = &pdev->dev;
	struct resource *sham_res;
	unsigned long sham_phys_size;
	int err;

	if(hold_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;
	if(release_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;
	

	sham_dd = devm_kzalloc(&pdev->dev, sizeof(struct sfax8_sham_dev),
				GFP_KERNEL);
	if (sham_dd == NULL) {
		dev_err(dev, "unable to alloc data struct.\n");
		err = -ENOMEM;
		goto sham_dd_err;
	}

	sham_dd->dev = dev;

	platform_set_drvdata(pdev, sham_dd);

	INIT_LIST_HEAD(&sham_dd->list);

	tasklet_init(&sham_dd->done_task, sfax8_sham_done_task,
					(unsigned long)sham_dd);

	crypto_init_queue(&sham_dd->queue, SF_SHAM_QUEUE_LENGTH);

	sham_dd->irq = -1;

	/* Get the base address */
	sham_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!sham_res) {
		dev_err(dev, "no MEM resource info\n");
		err = -ENODEV;
		goto res_err;
	}
	sham_dd->phys_base = sham_res->start;
	sham_phys_size = resource_size(sham_res);

	/* Get the IRQ */
	sham_dd->irq = platform_get_irq(pdev,  0);
	if (sham_dd->irq < 0) {
		dev_err(dev, "no IRQ resource info\n");
		err = sham_dd->irq;
		goto res_err;
	}

	err = request_irq(sham_dd->irq, sfax8_sham_irq, IRQF_SHARED, "sfax8-sham",
						sham_dd);
	if (err) {
		dev_err(dev, "unable to request sham irq.\n");
		goto res_err;
	}

	/* Initializing the clock */
	
	sham_dd->iclk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sham_dd->iclk)) {
		dev_err(dev, "clock intialization failed.\n");
		err = PTR_ERR(sham_dd->iclk);
		goto res_err;
	}
	clk_prepare_enable(sham_dd->iclk);

	sham_dd->bus_clk = of_clk_get((&pdev->dev)->of_node, 1);
	if (IS_ERR(sham_dd->bus_clk)) {
		dev_err(dev, "clock intialization failed.\n");
		err = PTR_ERR(sham_dd->bus_clk);
		goto res_err;
	}
	clk_prepare_enable(sham_dd->bus_clk);
	

	sham_dd->io_base = ioremap(sham_dd->phys_base, sham_phys_size);
	if (!sham_dd->io_base) {
		dev_err(dev, "can't ioremap\n");
		err = -ENOMEM;
		goto sham_io_err;
	}

	sfax8_sham_hw_init(sham_dd);

	spin_lock(&sfax8_sham.lock);
	list_add_tail(&sham_dd->list, &sfax8_sham.dev_list);
	spin_unlock(&sfax8_sham.lock);

	err = sfax8_sham_register_algs(sham_dd);
	if (err)
		goto err_algs;

	return 0;

err_algs:
	spin_lock(&sfax8_sham.lock);
	list_del(&sham_dd->list);
	spin_unlock(&sfax8_sham.lock);
sham_io_err:
	iounmap(sham_dd->io_base);
	free_irq(sham_dd->irq, sham_dd);
res_err:
	tasklet_kill(&sham_dd->done_task);
sham_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int sfax8_sham_remove(struct platform_device *pdev)
{
	static struct sfax8_sham_dev *sham_dd;
	sham_dd = platform_get_drvdata(pdev);
	if (!sham_dd)
		return -ENODEV;
	spin_lock(&sfax8_sham.lock);
	list_del(&sham_dd->list);
	spin_unlock(&sfax8_sham.lock);

	sfax8_sham_unregister_algs(sham_dd);

	tasklet_kill(&sham_dd->done_task);
/*
	if (sham_dd->caps.has_dma)
		sfax8_sham_dma_cleanup(sham_dd);
*/
	iounmap(sham_dd->io_base);

//	clk_put(sham_dd->iclk);

	if (sham_dd->irq >= 0)
		free_irq(sham_dd->irq, sham_dd);

	clk_disable_unprepare(sham_dd->iclk);
	clk_disable_unprepare(sham_dd->bus_clk);
	hold_reset(SF_CRYPTO_SOFT_RESET);
	return 0;
}

static struct platform_driver sfax8_sham_driver = {
	.probe		= sfax8_sham_probe,
	.remove		= sfax8_sham_remove,
	.driver		= {
		.name	= "sfax8_sham",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(sfax8_sham_dt_ids),
	},
};

module_platform_driver(sfax8_sham_driver);

MODULE_DESCRIPTION("Siflower SHAM1/224/256(HMAC) and MD5 hw acceleration support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("chang.li@siflower.com.cn");
