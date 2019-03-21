/*
 * include/video/sfax8_fb.h
 *
 * Platform data header for Siflower Ax8 serials Soc frame buffer
 *
 * Copyright (c) 2017 Shanghai Siflower Communication Technology Co., Ltd.
 *
 * Qi Zhang <qi.zhang@siflower.com.>
 */

#ifndef __SFAX8_FB_H_
#define __SFAX8_FB_H_

#include <uapi/video/sfax8fb.h>

#define GDU0_REG(x)		(x)
#define OVCDCR			GDU0_REG(0x1000)
#define OVCPCR			GDU0_REG(0x1004)
#define OVCBKCOLOR		GDU0_REG(0x1008)
#define OVCWPR			GDU0_REG(0x100c)
#define OVCW0CR			GDU0_REG(0x1080)
#define OVCW0PCAR		GDU0_REG(0x1084)
#define OVCW0PCBR		GDU0_REG(0x1088)
#define OVCW0VSSR		GDU0_REG(0x108c)
#define OVCW0CMR		GDU0_REG(0x1090)
#define OVCW0B0SAR		GDU0_REG(0x1094)
#define OVCW0B1SAR		GDU0_REG(0x1098)
#define OVCW0B2SAR		GDU0_REG(0x109c)
#define OVCW0B3SAR		GDU0_REG(0x10a0)
#define OVCW1CR			GDU0_REG(0x1100)
#define OVCW1PCAR		GDU0_REG(0x1104)
#define OVCW1PCBR		GDU0_REG(0x1108)
#define OVCW1PCCR		GDU0_REG(0x110c)
#define OVCW1VSSR		GDU0_REG(0x1110)
#define OVCW1CKCR		GDU0_REG(0x1114)
#define OVCW1CKR		GDU0_REG(0x1118)
#define OVCW1CMR		GDU0_REG(0x111c)
#define OVCW1B0SAR		GDU0_REG(0x1120)
#define OVCW1B1SAR		GDU0_REG(0x1124)
#define OVCW1B2SAR		GDU0_REG(0x1128)
#define OVCW1B3SAR		GDU0_REG(0x112c)
#define OVCW0PAL		GDU0_REG(0x1400)
#define OVCW1PAL		GDU0_REG(0x1800)

// osd offset
#define OVCDCR_LOAD_PARA_EN     BIT(11)
#define OVCDCR_IFTYPE		(1)
#define OVCWxCR_ENWIN		BIT(0)

#define OVCOMC_ToRGB		(31)
#define OVCOMC_oft_b		(8)
#define OVCOMC_oft_a		(0)
#define BUFAUTOEN		BIT(16)
#define BUFSEL_SHIFT		17
#define BUF_NUM_SHIFT		14
#define RBEXG			BIT(6)
#define BPP_MODE_SHIFT		1
#define LEFT_TOP_Y_SHIFT	16
#define LEFT_TOP_X_SHIFT	0
#define RIGHT_BOT_Y_SHIFT	16
#define RIGHT_BOT_X_SHIFT	0
#define VW_WIDTH_SHIFT          0
#define MAPCOLEN		BIT(24)
#define MAP_COLOR(color)	((color) << 0)

#define UPDATE_PAL              BIT(15)
#define W1PALFM                 3
#define W0PALFM                 0
#define ALPHA_SEL		(8)
#define BLD_PIX			BIT(7)
#define ALPHA0_SHIFT		12
#define ALPHA1_SHIFT		0
#define KEYBLEN			BIT(26)
#define KEYEN			BIT(25)
#define DIRCON			BIT(24)
#define COMPKEY_SHIFT		0

// lcd reg & offset
#define LCDCON1			GDU0_REG(0x000) //LCD control 1
#define LCDCON2			GDU0_REG(0x004) //LCD control 2
#define LCDCON3			GDU0_REG(0x008) //LCD control 3
#define LCDCON4			GDU0_REG(0x00c) //LCD control 4
#define LCDCON5			GDU0_REG(0x010) //LCD control 5
#define LCDCON6			GDU0_REG(0x018)
#define LCDVCLKFSR		GDU0_REG(0x030)
#define GDUINTPND		GDU0_REG(0x054) //LCD Interrupt pending
#define GDUSRCPND		GDU0_REG(0x058) //LCD Interrupt source
#define GDUINTMASK		GDU0_REG(0x05c) //LCD Interrupt mask

/* interrupt bits */
#define OSDERR			BIT(4)
#define OSDERRMASK		BIT(4)
#define OSDW1INT		BIT(3)
#define OSDW1INTMASK		BIT(3)
#define OSDW0INT		BIT(2)
#define OSDW0INTMASK		BIT(2)
#define VCLKINT			BIT(1)
#define VCLKINTMASK		BIT(1)
#define LCDINT			BIT(0)
#define LCDINTMASK		BIT(0)

/* LCD control register 1 */
#define LCDCON1_LINECNT 18 // [29:18]
#define LCDCON1_CLKVAL 8   // [17:8]
#define LCDCON1_VMMODE 7	 // [7:7]
#define LCDCON1_PNRMODE 5
#define LCDCON1_STNBPP 1
#define LCDCON1_ENVID 0 // [0:0]

#define LCDCON2_VBPD 16 // [26:16]
#define LCDCON2_VFPD 0  // [10:0]

#define LCDCON3_VSPW 16 // [26:16]
#define LCDCON3_HSPW 0  // [10:0]

#define LCDCON4_HBPD 16 // [26:16]
#define LCDCON4_HFPD 0  // [10:0]

#define LCDCON5_RGB565IF BIT(31)
#define LCDCON5_RGBORDER 24    // [29:24]
#define LCDCON5_CONFIGORDER 20 // [22:20] 0->dsi24bpp, 1->dsi16bpp1, 2->dsi16bpp2,3->dsi16bpp3,4->dsi18bpp1,5->dsi18bpp2
#define LCDCON5_VSTATUS 15     // [16:15]
#define LCDCON5_HSTATUS 13     // [14:13]
#define LCDCON5_DSPTYPE 11     // [12:11]
#define LCDCON5_INVVCLK BIT(10)     // [10:10]
#define LCDCON5_INVHSYNC BIT(9)     // [9:9]
#define LCDCON5_INVVSYNC BIT(8)    // [8:8]
#define LCDCON5_INVVD BIT(7)	// [7:7]
#define LCDCON5_INVVDEN BIT(6)      // [6:6]
#define LCDCON5_INVPWREN BIT(5)     // [5:5]
#define LCDCON5_PWREN BIT(3)	// [3:3]

#define LCDCON6_LINEVAL 16 // [26:16]
#define LCDCON6_HOZVAL 0   // [10:0]

#define LCDVCLKFSR_CDOWN 24

#define DATASIGMAPPING_RGB 0x6  // [5:4]2b'00, [3:2]2b'01, [1:0]2b'10. RGB
#define DATASIGMAPPING_RBG 0x9  // [5:4]2b'00, [3:2]2b'10, [1:0]2b'01. RBG
#define DATASIGMAPPING_GRB 0x12 // [5:4]2b'01, [3:2]2b'00, [1:0]2b'10. GRB
#define DATASIGMAPPING_GBR 0x18 // [5:4]2b'01, [3:2]2b'10, [1:0]2b'00. GBR
#define DATASIGMAPPING_BRG 0x21 // [5:4]2b'10, [3:2]2b'00, [1:0]2b'01. BRG
#define DATASIGMAPPING_BGR 0x24 // [5:4]2b'10, [3:2]2b'01, [1:0]2b'00. BGR

#endif
