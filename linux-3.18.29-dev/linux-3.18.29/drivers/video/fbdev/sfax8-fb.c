/* linux/drivers/video/sfax8-fb.c
 *
 * Copyright 20017 Shanghai Siflower Communication Technology Co., Ltd.
 *      Qi Zhang <qi.zhang@siflower.com.cn>
 *      http://www.siflower.com
 *
 * Siflower Ax8 Serials SoC Framebuffer Driver
 *
 * This driver is based on s3c-fb.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sf16a18.h>

#include <video/sfax8_fb.h>

/* This driver will export a number of framebuffer interfaces depending
 * on the configuration passed in via the platform data. Each fb instance
 * maps to a hardware window. Currently there is no support for runtime
 * setting of the alpha-blending functions that each window has, so only
 * window 0 is actually useful.
 *
 * Window 0 is treated specially, it is used for the basis of the LCD
 * output timings and as the control for the output power-down state.
 */

/* note, the previous use of <mach/regs-fb.h> to get platform specific data
 * has been replaced by using the platform device name to pick the correct
 * configuration data for the system.
 */
//#define CONFIG_FB_SFAX8_DEBUG_REGWRITE
#ifdef CONFIG_FB_SFAX8_DEBUG_REGWRITE
#undef writel
#define writel(v, r)                                                           \
	do {                                                                   \
		pr_debug("%s: %08x => %p\n", __func__, (unsigned int)v, r);    \
		__raw_writel(v, r);                                            \
	} while (0)
#endif /* FB_SFAX8_DEBUG_REGWRITE */

/* irq_flags bits */
#define SFAX8_FB_VSYNC_IRQ_EN 0

#define VSYNC_TIMEOUT_MSEC 50
#define SFAX8_FB_MAX_WIN 2
#define SFAX8_FB_PALETTE_SIZE 256

struct sfax8_fb;

#define VALID_BPP(x) (1 << ((x)-1))
#define VALID_BPP124 (VALID_BPP(1) | VALID_BPP(2) | VALID_BPP(4))
#define VALID_BPP1248 (VALID_BPP124 | VALID_BPP(8))

#define select_reg(reg0, reg1) (index ? (reg1) : (reg0))

/**
 * struct sfax8_fb_palette - palette information
 * @r: Red bitfield.
 * @g: Green bitfield.
 * @b: Blue bitfield.
 * @a: Alpha bitfield.
 */
struct sfax8_fb_palette {
	struct fb_bitfield r;
	struct fb_bitfield g;
	struct fb_bitfield b;
	struct fb_bitfield a;
};

/**
 * struct sfax8_fb_win - per window private data for each framebuffer.
 * @windata: The platform data supplied for the window configuration.
 * @parent: The hardware that this window is part of.
 * @fbinfo: Pointer pack to the framebuffer info for this window.
 * @varint: The variant information for this window.
 * @pseudo_palette: For use in TRUECOLOUR modes for entries 0..255/
 * @index: The window number of this window.
 * @palette: The bitfields for changing r/g/b into a hardware palette entry.
 */
struct sfax8_fb_win {
	struct sfax8_fb *parent;
	struct fb_info *fbinfo;
	struct sfax8_fb_palette palette;

	u32 pseudo_palette[SFAX8_FB_PALETTE_SIZE];
	u32 valid_bpp;
	unsigned int index;
};

/**
 * struct sfax8_fb_vsync - vsync information
 * @wait:	a queue for processes waiting for vsync
 * @count:	vsync interrupt count
 */
struct sfax8_fb_vsync {
	wait_queue_head_t wait;
	unsigned int count;
};

/**
 * struct sfax8_fb - overall hardware state of the hardware
 * @lock: The spinlock protection for this data structure.
 * @dev: The device that we bound to, for printing, etc.
 * @bus_clk: The clk (hclk) feeding our interface and possibly pixclk.
 * @lcd_clk: The clk (sclk) feeding pixclk.
 * @regs: The mapped hardware registers.
 * @variant: Variant information for this hardware.
 * @enabled: A bitmask of enabled hardware windows.
 * @output_on: Flag if the physical output is enabled.
 * @windows: The hardware windows that have been claimed.
 * @irq_num: IRQ line number
 * @irq_flags: irq flags
 * @vsync_info: VSYNC-related information (count, queues...)
 */
struct sfax8_fb {
	spinlock_t lock;
	struct device *dev;
	struct clk *bus_clk;
	struct clk *lcd_clk;
	struct clk *bus1xn_clk;
	void __iomem *regs;
	int power_gpio[2];

	unsigned char enabled;
	bool output_on;

	int num_windows;
	struct sfax8_fb_win *windows[SFAX8_FB_MAX_WIN];
	int irq_num;
	unsigned long irq_flags;
	unsigned char rgb_order;
	enum fb_bpp_mode bpp_mode;
	unsigned char is_rgb565_interface;
	struct sfax8_fb_vsync vsync_info;
	struct fb_videomode vtiming;
};

/**
 * sfax8_fb_validate_win_bpp - validate the bits-per-pixel for this mode.
 * @win: The device window.
 * @bpp: The bit depth.
 */
static bool sfax8_fb_validate_win_bpp(struct sfax8_fb_win *win,
				      unsigned int bpp)
{
	return win->valid_bpp & VALID_BPP(bpp);
}

static int sfax8_fb_bpp_mode_to_bpp(enum fb_bpp_mode mode)
{
	switch (mode) {
	case rgb565:
		return 16;
	case rgb888:
		return 32;
	default:
		return -EINVAL;
	}
}

/**
 * sfax8_fb_check_var() - framebuffer layer request to verify a given mode.
 * @var: The screen information to verify.
 * @info: The framebuffer device.
 *
 * Framebuffer layer call to verify the given information and allow us to
 * update various information depending on the hardware capabilities.
 */
static int sfax8_fb_check_var(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;

	dev_dbg(sfb->dev, "checking parameters\n");

	var->xres_virtual = max(var->xres_virtual, var->xres);
	var->yres_virtual = max(var->yres_virtual, var->yres);

	if (!sfax8_fb_validate_win_bpp(win, var->bits_per_pixel)) {
		dev_dbg(sfb->dev, "win %d: unsupported bpp %d\n", win->index,
			var->bits_per_pixel);
		return -EINVAL;
	}

	/* always ensure these are zero, for drop through cases below */
	var->transp.offset = 0;
	var->transp.length = 0;

	switch (var->bits_per_pixel) {
	case 19:
		/* 666 with one bit alpha/transparency */
		var->transp.offset = 18;
		var->transp.length = 1;
	/* drop through */
	case 18:
		/* 666 format */
		var->red.offset = 12;
		var->green.offset = 6;
		var->blue.offset = 0;
		var->red.length = 6;
		var->green.length = 6;
		var->blue.length = 6;
		var->bits_per_pixel = 32;
		break;

	case 16:
		/* 16 bpp, 565 format */
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		break;

	case 28:
	case 25:
		var->transp.length = var->bits_per_pixel - 24;
		var->transp.offset = 24;
	/* drop through */
	case 24:
	case 32:
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->bits_per_pixel = 32;
		break;

	default:
		dev_err(sfb->dev, "invalid bpp\n");
		return -EINVAL;
	}

	dev_dbg(sfb->dev, "%s: verified parameters\n", __func__);
	return 0;
}

/**
 * sfax8_fb_calc_pixclk() - calculate the divider to create the pixel clock.
 * @sfb: The hardware state.
 * @pixclock: The pixel clock wanted, in picoseconds.
 *
 * Given the specified pixel clock, work out the necessary divider to get
 * close to the output frequency.
 */
static int sfax8_fb_calc_pixclk(struct sfax8_fb *sfb, unsigned int pixclk)
{
	unsigned long clk;
	unsigned long long tmp;
	unsigned int result;

	clk = clk_get_rate(sfb->bus_clk);

	tmp = (unsigned long long)clk;
	tmp *= pixclk;

	do_div(tmp, 1000000000UL);
	result = (unsigned int)tmp / 1000;

	dev_dbg(sfb->dev, "pixclk=%u, clk=%lu, div=%d (%lu)\n", pixclk, clk,
		result, result ? clk / result : clk);

	return result;
}
#if 0
/**
 * sfax8_fb_align_word() - align pixel count to word boundary
 * @bpp: The number of bits per pixel
 * @pix: The value to be aligned.
 *
 * Align the given pixel count so that it will start on an 32bit word
 * boundary.
 */
static int sfax8_fb_align_word(unsigned int bpp, unsigned int pix)
{
	int pix_per_word;

	if (bpp > 16)
		return pix;

	pix_per_word = (8 * 32) / bpp;
	return ALIGN(pix, pix_per_word);
}
#endif

/**
 * sfax8_fb_enable() - Set the state of the main LCD output
 * @sfb: The main framebuffer state.
 * @enable: The state to set.
 */
static void sfax8_lcd_enable(struct sfax8_fb *sfb, int enable)
{
	u32 lcdcon1 = readl(sfb->regs + LCDCON1);

	if (enable)
		lcdcon1 |= 0x1 << LCDCON1_ENVID;
	else
		lcdcon1 &= ~(0x1 << LCDCON1_ENVID);

	writel(lcdcon1, sfb->regs + LCDCON1);

	sfb->output_on = enable;
}

/**
 * sfax8_fb_set_alpha() - set alpha transparency for a window
 *
 * @win: the window to set alpha transparency for
 * @alpha: alpha value
 */
static int sfax8_fb_set_alpha(struct sfax8_fb_win *win,
			      struct sfax8_fb_alpha *alpha)
{
	struct sfax8_fb *sfb = win->parent;
	u32 data;
	u32 alpha_value;

	/* only win 1 has alpha setting*/
	if (win->index == 0)
		return -EINVAL;

	if (alpha->alpha_sel & (~1))
		return -EINVAL;

	sfax8_lcd_enable(sfb, 0);

	data = readl(sfb->regs + OVCW1CR);
	data &= ~(1 << ALPHA_SEL);
	if (alpha->blend_category == per_plane) {
		alpha_value = readl(sfb->regs + OVCW1PCCR);
		data &= ~BLD_PIX;
		if (alpha->alpha_sel == USING_ALPHA_1) {
			alpha->alpha_1 &= 0xfff;
			alpha_value &= ~(0xfff << ALPHA1_SHIFT);
			alpha_value |= alpha->alpha_1 << ALPHA1_SHIFT;
		} else if (alpha->alpha_sel == USING_ALPHA_0) {
			alpha->alpha_0 &= 0xfff;
			alpha_value &= ~(0xfff << ALPHA0_SHIFT);
			alpha_value |= alpha->alpha_0 << ALPHA1_SHIFT;
		}
		writel(alpha_value, sfb->regs + OVCW1PCCR);
	} else if (alpha->blend_category == per_pixel)
		data |= BLD_PIX;

	data |= (alpha->alpha_sel << ALPHA_SEL);
	writel(data, sfb->regs + OVCW1CR);

	sfax8_lcd_enable(sfb, 1);

	return 0;
}

/**
 * sfax8_fb_enable() - Set the state of the specific window.
 * @sfb: The main framebuffer state.
 * @enable: The state to set.
 */
static void sfax8_fb_enable(struct sfax8_fb_win *win, int enable)
{
	void __iomem *reg, *base = win->parent->regs;
	int index = win->index;
	u32 wincr;

	reg = base + select_reg(OVCW0CR, OVCW1CR);

	wincr = readl(reg);

	if (enable) {
		wincr |= OVCWxCR_ENWIN;
	} else {
		wincr &= ~OVCWxCR_ENWIN;
	}

	writel(wincr, reg);
}

static void sfax8_fb_set_win(struct fb_info *info)
{
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;
	struct fb_var_screeninfo *var = &info->var;
	void __iomem *reg, *base = sfb->regs;
	u32 data = 0;
	int index = win->index;

	/* set screen position */
	data = (0 << LEFT_TOP_X_SHIFT) | (0 << LEFT_TOP_Y_SHIFT);
	reg = base + select_reg(OVCW0PCAR, OVCW1PCAR);
	writel(data, reg);
	data = ((var->xres - 1) << RIGHT_BOT_X_SHIFT) |
	       ((var->yres - 1) << RIGHT_BOT_Y_SHIFT);
	reg = base + select_reg(OVCW0PCBR, OVCW1PCBR);
	writel(data, reg);

	/* set virtual screen page width in the unit of pixel */
	reg = base + select_reg(OVCW0VSSR, OVCW1VSSR);
	data = readl(reg);
	data &= ~(0xffff << VW_WIDTH_SHIFT);
	data |= var->xres_virtual << VW_WIDTH_SHIFT;
	writel(data, reg);

	data = sfb->bpp_mode << BPP_MODE_SHIFT;
	data |= OVCWxCR_ENWIN;
	reg = base + select_reg(OVCW0CR, OVCW1CR);
	writel(data, reg);
}

/**
 * sfax8_fb_set_par() - framebuffer request to set new framebuffer state.
 * @info: The framebuffer to change.
 *
 * Framebuffer layer request to set a new mode for the specified framebuffer
 */
static int sfax8_fb_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;
	void __iomem *regs = sfb->regs;

	dev_dbg(sfb->dev, "setting framebuffer parameters\n");

	/* disable lcd when change config */
	sfax8_lcd_enable(sfb, 0);

	switch (var->bits_per_pixel) {
	case 32:
	case 24:
	case 16:
	case 12:
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	case 8:
		if (SFAX8_FB_PALETTE_SIZE >= 256)
			info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		else
			info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	case 1:
		info->fix.visual = FB_VISUAL_MONO01;
		break;
	default:
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	}

	info->fix.line_length = var->xres * var->bits_per_pixel / 8;

	/* enable global parameter load */
	writel(OVCDCR_LOAD_PARA_EN, regs + OVCDCR);

	sfax8_fb_set_win(info);

	if (!(sfb->enabled & (1 << win->index)))
		sfax8_fb_enable(win, 1);

	/* disable global parameter load */
	// writel(0, regs + OVCDCR);

	/* Enable for this window */
	sfb->enabled |= (1 << win->index);

	sfax8_lcd_enable(sfb, 1);

	return 0;
}

/**
 * sfax8_fb_update_palette() - set or schedule a palette update.
 * @sfb: The hardware information.
 * @win: The window being updated.
 * @reg: The palette index being changed.
 * @value: The computed palette value.
 *
 * Change the value of a palette register, either by directly writing to
 * the palette (this requires the palette RAM to be disconnected from the
 * hardware whilst this is in progress) or schedule the update for later.
 *
 * At the moment, since we have no VSYNC interrupt support, we simply set
 * the palette entry directly.
 */
static void sfax8_fb_update_palette(struct sfax8_fb *sfb,
				    struct sfax8_fb_win *win, unsigned int reg,
				    u32 value)
{
	void __iomem *palreg;
	u32 palcon;
	int index = win->index;

	palreg = sfb->regs + select_reg(OVCW1PAL, OVCW0PAL);

	dev_dbg(sfb->dev, "%s: win %d, reg %d (%p): %08x\n", __func__,
		win->index, reg, palreg, value);

	palcon = readl(sfb->regs + OVCPCR);
	writel(palcon | UPDATE_PAL, sfb->regs + OVCPCR);

	writel(value, palreg + (reg * 4));

	writel(palcon, sfb->regs + OVCPCR);
}

static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= ((1 << bf->length) - 1);
	return chan << bf->offset;
}

/**
 * sfax8_fb_setcolreg() - framebuffer layer request to change palette.
 * @regno: The palette index to change.
 * @red: The red field for the palette data.
 * @green: The green field for the palette data.
 * @blue: The blue field for the palette data.
 * @trans: The transparency (alpha) field for the palette data.
 * @info: The framebuffer being changed.
 */
static int sfax8_fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			      unsigned blue, unsigned transp,
			      struct fb_info *info)
{
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;
	unsigned int val;

	dev_dbg(sfb->dev, "%s: win %d: %d => rgb=%d/%d/%d\n", __func__,
		win->index, regno, red, green, blue);

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseudo-palette */

		if (regno < SFAX8_FB_PALETTE_SIZE) {
			u32 *pal = info->pseudo_palette;

			val = chan_to_field(red, &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue, &info->var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		if (regno < SFAX8_FB_PALETTE_SIZE) {
			val = chan_to_field(red, &win->palette.r);
			val |= chan_to_field(green, &win->palette.g);
			val |= chan_to_field(blue, &win->palette.b);

			sfax8_fb_update_palette(sfb, win, regno, val);
		}

		break;

	default:
		return 1; /* unknown type */
	}

	return 0;
}

/**
 * sfax8_fb_blank() - blank or unblank the given window
 * @blank_mode: The blank state from FB_BLANK_*
 * @info: The framebuffer to blank.
 *
 * Framebuffer layer request to change the power state.
 */
static int sfax8_fb_blank(int blank_mode, struct fb_info *info)
{
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;
	unsigned int index = win->index;
	void __iomem *reg;
	u32 wincon;

	dev_dbg(sfb->dev, "blank mode %d\n", blank_mode);

	sfax8_lcd_enable(sfb, 0);

	reg = sfb->regs + select_reg(OVCW0CR, OVCW1CR);
	wincon = readl(reg);

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
		wincon &= ~OVCWxCR_ENWIN;
		sfb->enabled &= ~(1 << index);
	/* fall through to FB_BLANK_NORMAL */

	case FB_BLANK_NORMAL:
		/* disable the DMA and display 0x0 (black) */
		writel(MAPCOLEN | MAP_COLOR(0x0),
		       sfb->regs + select_reg(OVCW0CMR, OVCW1CMR));
		break;

	case FB_BLANK_UNBLANK:
		writel(0x0, sfb->regs + select_reg(OVCW0CMR, OVCW1CMR));
		wincon |= OVCWxCR_ENWIN;
		sfb->enabled |= (1 << index);
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		return 1;
	}

	writel(wincon, reg);

	sfax8_lcd_enable(sfb, sfb->enabled ? 1 : 0);

	return 0;
}

/**
 * sfax8_fb_pan_display() - Pan the display.
 *
 * Note that the offsets can be written to the device at any time, as their
 * values are latched at each vsync automatically. This also means that only
 * the last call to this function will have any effect on next vsync, but
 * there is no need to sleep waiting for it to prevent tearing.
 *
 * @var: The screen information to verify.
 * @info: The framebuffer device.
 */
static int sfax8_fb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;
	int index = win->index;
	u32 left_top_x, left_top_y;
	u32 right_bot_x, right_bot_y;
	u32 reg_data;

	/* change settings in PCAR and PCBR */
	left_top_x = var->xoffset & 0xfff;
	left_top_y = var->yoffset & 0xfff;
	reg_data = (left_top_x << LEFT_TOP_X_SHIFT) |
		   (left_top_y << LEFT_TOP_Y_SHIFT);
	writel(reg_data, sfb->regs + select_reg(OVCW0PCAR, OVCW1PCAR));

	right_bot_x = var->xoffset & 0xfff;
	right_bot_y = var->yoffset & 0xfff;
	reg_data = (right_bot_x << RIGHT_BOT_X_SHIFT) |
		   (right_bot_y << RIGHT_BOT_Y_SHIFT);
	writel(reg_data, sfb->regs + select_reg(OVCW0PCBR, OVCW1PCBR));

	return 0;
}

/**
 * sfax8_fb_enable_irq() - enable framebuffer interrupts
 * @sfb: main hardware state
 */
static void sfax8_fb_enable_irq(struct sfax8_fb *sfb)
{
	void __iomem *regs = sfb->regs;
	u32 irq_ctrl_reg;

	if (!test_and_set_bit(SFAX8_FB_VSYNC_IRQ_EN, &sfb->irq_flags)) {
		/* IRQ disabled, enable it */
		irq_ctrl_reg = readl(regs + GDUINTMASK);

		irq_ctrl_reg &= ~(OSDERRMASK | OSDW0INTMASK | OSDW1INTMASK |
				  VCLKINTMASK | LCDINTMASK);

		writel(irq_ctrl_reg, regs + GDUINTMASK);
	}
}

/**
 * sfax8_fb_disable_irq() - disable framebuffer interrupts
 * @sfb: main hardware state
 */
static void sfax8_fb_disable_irq(struct sfax8_fb *sfb)
{
	void __iomem *regs = sfb->regs;
	u32 irq_ctrl_reg;

	if (test_and_clear_bit(SFAX8_FB_VSYNC_IRQ_EN, &sfb->irq_flags)) {
		/* IRQ enabled, disable it */
		irq_ctrl_reg = readl(regs + GDUINTMASK);

		irq_ctrl_reg |= OSDERRMASK | OSDW0INTMASK | OSDW1INTMASK |
				VCLKINTMASK | LCDINTMASK;

		writel(irq_ctrl_reg, regs + GDUINTMASK);
	}
}

static irqreturn_t sfax8_fb_irq(int irq, void *dev_id)
{
	struct sfax8_fb *sfb = dev_id;
	void __iomem *regs = sfb->regs;
	u32 irq_sts_reg;

	spin_lock(&sfb->lock);

	irq_sts_reg = readl(regs + GDUINTPND);

	if (irq_sts_reg & LCDINT) {

		/* VSYNC interrupt, accept it */
		writel(LCDINT, regs + GDUSRCPND);
		writel(LCDINT, regs + GDUINTPND);

		sfb->vsync_info.count++;
		wake_up_interruptible(&sfb->vsync_info.wait);
	}

	/* We only support waiting for VSYNC for now, so it's safe
	 * to always disable irqs here.
	 */
	sfax8_fb_disable_irq(sfb);

	spin_unlock(&sfb->lock);
	return IRQ_HANDLED;
}

/**
 * sfax8_fb_wait_for_vsync() - sleep until next VSYNC interrupt or timeout
 * @sfb: main hardware state
 * @crtc: head index.
 */
static int sfax8_fb_wait_for_vsync(struct sfax8_fb *sfb, u32 crtc)
{
	unsigned long count;
	int ret;

	if (crtc != 0)
		return -ENODEV;

	count = sfb->vsync_info.count;
	sfax8_fb_enable_irq(sfb);
	ret = wait_event_interruptible_timeout(
	    sfb->vsync_info.wait, count != sfb->vsync_info.count,
	    msecs_to_jiffies(VSYNC_TIMEOUT_MSEC));

	if (ret == 0)
		return -ETIMEDOUT;

	return 0;
}

static int sfax8_fb_ioctl(struct fb_info *info, unsigned int cmd,
			  unsigned long arg)
{
	struct sfax8_fb_win *win = info->par;
	struct sfax8_fb *sfb = win->parent;
	void __user *argp = (void __user *)arg;
	struct sfax8_fb_alpha alpha;
	int ret = 0;
	u32 crtc;

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = sfax8_fb_wait_for_vsync(sfb, crtc);
		break;
	case SFFB_PUT_ALPHA:
		if (copy_from_user(&alpha, argp, sizeof(alpha))) {
			ret = -EFAULT;
			break;
		}
		ret = sfax8_fb_set_alpha(win, &alpha);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static struct fb_ops sfax8_fb_ops = {
    .owner = THIS_MODULE,
    .fb_check_var = sfax8_fb_check_var,
    .fb_set_par = sfax8_fb_set_par,
    .fb_blank = sfax8_fb_blank,
    .fb_setcolreg = sfax8_fb_setcolreg,
    .fb_fillrect = sys_fillrect,
    .fb_copyarea = sys_copyarea,
    .fb_imageblit = sys_imageblit,
    .fb_pan_display = sfax8_fb_pan_display,
    .fb_ioctl = sfax8_fb_ioctl,
};

/**
 * sfax8_fb_missing_pixclock() - calculates pixel clock
 * @mode: The video mode to change.
 *
 * Calculate the pixel clock when none has been given through platform data.
 */
static void sfax8_fb_missing_pixclock(struct fb_videomode *mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;

	div = mode->left_margin + mode->hsync_len + mode->right_margin +
	      mode->xres;
	div *= mode->upper_margin + mode->vsync_len + mode->lower_margin +
	       mode->yres;
	div *= mode->refresh ?: 60;

	do_div(pixclk, div);

	mode->pixclock = pixclk;
}

/**
 * sfax8_fb_alloc_memory() - allocate display memory for framebuffer window
 * @sfb: The base resources for the hardware.
 * @win: The window to initialise memory for.
 *
 * Allocate memory for the given framebuffer.
 */
static int sfax8_fb_alloc_memory(struct sfax8_fb *sfb, struct sfax8_fb_win *win)
{
	struct fb_videomode *vmode = &sfb->vtiming;
	struct fb_info *fbi = win->fbinfo;
	int index = win->index;
	unsigned int size;
	dma_addr_t map_dma;

	dev_dbg(sfb->dev, "allocating memory for display\n");
	dev_dbg(sfb->dev, "size = %u * %u\n", vmode->xres, vmode->yres);

	size = vmode->xres * vmode->yres * sizeof(u32);
	dev_dbg(sfb->dev, "want %u bytes for window\n", size);

	fbi->screen_base =
	    dma_alloc_noncoherent(sfb->dev, size, &map_dma, GFP_KERNEL);
	if (!fbi->screen_base) {
		dev_err(sfb->dev, "alloc graphic memory failed!\n");
		return -ENOMEM;
	}
	dev_dbg(sfb->dev, "screen_base = %p\n", fbi->screen_base);
	writel((u32)fbi->screen_base,
	       sfb->regs + select_reg(OVCW0B0SAR, OVCW1B0SAR));

	fbi->fix.smem_len = size;
	fbi->fix.smem_start = map_dma;

	return 0;
}

/**
 * sfax8_fb_release_win() - release resources for a framebuffer window.
 * @win: The window to cleanup the resources for.
 *
 * Release the resources that where claimed for the hardware window,
 * such as the framebuffer instance and any memory claimed for it.
 */
static void sfax8_fb_release_win(struct sfax8_fb *sfb, struct sfax8_fb_win *win)
{
	struct fb_info *info = win->fbinfo;

	if (info) {
		unregister_framebuffer(info);
		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);
		if (info->screen_base)
			dma_free_noncoherent(sfb->dev, info->fix.smem_len,
					     info->screen_base,
					     info->fix.smem_start);
		framebuffer_release(info);
	}
}

/**
 * sfax8_fb_probe_win() - register an hardware window
 * @sfb: The base resources for the hardware
 * @variant: The variant information for this window.
 * @res: Pointer to where to place the resultant window.
 *
 * Allocate and do the basic initialisation for one of the hardware's graphics
 * windows.
 */
static int sfax8_fb_probe_win(struct sfax8_fb *sfb, unsigned int index,
			      struct sfax8_fb_win **res)
{
	struct fb_var_screeninfo *var;
	struct sfax8_fb_win *win;
	struct fb_info *fbinfo;
	int ret;

	dev_dbg(sfb->dev, "probing window %d\n", index);

	init_waitqueue_head(&sfb->vsync_info.wait);

	fbinfo = framebuffer_alloc(sizeof(struct sfax8_fb_win), sfb->dev);
	if (!fbinfo) {
		dev_err(sfb->dev, "failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	*res = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	win->parent = sfb;
	win->index = index;
	/*win->valid_bpp = VALID_BPP1248 | VALID_BPP(16) | VALID_BPP(18) |
			 VALID_BPP(19) | VALID_BPP(24) | VALID_BPP(25) |
			 VALID_BPP(28);*/
	win->valid_bpp = VALID_BPP(16) | VALID_BPP(32);

	ret = sfax8_fb_alloc_memory(sfb, win);
	if (ret) {
		dev_err(sfb->dev, "failed to allocate display memory\n");
		return ret;
	}

	/* setup the r/b/g positions for the window's palette */
	/* Set RGB 888 as default */
	win->palette.r.offset = 16;
	win->palette.r.length = 8;
	win->palette.g.offset = 8;
	win->palette.g.length = 8;
	win->palette.b.offset = 0;
	win->palette.b.length = 8;

	/* setup the initial video mode from the window */
	fb_videomode_to_var(&fbinfo->var, &sfb->vtiming);

	fbinfo->fix.type = FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel = FB_ACCEL_NONE;
	fbinfo->fix.xpanstep = 1;
	fbinfo->fix.ypanstep = 1;
	fbinfo->var.activate = FB_ACTIVATE_NOW;
	fbinfo->var.vmode = FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = sfax8_fb_bpp_mode_to_bpp(sfb->bpp_mode);
	fbinfo->fbops = &sfax8_fb_ops;
	fbinfo->flags = FBINFO_HWACCEL_XPAN | FBINFO_HWACCEL_YPAN;
	fbinfo->pseudo_palette = &win->pseudo_palette;

	snprintf(fbinfo->fix.id, 16, "Siflower FB%d", index);

	/* prepare to actually start the framebuffer */
	ret = sfax8_fb_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(sfb->dev, "check_var failed on initial video params\n");
		return ret;
	}

	/* create initial colour map */
	ret = fb_alloc_cmap(&fbinfo->cmap, SFAX8_FB_PALETTE_SIZE, 1);
	if (ret == 0)
		fb_set_cmap(&fbinfo->cmap, fbinfo);
	else
		dev_err(sfb->dev, "failed to allocate fb cmap\n");

	sfax8_fb_set_par(fbinfo);

	dev_dbg(sfb->dev, "about to register framebuffer\n");

	/* run the check_var and set_par on our configuration. */

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		dev_err(sfb->dev, "failed to register framebuffer\n");
		return ret;
	}

#if !defined(CONFIG_FRAMEBUFFER_CONSOLE) && defined(CONFIG_LOGO)
	if (index == 0)
		if (fb_prepare_logo(fbinfo, FB_ROTATE_UR)) {
			/* Start display and show logo on boot */
			fb_show_logo(fbinfo, FB_ROTATE_UR);
		}
#endif

	dev_info(sfb->dev, "window %d: fb %s\n", index, fbinfo->fix.id);

	return 0;
}

/**
 * sfax8_fb_set_rgb_timing() - set video timing for rgb interface.
 * @sfb: The base resources for the hardware.
 *
 * Set horizontal and vertical lcd rgb interface timing.
 */
static void sfax8_fb_set_rgb_timing(struct sfax8_fb *sfb)
{
	struct fb_videomode *vmode = &sfb->vtiming;
	void __iomem *regs = sfb->regs;
	int clkdiv;
	u32 data;

	/* disable auto frequency conversion */
	data = readl(regs + LCDVCLKFSR);
	data |= 0xf << LCDVCLKFSR_CDOWN;
	writel(data, regs + LCDVCLKFSR);

	if (!vmode->pixclock)
		sfax8_fb_missing_pixclock(vmode);

	clkdiv = sfax8_fb_calc_pixclk(sfb, vmode->pixclock);
	writel((clkdiv - 1) << LCDCON1_CLKVAL, regs + LCDCON1);

	data = ((vmode->lower_margin - 1) << LCDCON2_VFPD) |
	       ((vmode->upper_margin - 1) << LCDCON2_VBPD);
	writel(data, regs + LCDCON2);

	data = ((vmode->hsync_len - 1) << LCDCON3_HSPW) |
	       ((vmode->vsync_len - 1) << LCDCON3_VSPW);
	writel(data, regs + LCDCON3);

	data = ((vmode->left_margin - 2) << LCDCON4_HBPD) |
	       (vmode->right_margin << LCDCON4_HFPD);
	writel(data, regs + LCDCON4);

	data = readl(regs + LCDCON5);
	if (vmode->sync & FB_SYNC_HOR_HIGH_ACT)
		data &= ~LCDCON5_INVHSYNC;
	else
		data |= LCDCON5_INVHSYNC;
	if (vmode->sync & FB_SYNC_VERT_HIGH_ACT)
		data &= ~LCDCON5_INVVSYNC;
	else
		data |= LCDCON5_INVVSYNC;
	writel(data, regs + LCDCON5);

	data = ((vmode->xres - 1) << LCDCON6_HOZVAL) |
	       ((vmode->yres - 1) << LCDCON6_LINEVAL);
	writel(data, regs + LCDCON6);
}

static int sfax8_fb_set_gpio(struct sfax8_fb *sfb)
{
	int ret;
	int i;

	for (i = 0; i < 2; i++) {
		ret = gpio_request(sfb->power_gpio[i], NULL);
		if (ret) {
			dev_err(sfb->dev, "Failed to request gpio %d\n",
				sfb->power_gpio[i]);
			return ret;
		}

		ret = gpio_direction_output(sfb->power_gpio[i], 1);
		if (ret) {
			dev_err(sfb->dev, "Failed to set gpio %d to output\n",
				sfb->power_gpio[i]);
			gpio_free(sfb->power_gpio[i]);
			return ret;
		}
	}

	return 0;
}

static void sfax8_fb_free_gpio(struct sfax8_fb *sfb)
{
	int i;

	for (i = 0; i < 2; i++)
		gpio_free(sfb->power_gpio[i]);
}

static const char *const fb_bpp_modes[] = {
	[rgb565]		= "RGB565",
	[rgb888]		= "RGB888",
	[bpp_mode_unknown]	= "",
};

static enum fb_bpp_mode sfax8_fb_get_bpp_mode(struct sfax8_fb *sfb, const char *mode)
{
	int index, ret;
	const char *item;

	ret = -EINVAL;

	for (index = 0; index < ARRAY_SIZE(fb_bpp_modes); index++) {
		item = fb_bpp_modes[index];
		if (!item) {
			continue;
		}
		if (!strcmp(item, mode)) {
			ret = index;
			break;
		}
	}

	return (ret < 0) ? bpp_mode_unknown : ret;
}

static void sfax8_fb_clk_enable(struct sfax8_fb *sfb)
{
	u32 clk_gate;

	clk_gate = get_module_clk_gate(SF_GDU_SOFT_RESET, false);
	clk_gate |= 0xF;
	set_module_clk_gate(SF_GDU_SOFT_RESET, clk_gate, false);

	clk_prepare_enable(sfb->bus_clk);
	clk_prepare_enable(sfb->lcd_clk);
	clk_prepare_enable(sfb->bus1xn_clk);
}

static void sfax8_fb_clk_disable(struct sfax8_fb *sfb)
{
	u32 clk_gate;

	clk_gate = get_module_clk_gate(SF_GDU_SOFT_RESET, false);
	clk_gate &= ~0xF;
	set_module_clk_gate(SF_GDU_SOFT_RESET, clk_gate, false);

	clk_disable_unprepare(sfb->bus_clk);
	clk_disable_unprepare(sfb->lcd_clk);
	clk_disable_unprepare(sfb->bus1xn_clk);
}

static int sfax8_fb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sfax8_fb *sfb;
	struct device_node *nc;
	struct resource *res;
	int win, irq, rgb_order[3];
	int ret = 0;
	u32 reg_data;
	const char *mode;

	if(release_reset(SF_GDU_SOFT_RESET))
		return -EFAULT;

	nc = pdev->dev.of_node;
	if (!nc)
		return -ENODEV;

	sfb = devm_kzalloc(dev, sizeof(struct sfax8_fb), GFP_KERNEL);
	if (!sfb) {
		dev_err(dev, "no memory for framebuffers\n");
		return -ENOMEM;
	}

	dev_dbg(dev, "allocate new framebuffer %p\n", sfb);

	sfb->dev = dev;

	spin_lock_init(&sfb->lock);

	if (of_property_read_u32(nc, "num-windows", &sfb->num_windows)) {
		dev_err(dev, "%s has no valid 'num-windows' property\n",
			nc->full_name);
		return -EINVAL;
	}

	ret = of_get_fb_videomode(nc, &sfb->vtiming, 0);
	if (ret) {
		dev_err(dev, "Failed to get video mode\n");
		return -EINVAL;
	}

	sfb->bus_clk = of_clk_get(dev->of_node, 0);
	if (IS_ERR(sfb->bus_clk)) {
		dev_err(dev, "failed to get framebuffer bus clock\n");
		return PTR_ERR(sfb->bus_clk);
	}
	sfb->lcd_clk = of_clk_get(dev->of_node, 1);
	if (IS_ERR(sfb->lcd_clk)) {
		dev_err(dev, "failed to get framebuffer lcd clock\n");
		return PTR_ERR(sfb->lcd_clk);
	}
	sfb->bus1xn_clk = of_clk_get(dev->of_node, 2);
	if (IS_ERR(sfb->bus1xn_clk)) {
		dev_err(dev, "failed to get bus1xn clock\n");
		return PTR_ERR(sfb->bus1xn_clk);
	}

	sfax8_fb_clk_enable(sfb);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sfb->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(sfb->regs)) {
		ret = PTR_ERR(sfb->regs);
		goto err_gpio;
	}

	reg_data = readl(sfb->regs + LCDCON5);
	if (of_property_read_string(nc, "bpp-mode", &mode) || (!mode)) {
		dev_err(dev, "failed to get bpp-mode\n");
		ret = -EINVAL;
		goto err_gpio;
	}
	sfb->bpp_mode = sfax8_fb_get_bpp_mode(sfb, mode);
	if ((sfb->bpp_mode < 0) || (sfb->bpp_mode == bpp_mode_unknown)) {
		dev_err(dev, "unknown bpp mode\n");
		ret = -EINVAL;
		goto err_gpio;
	}
	if (sfb->bpp_mode == rgb565) {
		sfb->is_rgb565_interface = 1;
		reg_data |= LCDCON5_RGB565IF;
	}

	if (of_property_read_u32_array(nc, "rgb_order", rgb_order, 3)) {
		dev_err(dev, "failed to get rgb_order\n");
		ret = -EINVAL;
		goto err_gpio;
	}
	sfb->rgb_order = rgb_order[0] | (rgb_order[1] << 2) |
			(rgb_order[2] << 4);
	reg_data &= ~(0x3f << LCDCON5_RGBORDER);
	reg_data |= sfb->rgb_order << LCDCON5_RGBORDER;

	writel(reg_data, sfb->regs + LCDCON5);

	if (of_property_read_u32_array(nc, "power_gpio", sfb->power_gpio,
				       2)) {
		dev_err(dev, "failed to get power_gpio\n");
		ret = -EINVAL;
		goto err_gpio;
	}
	if (sfax8_fb_set_gpio(sfb)) {
		dev_err(dev, "set gpio fail\n");
		ret = -ENODEV;
		goto err_gpio;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq resource\n");
		ret = -ENOENT;
		goto err_irq;
	}
	sfb->irq_num = irq;
	ret = devm_request_irq(dev, sfb->irq_num, sfax8_fb_irq, 0, "sfax8_fb",
			       sfb);
	if (ret) {
		dev_err(dev, "irq request failed\n");
		goto err_irq;
	}

	dev_dbg(dev, "got resources (regs %p), probing windows\n", sfb->regs);

	platform_set_drvdata(pdev, sfb);

	/* initialise colour key controls */
	writel(0, sfb->regs + OVCW1CKCR);

	sfax8_fb_set_rgb_timing(sfb);

	/* we have the register setup, start allocating framebuffers */
	for (win = 0; win < sfb->num_windows; win++) {
		ret = sfax8_fb_probe_win(sfb, win, &sfb->windows[win]);
		if (ret < 0) {
			dev_err(dev, "failed to create window %d\n", win);
			for (; win >= 0; win--)
				sfax8_fb_release_win(sfb, sfb->windows[win]);
			goto err_win;
		}
	}

	/* enable LCD */
	sfax8_lcd_enable(sfb, 1);

	return 0;

err_win:
err_irq:
	sfax8_fb_free_gpio(sfb);
err_gpio:
	sfax8_fb_clk_disable(sfb);

	return ret;
}

/**
 * sfax8_fb_remove() - Cleanup on module finalisation
 * @pdev: The platform device we are bound to.
 *
 * Shutdown and then release all the resources that the driver allocated
 * on initialisation.
 */
static int sfax8_fb_remove(struct platform_device *pdev)
{
	struct sfax8_fb *sfb = platform_get_drvdata(pdev);
	int win, i;

	for (win = 0; win < SFAX8_FB_MAX_WIN; win++)
		if (sfb->windows[win])
			sfax8_fb_release_win(sfb, sfb->windows[win]);

	sfax8_fb_clk_disable(sfb);

	for (i = 0; i < 2; i++)
		gpio_free(sfb->power_gpio[i]);

	if(hold_reset(SF_GDU_SOFT_RESET))
		return -EFAULT;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sfax8_fb_suspend(struct device *dev)
{
	struct sfax8_fb *sfb = dev_get_drvdata(dev);
	struct sfax8_fb_win *win;
	int index;

	for (index = SFAX8_FB_MAX_WIN - 1; index >= 0; index--) {
		win = sfb->windows[index];
		if (!win)
			continue;

		/* use the blank function to push into power-down */
		sfax8_fb_blank(FB_BLANK_POWERDOWN, win->fbinfo);
	}

	clk_disable_unprepare(sfb->bus_clk);
	clk_disable_unprepare(sfb->lcd_clk);
	//clk_disable_unprepare(sfb->bus1xn_clk);

	return 0;
}

static int sfax8_fb_resume(struct device *dev)
{
	struct sfax8_fb *sfb = dev_get_drvdata(dev);
	struct sfax8_fb_win *win;
	int index;

	clk_prepare_enable(sfb->bus_clk);
	clk_prepare_enable(sfb->lcd_clk);
	//clk_prepare_enable(sfb->bus1xn_clk);

	sfax8_fb_set_rgb_timing(sfb);

	/* restore framebuffers */
	for (index = 0; index < SFAX8_FB_MAX_WIN; index++) {
		win = sfb->windows[index];
		if (!win)
			continue;

		dev_dbg(dev, "resuming window %d\n", index);
		sfax8_fb_set_par(win->fbinfo);
	}

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sfax8_fb_runtime_suspend(struct device *dev)
{
	struct sfax8_fb *sfb = dev_get_drvdata(dev);

	clk_disable_unprepare(sfb->bus_clk);
	clk_disable_unprepare(sfb->lcd_clk);
	//clk_disable_unprepare(sfb->bus1xn_clk);

	return 0;
}

static int sfax8_fb_runtime_resume(struct device *dev)
{
	struct sfax8_fb *sfb = dev_get_drvdata(dev);
	struct sfax8_fb_platdata *pd = sfb->pdata;

	clk_prepare_enable(sfb->bus_clk);
	clk_prepare_enable(sfb->lcd_clk);
	//clk_prepare_enable(sfb->bus1xn_clk);

	return 0;
}
#endif

static const struct of_device_id sfax8_fb_of_match[] = {
    {
	.compatible = "siflower,sfax8-fb",
    },
    {},
};
MODULE_DEVICE_TABLE(of, sfax8_fb_of_match);

static const struct dev_pm_ops sfax8_fb_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(sfax8_fb_suspend, sfax8_fb_resume)
	SET_RUNTIME_PM_OPS(sfax8_fb_runtime_suspend, sfax8_fb_runtime_resume,
			   NULL)};

static struct platform_driver sfax8_fb_driver = {
    .probe = sfax8_fb_probe,
    .remove = sfax8_fb_remove,
    .driver =
	{
	    .name = "sfax8-fb",
	    .of_match_table = sfax8_fb_of_match,
	    .owner = THIS_MODULE,
	    .pm = &sfax8_fb_pm_ops,
	},
};
module_platform_driver(sfax8_fb_driver);

MODULE_AUTHOR("Qi Zhang <qi.zhang@siflower.com.cn>");
MODULE_DESCRIPTION("Siflower Ax8 Serials SoC Framebuffer Driver");
MODULE_LICENSE("GPL");
