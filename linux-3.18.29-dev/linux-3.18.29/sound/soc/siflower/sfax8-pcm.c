/*
 *
 * ALSA SoC Audio Layer - SF PCM-Controller driver
 *
 * Copyright (c) 2016 Siflower Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/dmaengine.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <sf16a18.h>

/*Register Offsets */
#define SF_PCM_CTL		0x00
#define SF_PCM_MC		0x04
#define SF_PCM_CLKCTL		0x08
#define SF_PCM_CDCLKCFG		0x0C
#define SF_PCM_SCLKCFG		0x10
#define SF_PCM_TXFIFO		0x14
#define SF_PCM_RXFIFO		0x18
#define SF_PCM_IRQCTL		0x1C
#define SF_PCM_IRQSTAT		0x20
#define SF_PCM_FIFOSTAT		0x24
#define SF_PCM_CLRINT		0x28

/* Constant*/
#define SF_PCM_CLKSRC_PCLK     0
#define SF_PCM_CLKSRC_MUX      1
#define SF_PCM_SCLK_PER_FS     0
#define SF_PCM_TX_FIFO_DEPTH	0x10
#define SF_PCM_RX_FIFO_DEPTH	0x10
//#define SF_PCM_CDCLK_DIV		8
#define SF_PCM_WORDLENGTH_8		(0x11 << 16)
#define SF_PCM_WORDLENGTH_16	(0x0 << 16)
#define SF_PCM_WORDLENGTH_24	(0x8 << 16)
#define SF_PCM_WORDLENGTH_32	(0x10 << 16)
#define SF_PCM_WORDLENGTH_MASK	(0x1f << 16)
#define SF_PCM_MSYNC_RATIO		256

/* PCM_CTL Bit-Fields */
#define SF_PCM_CTL_MODE         (0x1 << 27)
#define SF_PCM_CTL_SINEDGE      (0x1 << 26)
#define SF_PCM_CTL_CHNUM_SHIFT  23
#define SF_PCM_CTL_CHNUM_MASK	0x7
#define SF_PCM_CTL_FMT_SHIFT    21
#define SF_PCM_CTL_WL_SHIFT     16
#define SF_PCM_CTL_TXDIPSTICK_MASK	0x3f
#define SF_PCM_CTL_TXDIPSTICK_SHIFT	10
#define SF_PCM_CTL_RXDIPSTICK_MASK	0x3f
#define SF_PCM_CTL_RXDIPSTICK_SHIFT	4
#define SF_PCM_CTL_TXDMA_EN		(0x1 << 3)
#define SF_PCM_CTL_RXDMA_EN		(0x1 << 2)
#define SF_PCM_CTL_TXMSB_AFTER_FSYNC	(0x1 << 1)
#define SF_PCM_CTL_RXMSB_AFTER_FSYNC	(0x1 << 0)

/*PCM MC Bit-Fields*/
#define SF_PCM_CTL_TXFIFO_EN		(0x1 << 5)
#define SF_PCM_CTL_RXFIFO_EN		(0x1 << 4)
#define SF_PCM_CTL_RX_EN				(0x1 << 3)
#define SF_PCM_CTL_TX_EN				(0x1 << 2)
#define SF_PCM_CTL_ENABLE		(0x1 << 0)

/* PCM_CLKCTL Bit-Fields */
#define SF_PCM_CLKCTL_SYNCDIV_MASK		0x1ff
#define SF_PCM_CLKCTL_SYNCDIV_SHIFT		8
#define SF_PCM_CLKCTL_SCLK_INV			(0x1 << 5)
#define SF_PCM_CLKCTL_SERCLKSEL_PCLK	(0x1 << 4)
#define SF_PCM_CLKCTL_SYNCCLK_EN		(0x1 << 3)
#define SF_PCM_CLKCTL_SCLK_EN			(0x1 << 2)
#define SF_PCM_CLKCTL_CDCLK_EN			(0x1 << 1)
#define SF_PCM_CLKCTL_SERCLK_EN			(0x1 << 0)
/* PCM_IRQCTL Bit-Fields
#define SF_PCM_IRQCTL_IRQEN		(0x1 << 14)
#define SF_PCM_IRQCTL_WRDEN		(0x1 << 12)
#define SF_PCM_IRQCTL_TXEMPTYEN	(0x1 << 11)
#define SF_PCM_IRQCTL_TXALMSTEMPTYEN	(0x1 << 10)
#define SF_PCM_IRQCTL_TXFULLEN		(0x1 << 9)
#define SF_PCM_IRQCTL_TXALMSTFULLEN	(0x1 << 8)
#define SF_PCM_IRQCTL_TXSTARVEN	(0x1 << 7)
#define SF_PCM_IRQCTL_TXERROVRFLEN	(0x1 << 6)
#define SF_PCM_IRQCTL_RXEMPTEN		(0x1 << 5)
#define SF_PCM_IRQCTL_RXALMSTEMPTEN	(0x1 << 4)
#define SF_PCM_IRQCTL_RXFULLEN		(0x1 << 3)
#define SF_PCM_IRQCTL_RXALMSTFULLEN	(0x1 << 2)
#define SF_PCM_IRQCTL_RXSTARVEN	(0x1 << 1)
#define SF_PCM_IRQCTL_RXERROVRFLEN	(0x1 << 0)
*/
/* PCM_IRQSTAT Bit-Fields
#define SF_PCM_IRQSTAT_IRQPND		(0x1 << 13)
#define SF_PCM_IRQSTAT_WRD_XFER	(0x1 << 12)
#define SF_PCM_IRQSTAT_TXEMPTY		(0x1 << 11)
#define SF_PCM_IRQSTAT_TXALMSTEMPTY	(0x1 << 10)
#define SF_PCM_IRQSTAT_TXFULL		(0x1 << 9)
#define SF_PCM_IRQSTAT_TXALMSTFULL	(0x1 << 8)
#define SF_PCM_IRQSTAT_TXSTARV		(0x1 << 7)
#define SF_PCM_IRQSTAT_TXERROVRFL	(0x1 << 6)
#define SF_PCM_IRQSTAT_RXEMPT		(0x1 << 5)
#define SF_PCM_IRQSTAT_RXALMSTEMPT	(0x1 << 4)
#define SF_PCM_IRQSTAT_RXFULL		(0x1 << 3)
#define SF_PCM_IRQSTAT_RXALMSTFULL	(0x1 << 2)
#define SF_PCM_IRQSTAT_RXSTARV		(0x1 << 1)
#define SF_PCM_IRQSTAT_RXERROVRFL	(0x1 << 0)
*/
/* PCM_FIFOSTAT Bit-Fields
#define SF_PCM_FIFOSTAT_TXCNT_MSK		(0x3f << 14)
#define SF_PCM_FIFOSTAT_TXFIFOEMPTY		(0x1 << 13)
#define SF_PCM_FIFOSTAT_TXFIFOALMSTEMPTY	(0x1 << 12)
#define SF_PCM_FIFOSTAT_TXFIFOFULL		(0x1 << 11)
#define SF_PCM_FIFOSTAT_TXFIFOALMSTFULL	(0x1 << 10)
#define SF_PCM_FIFOSTAT_RXCNT_MSK		(0x3f << 4)
#define SF_PCM_FIFOSTAT_RXFIFOEMPTY		(0x1 << 3)
#define SF_PCM_FIFOSTAT_RXFIFOALMSTEMPTY	(0x1 << 2)
#define SF_PCM_FIFOSTAT_RXFIFOFULL		(0x1 << 1)
#define SF_PCM_FIFOSTAT_RXFIFOALMSTFULL	(0x1 << 0)
*/
/**
 * struct sf_pcm_info - SF PCM Controller information
 * @dev: The parent device passed to use from the probe.
 * @regs: The pointer to the device register block.
 */
struct sf_pcm_info {
	spinlock_t lock;
	struct device	*dev;
	struct snd_dmaengine_dai_dma_data	dma_data[2];
	void __iomem	*regs;

	unsigned int sclk_per_fs;

	/* Whether to keep PCMSCLK enabled even when idle(no active xfer) */
	unsigned int idleclk;

	struct clk	*pclk;
	struct clk	*audio_clk;

};

static struct sf_pcm_info sf_pcm[2];
static int dev_id = 0;

static void sf_pcm_clk_gate(int on)
{
	u32 val;

	val = get_module_clk_gate(SF_PCM_SOFT_RESET, 0);
	if (on)
		val |= (0x15 << dev_id);
	else
		val &= (0x2a >> dev_id);
	set_module_clk_gate(SF_PCM_SOFT_RESET, val, 0);
}

static void sf_pcm_snd_txctrl(struct sf_pcm_info *pcm, int on)
{
	void __iomem *regs = pcm->regs;
	u32 ctl, mcctl, clkctl;

	clkctl = readl(regs + SF_PCM_CLKCTL);
	ctl = readl(regs + SF_PCM_CTL);
	mcctl = readl(regs + SF_PCM_MC);
	ctl &= ~(SF_PCM_CTL_TXDIPSTICK_MASK
			 << SF_PCM_CTL_TXDIPSTICK_SHIFT);

	if (on) {
		ctl |= SF_PCM_CTL_TXDMA_EN;
		mcctl |= SF_PCM_CTL_TXFIFO_EN;
		mcctl |= SF_PCM_CTL_ENABLE;
		mcctl |= SF_PCM_CTL_TX_EN;
		ctl |= (SF_PCM_TX_FIFO_DEPTH<<SF_PCM_CTL_TXDIPSTICK_SHIFT);
		clkctl |= SF_PCM_CLKCTL_SERCLK_EN;
		clkctl |= SF_PCM_CLKCTL_SCLK_EN;
		clkctl |= SF_PCM_CLKCTL_SYNCCLK_EN;
	} else {
		ctl &= ~SF_PCM_CTL_TXDMA_EN;
		mcctl &= ~SF_PCM_CTL_TXFIFO_EN;
		mcctl &= ~SF_PCM_CTL_TX_EN;
		mcctl &= ~SF_PCM_CTL_ENABLE;
		clkctl &= ~SF_PCM_CLKCTL_SERCLK_EN;
		clkctl &= ~SF_PCM_CLKCTL_SCLK_EN;
		clkctl &= ~SF_PCM_CLKCTL_SYNCCLK_EN;
	}

	writel(mcctl, regs + SF_PCM_MC);
	mb();
	writel(clkctl, regs + SF_PCM_CLKCTL);
	writel(ctl, regs + SF_PCM_CTL);
}

static void sf_pcm_snd_rxctrl(struct sf_pcm_info *pcm, int on)
{
	void __iomem *regs = pcm->regs;
	u32 mcctl, ctl, clkctl;

	ctl = readl(regs + SF_PCM_CTL);
	mcctl = readl(regs + SF_PCM_MC);
	clkctl = readl(regs + SF_PCM_CLKCTL);
	ctl &= ~(SF_PCM_CTL_RXDIPSTICK_MASK
			 << SF_PCM_CTL_RXDIPSTICK_SHIFT);

	if (on) {
		ctl |= SF_PCM_CTL_RXDMA_EN;
		mcctl |= SF_PCM_CTL_RXFIFO_EN;
		mcctl |= SF_PCM_CTL_ENABLE;
		mcctl |= SF_PCM_CTL_RX_EN;
		ctl |= (SF_PCM_RX_FIFO_DEPTH<<SF_PCM_CTL_RXDIPSTICK_SHIFT);
		ctl |= SF_PCM_CTL_SINEDGE;
		clkctl |= SF_PCM_CLKCTL_SERCLK_EN;
		clkctl |= SF_PCM_CLKCTL_SCLK_EN;
		clkctl |= SF_PCM_CLKCTL_SYNCCLK_EN;
	} else {
		ctl &= ~SF_PCM_CTL_RXDMA_EN;
		ctl &= ~SF_PCM_CTL_SINEDGE;
		mcctl &= ~SF_PCM_CTL_RXFIFO_EN;
		mcctl &= ~SF_PCM_CTL_RX_EN;
		mcctl &= ~SF_PCM_CTL_ENABLE;
		clkctl &= ~SF_PCM_CLKCTL_SERCLK_EN;
		clkctl &= ~SF_PCM_CLKCTL_SCLK_EN;
		clkctl &= ~SF_PCM_CLKCTL_SYNCCLK_EN;
	}

	writel(clkctl, regs + SF_PCM_CLKCTL);
	writel(mcctl, regs + SF_PCM_MC);
	writel(ctl, regs + SF_PCM_CTL);
}

static int sf_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sf_pcm_info *pcm = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	unsigned long flags;

	dev_dbg(pcm->dev, "Entered %s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

		spin_lock_irqsave(&pcm->lock, flags);

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			sf_pcm_snd_rxctrl(pcm, 1);
		else
			sf_pcm_snd_txctrl(pcm, 1);

		spin_unlock_irqrestore(&pcm->lock, flags);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&pcm->lock, flags);

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			sf_pcm_snd_rxctrl(pcm, 0);
		else
			sf_pcm_snd_txctrl(pcm, 0);

		sf_pcm_clk_gate(0);

		spin_unlock_irqrestore(&pcm->lock, flags);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int sf_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sf_pcm_info *pcm = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	void __iomem *regs = pcm->regs;
	struct clk *clk;
	int sclk_div, sync_div;
	unsigned long flags;
	u32 clkctl, pcmctl, word_length, param_word, param_channel, param_rate, clk_freq, cdclk_div, cd_rate;

	dev_dbg(pcm->dev, "Entered %s\n", __func__);
	param_word = params_width(params);
	param_channel = params_channels(params);
	param_rate = params_rate(params);
	dev_dbg(pcm->dev, "word is %d,channel is %d, rate is %d\n", param_word, param_channel, param_rate);

	/* Get hold of the PCMSOURCE_CLK */
	clkctl = readl(regs + SF_PCM_CLKCTL);
	if (clkctl & SF_PCM_CLKCTL_SERCLKSEL_PCLK)
		clk = pcm->pclk;
	else
		clk = pcm->audio_clk;

	clk_freq = clk_get_rate(clk);

	spin_lock_irqsave(&pcm->lock, flags);
	pcmctl = readl(regs + SF_PCM_CTL);

	/* Strictly check for sample size */
	switch (param_word) {
	case 8:
		word_length = SF_PCM_WORDLENGTH_8;
		break;
	case 16:
		word_length = SF_PCM_WORDLENGTH_16;
		break;
	case 24:
		word_length = SF_PCM_WORDLENGTH_24;
		break;
	case 32:
		word_length = SF_PCM_WORDLENGTH_32;
		break;
	default:
		return -EINVAL;
	}
	pcmctl &= ~SF_PCM_WORDLENGTH_MASK;
	pcmctl |= word_length;

	pcmctl &= ~(SF_PCM_CTL_CHNUM_MASK << SF_PCM_CTL_CHNUM_SHIFT);
	if (param_channel < 1 || param_channel > 8) {
		return -EINVAL;
	}else{
		pcmctl |= (param_channel - 1) << SF_PCM_CTL_CHNUM_SHIFT;
	}

	writel(pcmctl, regs + SF_PCM_CTL);

	/*set codec mclk source freq*/
	switch(param_rate){
		case 8000:
		case 12000:
		case 16000:
		case 24000:
		case 32000:
		case 48000:
		case 96000:
			cd_rate = 12288000;
			break;
		case 8018:
		case 11025:
		case 22050:
		case 44100:
		case 88200:
			cd_rate = 11289600;
			break;
		default:
			printk(KERN_ERR"PCM: unsupport sample rate!\n");
			return -EINVAL;
	}

	cdclk_div = clk_freq / cd_rate - 1;
	writel(cdclk_div, regs + SF_PCM_CDCLKCFG);

	clkctl |= SF_PCM_CLKCTL_CDCLK_EN;
	/* Set the SYNC divider */
	//sync_div = clk_freq / (sclk_div + 1) / param_rate - 1;
	sync_div = param_word * param_channel + 1;
	clkctl &= ~(SF_PCM_CLKCTL_SYNCDIV_MASK
				<< SF_PCM_CLKCTL_SYNCDIV_SHIFT);
	clkctl |= ((sync_div & SF_PCM_CLKCTL_SYNCDIV_MASK)
				<< SF_PCM_CLKCTL_SYNCDIV_SHIFT);

	writel(clkctl, regs + SF_PCM_CLKCTL);

	/* Set the SCLK divider */
	sclk_div = clk_freq / (param_rate * (sync_div + 1)) - 1;
	writel(sclk_div, regs + SF_PCM_SCLKCFG);

	spin_unlock_irqrestore(&pcm->lock, flags);

	dev_dbg(pcm->dev, "PCMSOURCE_CLK-%d SCLK_DIV=%d SYNC_DIV=%d PCMCTL=%d\n",
				clk_freq, sclk_div, sync_div, pcmctl);

	return 0;
}

static int sf_pcm_set_fmt(struct snd_soc_dai *cpu_dai,
			       unsigned int fmt)
{
	struct sf_pcm_info *pcm = snd_soc_dai_get_drvdata(cpu_dai);
	void __iomem *regs = pcm->regs;
	unsigned long flags;
	int ret = 0;
	u32 ctl;

	sf_pcm_clk_gate(1);

	dev_dbg(pcm->dev, "Entered %s\n", __func__);

	spin_lock_irqsave(&pcm->lock, flags);

	ctl = readl(regs + SF_PCM_CTL);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_NF:
	case SND_SOC_DAIFMT_IB_IF:
		ctl |= SF_PCM_CLKCTL_SCLK_INV;
		break;
	case SND_SOC_DAIFMT_NB_NF:
	case SND_SOC_DAIFMT_NB_IF:
		ctl &= ~SF_PCM_CLKCTL_SCLK_INV;
		break;
	default:
		dev_err(pcm->dev, "Unsupported clock inversion!\n");
		ret = -EINVAL;
		goto exit;
	}
	dev_dbg(pcm->dev, "clock inv is %4x\n", fmt & SND_SOC_DAIFMT_INV_MASK);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* Nothing to do, Master by default */
		break;
	default:
		dev_err(pcm->dev, "Unsupported master/slave format!\n");
		ret = -EINVAL;
		goto exit;
	}
	#if 0
	switch (fmt & SND_SOC_DAIFMT_CLOCK_MASK) {
	case SND_SOC_DAIFMT_CONT:
		pcm->idleclk = 1;
		break;
	case SND_SOC_DAIFMT_GATED:
		pcm->idleclk = 0;
		break;
	default:
		dev_err(pcm->dev, "Invalid Clock gating request!\n");
		ret = -EINVAL;
		goto exit;
	}
	#endif
	/*To save power, we always disable PCM SCLK when no data transmission.*/
	pcm->idleclk = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		ctl |= SF_PCM_CTL_TXMSB_AFTER_FSYNC;
		ctl |= SF_PCM_CTL_RXMSB_AFTER_FSYNC;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		ctl &= ~SF_PCM_CTL_TXMSB_AFTER_FSYNC;
		ctl &= ~SF_PCM_CTL_RXMSB_AFTER_FSYNC;
		break;
	default:
		dev_err(pcm->dev, "Unsupported data format!\n");
		ret = -EINVAL;
		goto exit;
	}

	writel(ctl, regs + SF_PCM_CTL);

exit:
	spin_unlock_irqrestore(&pcm->lock, flags);

	return ret;
}

/*static int sf_pcm_set_clkdiv(struct snd_soc_dai *cpu_dai,
						int div_id, int div)
{
	struct sf_pcm_info *pcm = snd_soc_dai_get_drvdata(cpu_dai);

	switch (div_id) {
	case SF_PCM_SCLK_PER_FS:
		pcm->sclk_per_fs = div;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}*/

static int sf_pcm_set_sysclk(struct snd_soc_dai *cpu_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct sf_pcm_info *pcm = snd_soc_dai_get_drvdata(cpu_dai);
	void __iomem *regs = pcm->regs;
	u32 clkctl = readl(regs + SF_PCM_CLKCTL);

	switch (clk_id) {
	case SF_PCM_CLKSRC_PCLK:
		clkctl |= SF_PCM_CLKCTL_SERCLKSEL_PCLK;
		break;

	case SF_PCM_CLKSRC_MUX:
		clkctl &= ~SF_PCM_CLKCTL_SERCLKSEL_PCLK;

	/*	if (clk_get_rate(pcm->audio_clk) != freq)
			clk_set_rate(pcm->audio_clk, freq);
*/
		break;

	default:
		return -EINVAL;
	}

	writel(clkctl, regs + SF_PCM_CLKCTL);
	dev_dbg(pcm->dev, "freq is  %d, clk_id is %d!\n", freq, clk_id);
	return 0;
}

static const struct snd_soc_dai_ops sf_pcm_dai_ops = {
	.set_sysclk	= sf_pcm_set_sysclk,
	.trigger	= sf_pcm_trigger,
	.hw_params	= sf_pcm_hw_params,
	.set_fmt	= sf_pcm_set_fmt,
};

static int sf_pcm_dai_probe(struct snd_soc_dai *dai)
{
	struct sf_pcm_info *pcm = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai,
							&pcm->dma_data[SNDRV_PCM_STREAM_PLAYBACK],
							&pcm->dma_data[SNDRV_PCM_STREAM_CAPTURE]);

	return 0;
}

#define SF_PCM_RATES  (SNDRV_PCM_RATE_8000_96000)

#define SF_PCM_DAI_DECLARE			\
	.symmetric_rates = 1,					\
	.probe = sf_pcm_dai_probe,				\
	.ops = &sf_pcm_dai_ops,				\
	.playback = {						\
		.channels_min	= 1,				\
		.channels_max	= 8,				\
		.rates		= SF_PCM_RATES,		\
		.formats	= SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8,	\
	},							\
	.capture = {						\
		.channels_min	= 1,				\
		.channels_max	= 8,				\
		.rates		= SF_PCM_RATES,		\
		.formats	= SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8,	\
	}

static struct snd_soc_dai_driver sf_pcm_dai[] = {
	[0] = {
		.name	= "sf-pcm.0",
		SF_PCM_DAI_DECLARE,
	},
	[1] = {
		.name	= "sf-pcm.1",
		SF_PCM_DAI_DECLARE,
	},
};

static const struct snd_soc_component_driver sf_pcm_component = {
	.name		= "sf-pcm",
};

static int sf_pcm_dev_probe(struct platform_device *pdev)
{
	struct sf_pcm_info *pcm;
	struct resource *mem_res;
	int ret, id;

	if(release_reset(SF_PCM_SOFT_RESET))
		return -EFAULT;

	dev_dbg(&pdev->dev, "PCM start!\n");

	if ( of_property_read_u32(pdev->dev.of_node, "id", &id) ){
		dev_err(&pdev->dev, "PCM get id error!\n");
		return -EINVAL;
	}else{
		pdev->id = id;
	}
	/* Check for valid device index */
	if (pdev->id >= ARRAY_SIZE(sf_pcm)) {
		dev_err(&pdev->dev, "id %d out of range\n", pdev->id);
		return -EINVAL;
	}

	dev_id = pdev->id;

	pcm = &sf_pcm[pdev->id];

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Unable to get register resource\n");
		return -ENXIO;
	}

	pcm->dev = &pdev->dev;

	spin_lock_init(&pcm->lock);

	/* Default is 128fs */
	pcm->sclk_per_fs = 128;

	pcm->audio_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pcm->audio_clk)) {
		dev_err(&pdev->dev, "failed to get pcm-audio_clk\n");
		ret = PTR_ERR(pcm->audio_clk);
		goto err1;
	}
	ret = clk_prepare_enable(pcm->audio_clk);
	if (ret){
		dev_err(&pdev->dev, "failed to start audio clk\n");
		goto err1;
	}
	/* record our pcm structure for later use in the callbacks */
	dev_set_drvdata(&pdev->dev, pcm);

	if (!request_mem_region(mem_res->start,
				resource_size(mem_res), "sfax8-pcm")) {
		dev_err(&pdev->dev, "Unable to request register region\n");
		ret = -EBUSY;
		goto err2;
	}

	pcm->regs = ioremap(mem_res->start, 0x100);
	if (pcm->regs == NULL) {
		dev_err(&pdev->dev, "cannot ioremap registers\n");
		ret = -ENXIO;
		goto err3;
	}

/*	pcm->pclk = devm_clk_get(&pdev->dev, "pcm-pclk");
	if (IS_ERR(pcm->pclk)) {
		dev_err(&pdev->dev, "failed to get pcm-pclk\n");
		ret = -ENOENT;
		goto err4;
	}
	clk_prepare_enable(pcm->pclk);
*/

	pcm->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr = (dma_addr_t)mem_res->start
							+ SF_PCM_RXFIFO;
	pcm->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr = (dma_addr_t)mem_res->start
							+ SF_PCM_TXFIFO;

	pcm->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr_width =
							DMA_SLAVE_BUSWIDTH_UNDEFINED;
	pcm->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr_width =
							DMA_SLAVE_BUSWIDTH_UNDEFINED;

	pcm->dma_data[SNDRV_PCM_STREAM_CAPTURE].maxburst = 16;
	pcm->dma_data[SNDRV_PCM_STREAM_PLAYBACK].maxburst = 16;

	ret = devm_snd_soc_register_component(&pdev->dev, &sf_pcm_component,
					 &sf_pcm_dai[pdev->id], 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to get register DAI: %d\n", ret);
		goto err4;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "failed to get register DMA: %d\n", ret);
		goto err4;
	}

	return 0;

/*err5:
	clk_disable_unprepare(pcm->pclk);*/
err4:
	iounmap(pcm->regs);
err3:
	release_mem_region(mem_res->start, resource_size(mem_res));
err2:
	clk_disable_unprepare(pcm->audio_clk);
err1:
	return ret;
}

static int sf_pcm_dev_remove(struct platform_device *pdev)
{
	struct sf_pcm_info *pcm = &sf_pcm[pdev->id];
	struct resource *mem_res;

	iounmap(pcm->regs);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem_res->start, resource_size(mem_res));

	clk_disable_unprepare(pcm->audio_clk);
/*	clk_disable_unprepare(pcm->pclk);*/

	if(hold_reset(SF_PCM_SOFT_RESET))
		return -EFAULT;
	return 0;
}

static const struct of_device_id sfax8_pcm_of_match[] = {
	{ .compatible = "siflower,sfax8-pcm",},
	{},
};

static struct platform_driver sf_pcm_driver = {
	.probe  = sf_pcm_dev_probe,
	.remove = sf_pcm_dev_remove,
	.driver = {
		.name = "sfax8-pcm",
		.owner = THIS_MODULE,
		.of_match_table = sfax8_pcm_of_match,
	},
};

module_platform_driver(sf_pcm_driver);

/* Module information */
MODULE_AUTHOR("Xijun Guo, <xijun.guo@siflower.com>");
MODULE_DESCRIPTION("SF PCM Controller Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:SF-pcm");
