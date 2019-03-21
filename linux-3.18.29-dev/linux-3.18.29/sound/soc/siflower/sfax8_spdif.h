/* sound/soc/siflower/sfax8_spdif.h
 *
 * ALSA SoC Audio Layer - Siflower S/PDIF Controller driver
 *
 * Copyright (c) 2016 Shanghai Siflower Communication Technology Co., Ltd.
 *		http://www.siflower.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SFAX8_SPDIF_H
#define __SND_SOC_SFAX8_SPDIF_H

/* Registers */
#define CTRL		0x00
#define TX_CONFIG	0x04
#define RX_CONFIG	0x08
#define TXCHST		0x0C
#define RXCHST		0x10
#define FIFO_STAT	0x14
#define INT_MASK	0x18
#define INT_STAT	0x1C
#define TX_FIFO		0x20
#define RX_FIFO		0x24
#define TX_CHST_A(n)	(0x30 + n * 4)
#define TX_CHST_B(n)	(0x50 + n * 4)
#define TX_UDAT_A(n)	(0x70 + n * 4)
#define TX_UDAT_B(n)	(0x90 + n * 4)

/* CTRL */
#define CTRL_MASK		    0xff
#define CLK_EN			    (0x1 << 5)
#define RXFIFO_EN		    (0x1 << 4)
#define TXFIFO_EN		    (0x1 << 3)
#define RX_EN			    (0x1 << 2)
#define TX_EN			    (0x1 << 1)
#define SPDIF_EN		    (0x1 << 0)

/* TX_CONFIG */
#define TX_CONFIG_MASK		    0xfff200ff
#define TX_DIPSTICK_SHIFT	    0
#define TX_DIPSTICK_MASK	    (0x1f)
#define TX_VALIDITY			0x2
#define TX_VALIDITY_SHIFT	    5
#define TX_DMA_EN		    (0x1 << 7)
#define TX_USER_DATA_MODE_SHIFT	    8
#define TX_CHST_MODE_SHIFT	    10
#define TX_PARITY_MODE		    (0x1 << 12)
#define TX_VALIDITY_MODE	    (0x1 << 13)
#define TX_AUX_EN		    (0x1 << 14)
#define TX_DUPLICATE		    (0x1 << 16)
#define TX_ONLY_CHANNEL_A	    (0x1 << 17)
#define TX_SWAP_SHIFT		    18
#define TX_RATIO_SHIFT		    20
#define TX_MODE_SHIFT		    28

/* RX_CONFIG */
#define RX_DIPSTICK_SHIFT	    0
#define RX_MODE_SHIFT		    5
#define RX_DMA_EN		    (0x1 << 16)
#define RX_USER_DATA_MODE_SHIFT	    18
#define RX_CHST_MODE_SHIFT	    20
#define RX_PARITY_CHECK_EN	    (0x1 << 22)
#define RX_ALWAYS_STORE_DATA	    (0x1 << 23)
#define RX_STORE_VALIDITY_BIT	    (0x1 << 24)
#define RX_STORE_USER_DATA_BIT	    (0x1 << 25)
#define RX_STORE_CHST_BIT	    (0x1 << 26)
#define RX_STORE_PARITY_BIT	    (0x1 << 27)
#define RX_STORE_BLOCK_START_MARK   (0x1 << 28)
#define RX_STORE_CHANNEL_ID	    (0x1 << 29)
#define RX_SWAP_SHIFT		    30

/* TXCHST */
#define TXCHST_MASK		    0x3000
#define TX_PCM_FORMAT		    (0x1 << 0)
#define TX_NO_COPYRIGHT		    (0x1 << 1)
#define TX_ADD_INFO		    (0x1 << 2)
#define TX_CATEGORY_MASK	    0xff
#define TX_CATEGORY_SHIFT	    4
#define TX_FREQUENCY_SHIFT	    12
#define TX_CLOCK_ACCURACY_SHIFT	    14

/* FIFO_STAT */
#define TX_FIFO_EMPTY		    (0x1 << 0)
#define TX_FIFO_FULL		    (0x1 << 1)
#define TX_FIFO_ALMOST_EMPTY	    (0x1 << 2)
#define TX_FIFO_ALMOST_FULL	    (0x1 << 3)
#define TX_FIFO_OVER_FLOW	    (0x1 << 4)
#define TX_FIFO_UNDER_FLOW	    (0x1 << 5)
#define RX_FIFO_EMPTY		    (0x1 << 6)
#define RX_FIFO_FULL		    (0x1 << 7)
#define RX_FIFO_ALMOST_EMPTY	    (0x1 << 8)
#define RX_FIFO_ALMOST_FULL	    (0x1 << 9)
#define RX_FIFO_OVER_FLOW	    (0x1 << 10)
#define RX_FIFO_UNDER_FLOW	    (0x1 << 11)
#define TX_FREE_COUNT_SHIFT	    12
#define RX_WORD_COUNT_SHIFT	    20

/* INT_MASK */
#define SPDIF_INT_MASK		    (0x1 << 0)
#define CHANGE_MODE_SHIFT	    24

/* INT_STAT */
#define SPDIF_INT		    (0x1 << 0)
#define TX_FIFO_EMPTY_INT	    (0x1 << 1)
#define TX_FIFO_FULL_INT	    (0x1 << 2)
#define TX_FIFO_ALMOST_EMPTY_INT    (0x1 << 3)
#define TX_FIFO_ALMOST_FULL_INT	    (0x1 << 4)
#define TX_FIFO_OVER_FLOW_INT	    (0x1 << 5)
#define TX_FIFO_UNDER_FLOW_INT	    (0x1 << 6)
#define RX_FIFO_EMPTY_INT	    (0x1 << 7)
#define RX_FIFO_FULL_INT	    (0x1 << 8)
#define RX_FIFO_ALMOST_EMPTY_INT    (0x1 << 9)
#define RX_FIFO_ALMOST_FULL_INT	    (0x1 << 10)
#define RX_FIFO_OVER_FLOW_INT	    (0x1 << 11)
#define RX_FIFO_UNDER_FLOW_INT	    (0x1 << 12)
#define CHB_PARITY_ERROR	    (0x1 << 14)
#define CHA_PARITY_ERROR	    (0x1 << 15)
#define RX_ERROR		    (0x1 << 16)
#define BLOCK_START		    (0x1 << 17)
#define CHANGE_MODE_INT_SHIFT	    20
#define CHB_USER_DATA_CHANGE_INT    (0x1 << 26)
#define CHA_USER_DATA_CHANGE_INT    (0x1 << 27)
#define CHB_CHST_CHANGE_INT	    (0x1 << 28)
#define CHA_CHST_CHANGE_INT	    (0x1 << 29)
#define CHST_UPDATE_INT		    (0x1 << 30)
#define USER_DATA_UPDATE_INT	    (0x1 << 31)

/* default value */
#define DEFAULT_FIFO_DEPTH	    8

#endif
