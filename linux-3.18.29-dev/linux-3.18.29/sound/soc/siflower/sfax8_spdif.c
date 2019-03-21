/* sound/soc/sfax8/spdif.c
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

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>

#include <sound/asoundef.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <sf16a18.h>

#include "sfax8_spdif.h"

/*
 * SPDIF control structure
 * Defines channel status, subcode
 */
struct spdif_mixer_control {
	/* spinlock to access control data */
	spinlock_t ctl_lock;

	/* IEC958 channel tx status bit */
	unsigned char ch_status[24];

	/* User bits */
	unsigned char subcode[147];

	/* Buffer offset for U */
	u32 upos;

	/* Ready buffer index of the two buffers */
	u32 ready_buf;
};

/**
 * struct sfax8_spdif_info - Siflower S/PDIF Controller information
 * @lock: Spin lock for S/PDIF.
 * @dev: The parent device passed to use from the probe.
 * @regs: The pointer to the device register block.
 * @clk_rate: Current clock rate for calcurate ratio.
 * @pclk: The peri-clock pointer for spdif master operation.
 * @dma_playback: DMA information for playback channel.
 */
struct sfax8_spdif_info {
	struct spdif_mixer_control sf_spdif_control;
	spinlock_t	lock;
	struct device	*dev;
	void __iomem	*regs;
	unsigned long	clk_rate;
	struct clk	*pclk;
	struct snd_dmaengine_dai_dma_data dma_playback;
};

static struct sfax8_spdif_info spdif_info;

static inline struct sfax8_spdif_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return snd_soc_dai_get_drvdata(cpu_dai);
}

static void sfax8_spdif_clk_gate(int on)
{
	u32 val;

	val = get_module_clk_gate(SF_SPDIF_SOFT_RESET, 0);
	if (on)
		val |= 0x3;
	else
		val &= ~0x3;
	set_module_clk_gate(SF_SPDIF_SOFT_RESET, val, 0);
}

static void sfax8_pdif_write_channel_status(struct sfax8_spdif_info *spdif)
{
	struct spdif_mixer_control *ctrl = &spdif->sf_spdif_control;
	void __iomem *regs = spdif->regs;
	u32 ch_status, i;

	for (i = 0; i < 6; i++) {
		ch_status = (ctrl->ch_status[i * 4 + 3] << 24) |
			    (ctrl->ch_status[i * 4 + 2] << 16) |
			    (ctrl->ch_status[i * 4 + 1] << 8) |
			    (ctrl->ch_status[i * 4]);
		writel(ch_status, regs + TX_CHST_A(i));
	}
}

/* Get valid good bit from interrupt status register */
static int sfax8_spdif_vbit_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct sfax8_spdif_info *spdif = to_info(cpu_dai);
	void __iomem *regs = spdif->regs;
	u32 val;

	val = readl(regs + INT_STAT);
	ucontrol->value.integer.value[0] = ((val & CHB_PARITY_ERROR) != 0);

	return 0;
}

static void sfax8_spdif_snd_txctrl(struct sfax8_spdif_info *spdif, int on)
{
	void __iomem *regs = spdif->regs;
	u32 clkcon;
	u32 enable_bits = CLK_EN;

	clkcon = readl(regs + CTRL);
	if (on)
		writel(clkcon | enable_bits, regs + CTRL);
	else
		writel(clkcon & ~enable_bits, regs + CTRL);
}

static int sfax8_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sfax8_spdif_info *spdif = to_info(rtd->cpu_dai);
	unsigned long flags;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spin_lock_irqsave(&spdif->lock, flags);
		sfax8_spdif_snd_txctrl(spdif, 1);
		spin_unlock_irqrestore(&spdif->lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&spdif->lock, flags);
		sfax8_spdif_snd_txctrl(spdif, 0);
		spin_unlock_irqrestore(&spdif->lock, flags);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sfax8_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sfax8_spdif_info *spdif = to_info(rtd->cpu_dai);
	void __iomem *regs = spdif->regs;
	u32 ctrl, tx_config, tx_channel_status;
	unsigned long flags;
	int ratio;
	u32 param_width, param_rate, param_channel;

	sfax8_spdif_clk_gate(1);

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		dev_err(spdif->dev, "Capture is not supported\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&spdif->lock, flags);

	ctrl = readl(regs + CTRL) & CTRL_MASK;
	tx_config = readl(regs + TX_CONFIG) & TX_CONFIG_MASK;
	tx_channel_status = readl(regs + TXCHST) & TXCHST_MASK;

	ctrl |= SPDIF_EN | TXFIFO_EN | TX_EN;

	tx_config |= (DEFAULT_FIFO_DEPTH << TX_DIPSTICK_SHIFT) |
		     TX_DMA_EN | (TX_VALIDITY << TX_VALIDITY_SHIFT);

	param_width = params_width(params);
	if (param_width < 16 || param_width > 24) {
		dev_err(spdif->dev, "Unsupported data size.\n");
		goto err;
	}
	tx_config |= (param_width - 16) << TX_MODE_SHIFT;

	param_rate = params_rate(params);
	ratio = clk_get_rate(spdif->pclk) / (param_rate * 128) - 1;
	if (ratio <= 0 || ratio > 255) {
		dev_err(spdif->dev, "Invalid clock ratio %ld/%d\n",
				spdif->clk_rate, params_rate(params));
		goto err;
	}
	tx_config |= ratio << TX_RATIO_SHIFT;

	switch (param_rate) {
	case 32000:
		tx_channel_status |= 2 << TX_FREQUENCY_SHIFT;
		break;
	case 44100:
		tx_channel_status |= 0 << TX_FREQUENCY_SHIFT;
		break;
	case 48000:
		tx_channel_status |= 1 << TX_FREQUENCY_SHIFT;
		break;
	default:
		tx_channel_status |= 3 << TX_FREQUENCY_SHIFT;
	}

	param_channel = params_channels(params);
	switch (param_channel) {
	case 1:
		tx_config |= TX_ONLY_CHANNEL_A;
		break;
	case 2:
		tx_config &= ~TX_ONLY_CHANNEL_A;
		break;
	default:
		dev_err(spdif->dev, "Unsupported channel number %d\n",
		    param_channel);
		goto err;
	}

	tx_channel_status &= ~TX_CATEGORY_MASK;
	tx_channel_status |= 0x1 << TX_CATEGORY_SHIFT;
	tx_channel_status |= TX_NO_COPYRIGHT;

	dev_dbg(spdif->dev, "CTRL=%x,TX_CONFIG=%x,TXCHST=%x\n",
	    ctrl, tx_config, tx_channel_status);
	writel(ctrl, regs + CTRL);
	writel(tx_config, regs + TX_CONFIG);
	writel(tx_channel_status, regs + TXCHST);

	spin_unlock_irqrestore(&spdif->lock, flags);

	return 0;
err:
	spin_unlock_irqrestore(&spdif->lock, flags);
	return -EINVAL;
}

static void sfax8_spdif_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sfax8_spdif_info *spdif = to_info(rtd->cpu_dai);
	void __iomem *regs = spdif->regs;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	writel(0, regs + CTRL);

	sfax8_spdif_clk_gate(0);
}

/*
 * Siflower SPDIF IEC958 controller(mixer) functions
 *
 *	Channel status get/put control
 *	User bit value get/put control
 *	Valid bit value get control
 *	DPLL lock status get control
 *	User bit sync mode selection control
 */

static int sfax8_spdif_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}

static int sfax8_spdif_pb_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *uvalue)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct sfax8_spdif_info *spdif = to_info(cpu_dai);
	struct spdif_mixer_control *ctrl = &spdif->sf_spdif_control;
	int i = 0;

	for (i = 0; i < 24; i++)
		uvalue->value.iec958.status[i] = ctrl->ch_status[i];

	return 0;
}

static int sfax8_spdif_pb_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *uvalue)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct sfax8_spdif_info *spdif = to_info(cpu_dai);
	struct spdif_mixer_control *ctrl = &spdif->sf_spdif_control;
	int i = 0;

	for (i = 0; i < 24; i++)
		ctrl->ch_status[i] = uvalue->value.iec958.status[i];

	sfax8_pdif_write_channel_status(&spdif_info);

	return 0;
}

/* Valid bit infomation */
static int sfax8_spdif_vbit_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

/* User bit sync mode info */
static int sfax8_spdif_usync_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

/* Siflower SPDIF IEC958 controller defines */
static struct snd_kcontrol_new sf_spdif_ctrls[] = {
    /* Channel status controller */
    {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
	.access = SNDRV_CTL_ELEM_ACCESS_READ |
		  SNDRV_CTL_ELEM_ACCESS_WRITE |
		  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info = sfax8_spdif_info,
	.get = sfax8_spdif_pb_get,
	.put = sfax8_spdif_pb_put,
    },
    /* Valid bit error controller */
    {
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "IEC958 V-Bit Errors",
	.access = SNDRV_CTL_ELEM_ACCESS_READ |
		  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info = sfax8_spdif_vbit_info,
	.get = sfax8_spdif_vbit_get,
    },
    /* User bit sync mode controller */
    {
	.iface = SNDRV_CTL_ELEM_IFACE_HWDEP,
	.name = SNDRV_CTL_NAME_IEC958("U-bit Sync Mode", NONE, SWITCH),
	.access = SNDRV_CTL_ELEM_ACCESS_READ |
		  SNDRV_CTL_ELEM_ACCESS_WRITE |
		  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info = sfax8_spdif_usync_info,
	.get = NULL,
	.put = NULL,
    },
};

static int sfax8_spdif_dai_probe(struct snd_soc_dai *dai)
{
    struct sfax8_spdif_info *spdif = to_info(dai);

    snd_soc_dai_init_dma_data(dai, &spdif->dma_playback, NULL);

    snd_soc_add_dai_controls(dai, sf_spdif_ctrls, ARRAY_SIZE(sf_spdif_ctrls));

    return 0;
}

#ifdef CONFIG_PM
static int sfax8_spdif_suspend(struct snd_soc_dai *cpu_dai)
{
	struct sfax8_spdif_info *spdif = to_info(cpu_dai);
/* 	u32 con = spdif->saved_con;
 */

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

/* 	spdif->saved_clkcon = readl(spdif->regs	+ CLKCON) & CLKCTL_MASK;
 * 	spdif->saved_con = readl(spdif->regs + CON) & CON_MASK;
 * 	spdif->saved_cstas = readl(spdif->regs + CSTAS) & CSTAS_MASK;
 *
 * 	writel(con | CON_SW_RESET, spdif->regs + CON);
 * 	cpu_relax();
 */

	return 0;
}

static int sfax8_spdif_resume(struct snd_soc_dai *cpu_dai)
{
	struct sfax8_spdif_info *spdif = to_info(cpu_dai);

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

/* 	writel(spdif->saved_clkcon, spdif->regs	+ CLKCON);
 * 	writel(spdif->saved_con, spdif->regs + CON);
 * 	writel(spdif->saved_cstas, spdif->regs + CSTAS);
 */

	return 0;
}
#else
#define sfax8_spdif_suspend NULL
#define sfax8_spdif_resume NULL
#endif

static const struct snd_soc_dai_ops sfax8_spdif_dai_ops = {
	.trigger	= sfax8_spdif_trigger,
	.hw_params	= sfax8_spdif_hw_params,
	.shutdown	= sfax8_spdif_shutdown,
};

static struct snd_soc_dai_driver sfax8_spdif_dai = {
	.name = "sfax8-spdif",
	.playback = {
		.stream_name = "S/PDIF Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 ),
		.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S20_3LE |
				SNDRV_PCM_FMTBIT_S24_LE ),
	},
	.probe = sfax8_spdif_dai_probe,
	.ops = &sfax8_spdif_dai_ops,
	.suspend = sfax8_spdif_suspend,
	.resume = sfax8_spdif_resume,
};

static const struct snd_soc_component_driver sfax8_spdif_component = {
	.name		= "sfax8-spdif",
};

static int sfax8_spdif_probe(struct platform_device *pdev)
{
	struct resource *mem_res;
	struct sfax8_spdif_info *spdif;
	int ret;

	dev_dbg(&pdev->dev, "Entered %s\n", __func__);

	if(release_reset(SF_SPDIF_SOFT_RESET))
		return -EFAULT;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Unable to get register resource.\n");
		return -ENXIO;
	}

	/* TODO: Use gpio drivers */
	//sf_module_set_pad_func(SF_SPDIF);

	spdif = &spdif_info;
	spdif->dev = &pdev->dev;

	spin_lock_init(&spdif->lock);

	spdif->pclk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(spdif->pclk)) {
		dev_err(&pdev->dev, "failed to get spdif clock\n");
		ret = -ENOENT;
		goto err0;
	}
	clk_prepare_enable(spdif->pclk);
	spdif->clk_rate = clk_get_rate(spdif->pclk);

	/* Request S/PDIF Register's memory region */
	if (!request_mem_region(mem_res->start,
				resource_size(mem_res), "sfax8-spdif")) {
		dev_err(&pdev->dev, "Unable to request register region\n");
		ret = -EBUSY;
		goto err1;
	}

	spdif->regs = ioremap(mem_res->start, 0x100);
	if (spdif->regs == NULL) {
		dev_err(&pdev->dev, "Cannot ioremap registers\n");
		ret = -ENXIO;
		goto err2;
	}

	dev_set_drvdata(&pdev->dev, spdif);

	ret = devm_snd_soc_register_component(&pdev->dev,
			&sfax8_spdif_component, &sfax8_spdif_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "fail to register dai\n");
		goto err3;
	}

	spdif->dma_playback.addr = mem_res->start + TX_FIFO;
	spdif->dma_playback.maxburst = 8;

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "failed to register DMA: %d\n", ret);
		goto err3;
	}

	return 0;
err3:
	iounmap(spdif->regs);
err2:
	release_mem_region(mem_res->start, resource_size(mem_res));
err1:
	clk_disable_unprepare(spdif->pclk);
err0:
	return ret;
}

static int sfax8_spdif_remove(struct platform_device *pdev)
{
	struct sfax8_spdif_info *spdif = &spdif_info;
	struct resource *mem_res;

	iounmap(spdif->regs);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res)
		release_mem_region(mem_res->start, resource_size(mem_res));

	clk_disable_unprepare(spdif->pclk);

	if(hold_reset(SF_SPDIF_SOFT_RESET))
		return -EFAULT;

	return 0;
}

static const struct of_device_id sfax8_spdif_of_match[] = {
	{ .compatible = "siflower,sfax8-spdif", },
	{},
};
MODULE_DEVICE_TABLE(of, sfspi_of_match);

static struct platform_driver sfax8_spdif_driver = {
    .probe = sfax8_spdif_probe,
    .remove = sfax8_spdif_remove,
    .driver = {
		.name = "sfax8-spdif",
		.owner = THIS_MODULE,
		.of_match_table = sfax8_spdif_of_match,
    },
};

module_platform_driver(sfax8_spdif_driver);

MODULE_AUTHOR("Qi Zhang<qi.zhang@siflower.com.cn>");
MODULE_DESCRIPTION("Siflower S/PDIF CPU DAI Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sfax8-spdif-dai");
