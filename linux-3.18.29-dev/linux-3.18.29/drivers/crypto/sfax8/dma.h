/*
 * Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DMA_H_
#define _DMA_H_
#define DMA_HASH_BURST_LEN	(16)
#define DMA_BLKC_BURST_LEN	(4)
#define DMA_NO_INT	(0 << 8)
#define DMA_EN_BUF_BOUND	(1 << 3)
#define DMA_TWO_CH_MODE		(1 << 2)
#define DMA_MODE_ADMA		(1 << 1)
#define DMA_EN_ADMA			(1 << 0)
#define DMA_DEFAULT_MASK	(0x3F)
#define DMA_DONE_INT		(0x37)
#define DMA_START			(1 << 0)
#define DMA_SDMA_FINISH		(1 << 3)

static inline u32 sfax8_addr_to_dma_addr(u32 addr)
{

	return ((addr & ~0xf0000000 )| 0x40000000);
}

#endif /* _DMA_H_ */
