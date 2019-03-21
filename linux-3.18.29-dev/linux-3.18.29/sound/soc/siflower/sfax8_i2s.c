/*
 * ALSA SoC Synopsys I2S Audio Layer
 *
 * sound/soc/sf/sfax8_i2s.c
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <sound/sfax8_i2s.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

//Module reset
#include <sf16a18.h>


/* common register for all channel */
#define IER		0x000
#define IRER		0x004
#define ITER		0x008
#define CER		0x00C
#define CCR		0x010
#define RXFFR		0x014
#define TXFFR		0x018

/* I2STxRxRegisters for all channels */
#define LRBR_LTHR(x)	(0x40 * x + 0x020)
#define RRBR_RTHR(x)	(0x40 * x + 0x024)
#define RER(x)		(0x40 * x + 0x028)
#define TER(x)		(0x40 * x + 0x02C)
#define RCR(x)		(0x40 * x + 0x030)
#define TCR(x)		(0x40 * x + 0x034)
#define ISR(x)		(0x40 * x + 0x038)
#define IMR(x)		(0x40 * x + 0x03C)
#define ROR(x)		(0x40 * x + 0x040)
#define TOR(x)		(0x40 * x + 0x044)
#define RFCR(x)		(0x40 * x + 0x048)
#define TFCR(x)		(0x40 * x + 0x04C)
#define RFF(x)		(0x40 * x + 0x050)
#define TFF(x)		(0x40 * x + 0x054)

/* I2SCOMPRegisters */
#define I2S_COMP_PARAM_2	0x01F0
#define I2S_COMP_PARAM_1	0x01F4
#define I2S_COMP_VERSION	0x01F8
#define I2S_COMP_TYPE		0x01FC

/*
* IMCR	IIS Mode register
* ICDR	IIS CDCLK Divisor register
* ISDR	IIS SCLK  Divisor register
*/
#define IMCR				0x3000
#define ICDR				0x3004
#define ISDR				0x3008

/*
 * Component parameter register fields - define the I2S block's
 * configuration.
 */
#define	COMP1_TX_WORDSIZE_3(r)	(((r) & GENMASK(27, 25)) >> 25)
#define	COMP1_TX_WORDSIZE_2(r)	(((r) & GENMASK(24, 22)) >> 22)
#define	COMP1_TX_WORDSIZE_1(r)	(((r) & GENMASK(21, 19)) >> 19)
#define	COMP1_TX_WORDSIZE_0(r)	(((r) & GENMASK(18, 16)) >> 16)
#define	COMP1_TX_CHANNELS(r)	(((r) & GENMASK(10, 9)) >> 9)
#define	COMP1_RX_CHANNELS(r)	(((r) & GENMASK(8, 7)) >> 7)
#define	COMP1_RX_ENABLED(r)	(((r) & BIT(6)) >> 6)
#define	COMP1_TX_ENABLED(r)	(((r) & BIT(5)) >> 5)
#define	COMP1_MODE_EN(r)	(((r) & BIT(4)) >> 4)
#define	COMP1_FIFO_DEPTH_GLOBAL(r)	(((r) & GENMASK(3, 2)) >> 2)
#define	COMP1_APB_DATA_WIDTH(r)	(((r) & GENMASK(1, 0)) >> 0)

#define	COMP2_RX_WORDSIZE_3(r)	(((r) & GENMASK(12, 10)) >> 10)
#define	COMP2_RX_WORDSIZE_2(r)	(((r) & GENMASK(9, 7)) >> 7)
#define	COMP2_RX_WORDSIZE_1(r)	(((r) & GENMASK(5, 3)) >> 3)
#define	COMP2_RX_WORDSIZE_0(r)	(((r) & GENMASK(2, 0)) >> 0)

/* Number of entries in WORDSIZE and DATA_WIDTH parameter registers */
#define	COMP_MAX_WORDSIZE	(1 << 3)
#define	COMP_MAX_DATA_WIDTH	(1 << 2 )

#define I2S_MAX_CHANNEL_NUM		2
#define I2S_MIN_CHANNEL_NUM		2

#define I2S_CLK_GATE_MASTER		1
#define I2S_CLK_GATE_SLAVE		(1 << 1)
#define I2S_CLK_GATE_PCLK		(1 << 3)
#define I2S_CLK_GATE_AUDIOCLK		(1 << 4)

typedef enum {
	CLK_OFF = 0,
	CLK_ON = 1,
}CLK_GATE_STA;

DEFINE_SPINLOCK(i2s_lock);
union sf_i2s_snd_dma_data {
	struct i2s_dma_data pd;
	struct snd_dmaengine_dai_dma_data dt;
};

struct sf_i2s_dev {
	void __iomem *i2s_base;
	struct clk *clk;
	int active;
	unsigned int capability;
	struct device *dev;
	/* data related to DMA transfers b/w i2s and DMAC */
	union sf_i2s_snd_dma_data play_dma_data;
	union sf_i2s_snd_dma_data capture_dma_data;
	struct i2s_clk_config_data config;
	int (*i2s_clk_cfg)(struct i2s_clk_config_data *config, unsigned long rate);
};
static int sf_i2s_clk_cfg(struct i2s_clk_config_data *config, unsigned long rate);

static inline void i2s_write_common_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, (void __iomem*)(((int)io_base & 0xFF000000) | reg));
}

static inline u32 i2s_read_common_reg(void __iomem *io_base, int reg)
{
	u32 ret;
	ret = readl((void __iomem*)(((int)io_base & 0xFF000000) | reg));
	return ret;
}

static inline void i2s_write_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, io_base + reg);
}

static inline u32 i2s_read_reg(void __iomem *io_base, int reg)
{
	u32 ret = readl(io_base + reg);
	return ret;
}

static inline void i2s_disable_channels(struct sf_i2s_dev *dev, u32 stream)
{
	u32 i = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < 4; i++)
			i2s_write_reg(dev->i2s_base, TER(i), 0);
	} else {
		for (i = 0; i < 4; i++)
			i2s_write_reg(dev->i2s_base, RER(i), 0);
	}
}

static inline void i2s_clear_irqs(struct sf_i2s_dev *dev, u32 stream)
{
	u32 i = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < 4; i++)
			i2s_read_reg(dev->i2s_base, TOR(i));
	} else {
		for (i = 0; i < 4; i++)
			i2s_read_reg(dev->i2s_base, ROR(i));
	}
}

static void i2s_clk_gate(CLK_GATE_STA onff, struct sf_i2s_dev *dev)
{
	u32 val;
	return;
	spin_lock(&i2s_lock);
	val = get_module_clk_gate(SF_IIS_SOFT_RESET, 0);
	if (onff){
		if( !((val & I2S_CLK_GATE_PCLK) && (val & I2S_CLK_GATE_AUDIOCLK))){
			val |= (I2S_CLK_GATE_PCLK | I2S_CLK_GATE_AUDIOCLK);
		}
		if(dev->capability & SF_I2S_MASTER)
			val |= I2S_CLK_GATE_MASTER;
		if(dev->capability & SF_I2S_SLAVE)
			val |= I2S_CLK_GATE_SLAVE;
	}else{
		if(dev->capability & SF_I2S_MASTER){
			if((val & I2S_CLK_GATE_SLAVE) == 0)
				val = 0;
			else
				val &= ~I2S_CLK_GATE_MASTER;
		}

		if(dev->capability & SF_I2S_SLAVE){
			if((val & I2S_CLK_GATE_MASTER) == 0)
				val = 0;
			else
				val &= ~I2S_CLK_GATE_SLAVE;
		}
	}
	set_module_clk_gate(SF_IIS_SOFT_RESET, val, 0);
	spin_unlock(&i2s_lock);
}

static void i2s_start(struct sf_i2s_dev *dev,
		      struct snd_pcm_substream *substream)
{
	u32 i, irq;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < 4; i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq & ~0x30);
		}
		i2s_write_reg(dev->i2s_base, ITER, 1);
	} else {
		for (i = 0; i < 4; i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq & ~0x03);
		}
		i2s_write_reg(dev->i2s_base, IRER, 1);
	}

	i2s_write_reg(dev->i2s_base, IER, 1);

	if(dev->capability & SF_I2S_MASTER){
		i2s_write_reg(dev->i2s_base, CER, 1);
	}
}

static void i2s_stop(struct sf_i2s_dev *dev,
		struct snd_pcm_substream *substream)
{
	u32 i = 0, irq;

	i2s_clear_irqs(dev, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		i2s_write_reg(dev->i2s_base, ITER, 0);

		for (i = 0; i < 4; i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq | 0x30);
		}
	} else {
		i2s_write_reg(dev->i2s_base, IRER, 0);

		for (i = 0; i < 4; i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq | 0x03);
		}
	}

	if (!dev->active) {
		if(dev->capability & SF_I2S_MASTER){
			i2s_write_reg(dev->i2s_base, CER, 0);
		}
		i2s_write_reg(dev->i2s_base, IER, 0);
	}
}

static int sf_i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	union sf_i2s_snd_dma_data *dma_data = NULL;


	if (!(dev->capability & SF_I2S_RECORD) &&
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE))
		return -EINVAL;

	if (!(dev->capability & SF_I2S_PLAY) &&
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK))
		return -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &dev->play_dma_data;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data = &dev->capture_dma_data;

	snd_soc_dai_set_dma_data(cpu_dai, substream, (void *)dma_data);

	return 0;
}

static int sf_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	struct i2s_clk_config_data *config = &dev->config;
	u32 ccr, xfer_resolution, ch_reg, irq;
	int ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		config->data_width = 16;
		ccr = 0x00;
		xfer_resolution = 0x02;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		config->data_width = 24;
		ccr = 0x08;
		xfer_resolution = 0x04;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		config->data_width = 32;
		ccr = 0x10;
		xfer_resolution = 0x05;
		break;

	default:
		dev_err(dev->dev, "sfax8-i2s: unsuppted PCM fmt");
		return -EINVAL;
	}

	config->chan_nr = params_channels(params);
	printk("Playback Stream channels:%d, dat_width is %d\n", config->chan_nr,
		config->data_width);
	switch (config->chan_nr) {
	case EIGHT_CHANNEL_SUPPORT:
	case SIX_CHANNEL_SUPPORT:
	case FOUR_CHANNEL_SUPPORT:
	case TWO_CHANNEL_SUPPORT:
		break;
	default:
		dev_err(dev->dev, "channel not supported\n");
		return -EINVAL;
	}

	i2s_disable_channels(dev, substream->stream);

	for (ch_reg = 0; ch_reg < (config->chan_nr / 2); ch_reg++) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_write_reg(dev->i2s_base, TCR(ch_reg),
				      xfer_resolution);
			i2s_write_reg(dev->i2s_base, TFCR(ch_reg), 0x08);
			irq = i2s_read_reg(dev->i2s_base, IMR(ch_reg));
			i2s_write_reg(dev->i2s_base, IMR(ch_reg), irq & ~0x30);
			i2s_write_reg(dev->i2s_base, TER(ch_reg), 1);
		} else {
			i2s_write_reg(dev->i2s_base, RCR(ch_reg),
				      xfer_resolution);
			i2s_write_reg(dev->i2s_base, RFCR(ch_reg), 0x07);
			irq = i2s_read_reg(dev->i2s_base, IMR(ch_reg));
			i2s_write_reg(dev->i2s_base, IMR(ch_reg), irq & ~0x03);
			i2s_write_reg(dev->i2s_base, RER(ch_reg), 1);
		}
	}


	config->sample_rate = params_rate(params);
	if (dev->capability & SF_I2S_MASTER) {
		i2s_write_reg(dev->i2s_base, CCR, ccr);
		if (dev->i2s_clk_cfg) {
			ret = dev->i2s_clk_cfg(config, clk_get_rate(dev->clk));
			if (ret < 0) {
				dev_err(dev->dev, "runtime audio clk config fail\n");
				return ret;
			} else {
				i2s_write_reg(dev->i2s_base, ISDR, ret);
			}
		} else {
			u32 bitclk = config->sample_rate *
					config->data_width * 2;

			ret = clk_set_rate(dev->clk, bitclk);
			if (ret) {
				dev_err(dev->dev, "Can't set I2S clock rate: %d\n",
					ret);
				return ret;
			}
		}
	}
	return 0;
}
static int sf_i2s_clk_cfg(struct i2s_clk_config_data *config, unsigned long rate)
{
	unsigned int div = 0;
	div = rate / (config->sample_rate * 2 * config->data_width);
	if(div > 1)
		return div - 1;
	else
		return 0;
}


static void sf_i2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	snd_soc_dai_set_dma_data(dai, substream, NULL);
}

static int sf_i2s_prepare(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	i2s_clear_irqs(dev, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(dev->i2s_base, TXFFR, 1);
	else
		i2s_write_reg(dev->i2s_base, RXFFR, 1);

	return 0;
}

static int sf_i2s_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev->active++;
		i2s_start(dev, substream);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dev->active--;
		i2s_stop(dev, substream);

		i2s_clk_gate(CLK_OFF, dev);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sf_i2s_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;

	i2s_clk_gate(CLK_ON, dev);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBS_CFS:
		ret = 0;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
	case SND_SOC_DAIFMT_CBS_CFM:
		ret = -EINVAL;
		break;
	default:
		dev_dbg(dev->dev, "sf : Invalid master/slave format\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sf_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
				  int clk_id, unsigned int freq, int dir)
{
	//TODO get sysclk and config CDCLK rate
	return 0;
}

static struct snd_soc_dai_ops sf_i2s_dai_ops = {
	.startup	= sf_i2s_startup,
	.shutdown	= sf_i2s_shutdown,
	.hw_params	= sf_i2s_hw_params,
	.prepare	= sf_i2s_prepare,
	.trigger	= sf_i2s_trigger,
	.set_fmt	= sf_i2s_set_fmt,
	.set_sysclk	= sf_i2s_set_sysclk,
};

static const struct snd_soc_component_driver sf_i2s_component = {
	.name		= "sf-i2s",
};

#ifdef CONFIG_PM

static int sf_i2s_suspend(struct snd_soc_dai *dai)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (dev->capability & SF_I2S_MASTER)
		clk_disable(dev->clk);
	return 0;
}

static int sf_i2s_resume(struct snd_soc_dai *dai)
{
	struct sf_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (dev->capability & SF_I2S_MASTER)
		clk_enable(dev->clk);
	return 0;
}

#else
#define sf_i2s_suspend	NULL
#define sf_i2s_resume	NULL
#endif

/*
 * The following tables allow a direct lookup of various parameters
 * defined in the I2S block's configuration in terms of sound system
 * parameters.  Each table is sized to the number of entries possible
 * according to the number of configuration bits describing an I2S
 * block parameter.
 */

/* Maximum bit resolution of a channel - not uniformly spaced */
static const u32 fifo_width[COMP_MAX_WORDSIZE] = {
	12, 16, 20, 24, 32, 0, 0, 0
};

/* Width of (DMA) bus */
static const u32 bus_widths[COMP_MAX_DATA_WIDTH] = {
	DMA_SLAVE_BUSWIDTH_1_BYTE,
	DMA_SLAVE_BUSWIDTH_2_BYTES,
	DMA_SLAVE_BUSWIDTH_3_BYTES,
	DMA_SLAVE_BUSWIDTH_UNDEFINED,
};
/* PCM format to support channel resolution */
static const u32 formats[COMP_MAX_WORDSIZE] = {
	SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S24_LE,
	SNDRV_PCM_FMTBIT_S24_LE,
	SNDRV_PCM_FMTBIT_S32_LE,
	0,
	0,
	0
};

static int sf_configure_dai(struct sf_i2s_dev *dev,
				   struct snd_soc_dai_driver *sf_i2s_dai,
				   unsigned int rates)
{
	dev_dbg(dev->dev, "sfax8: playback supported\n");
	sf_i2s_dai->playback.channels_min = I2S_MIN_CHANNEL_NUM;
	sf_i2s_dai->playback.channels_max = I2S_MAX_CHANNEL_NUM;
	sf_i2s_dai->playback.formats = formats[0];
	sf_i2s_dai->playback.rates = rates;

	dev_dbg(dev->dev, "sfax8: record supported\n");
	sf_i2s_dai->capture.channels_min = I2S_MIN_CHANNEL_NUM;
	sf_i2s_dai->capture.channels_max = I2S_MIN_CHANNEL_NUM;
	sf_i2s_dai->capture.formats = formats[0];
	sf_i2s_dai->capture.rates = rates;

	return 0;
}

static int sf_configure_dai_by_pd(struct sf_i2s_dev *dev,
				   struct snd_soc_dai_driver *sf_i2s_dai,
				   struct resource *res,
				   const struct i2s_platform_data *pdata)
{
	/*
	 * try to get bus width via I2S_COMP_PARAM_1 register,but
	 * this register don't exist in our soc.
	 */
	u32 comp1 = i2s_read_reg(dev->i2s_base, I2S_COMP_PARAM_1);

	u32 idx = COMP1_APB_DATA_WIDTH(comp1);
	int ret;

	if (WARN_ON(idx >= ARRAY_SIZE(bus_widths)))
		return -EINVAL;

	ret = sf_configure_dai(dev, sf_i2s_dai, pdata->snd_rates);
	if (ret < 0)
		return ret;

	/* Set DMA slaves info */
	dev->play_dma_data.pd.data = pdata->play_dma_data;
	dev->capture_dma_data.pd.data = pdata->capture_dma_data;
	dev->play_dma_data.pd.addr = res->start + I2S_TXDMA;
	dev->capture_dma_data.pd.addr = res->start + I2S_RXDMA;
	dev->play_dma_data.pd.max_burst = 16;
	dev->capture_dma_data.pd.max_burst = 16;
/*
	dev->play_dma_data.pd.addr_width = bus_widths[idx];
	dev->capture_dma_data.pd.addr_width = bus_widths[idx];
*/
	dev->play_dma_data.pd.filter = pdata->filter;
	dev->capture_dma_data.pd.filter = pdata->filter;

	return 0;
}

static int sf_configure_dai_by_dt(struct sf_i2s_dev *dev,
				   struct snd_soc_dai_driver *sf_i2s_dai,
				   struct resource *res)
{
	int ret, fifo_depth;
	ret = sf_configure_dai(dev, sf_i2s_dai, SNDRV_PCM_RATE_8000_192000);
	if (ret < 0) {
		return ret;
	}
	fifo_depth = 16;
	dev->capability |= SF_I2S_PLAY;
	dev->play_dma_data.dt.addr = res->start + I2S_TXDMA;
/*
  	dev->play_dma_data.dt.addr_width = bus_widths[4];
*/
	dev->play_dma_data.dt.chan_name = "tx";
	dev->play_dma_data.dt.fifo_size = fifo_depth *
		(fifo_width[4]) >> 8;
	dev->play_dma_data.dt.maxburst = 8;


	dev->capability |= SF_I2S_RECORD;
	dev->capture_dma_data.dt.addr = res->start + I2S_RXDMA;
/*
	dev->capture_dma_data.dt.addr_width = bus_widths[4];
*/
	dev->capture_dma_data.dt.chan_name = "rx";
	dev->capture_dma_data.dt.fifo_size = fifo_depth *
		(fifo_width[4] >> 8);
	dev->capture_dma_data.dt.maxburst = 8;
	return 0;

}
// static struct sf_i2s_dev sf_i2s[2];


static int sf_i2s_probe(struct platform_device *pdev)
{
	const struct i2s_platform_data *pdata = pdev->dev.platform_data;
	struct sf_i2s_dev *dev;
	struct resource *res;
	int ret;
	struct snd_soc_dai_driver *sf_i2s_dai;
	const char *interface;
	u32 mode;

	if(release_reset(SF_IIS_SOFT_RESET))
		return -EFAULT;
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	dev->capability = 0;
	if (!dev) {
		dev_warn(&pdev->dev, "kzalloc fail\n");
		return -ENOMEM;
	}

	sf_i2s_dai = devm_kzalloc(&pdev->dev, sizeof(*sf_i2s_dai), GFP_KERNEL);
	if (!sf_i2s_dai)
		return -ENOMEM;
	sf_i2s_dai->ops = &sf_i2s_dai_ops;
	sf_i2s_dai->suspend = sf_i2s_suspend;
	sf_i2s_dai->resume = sf_i2s_resume;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->i2s_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->i2s_base))
		return PTR_ERR(dev->i2s_base);

	if ((u32)dev->i2s_base & 0x1000){
		dev->capability |= SF_I2S_SLAVE;
	}else{
		dev->capability |= SF_I2S_MASTER;
	}

	dev->dev = &pdev->dev;

	if (pdata) {
		dev->capability = pdata->cap;
		ret = sf_configure_dai_by_pd(dev, sf_i2s_dai, res, pdata);
	} else {
		//get i2s interface from dts
		interface = of_get_property(pdev->dev.of_node, "interface", NULL);
		if(!strcmp(interface, "i2s0")){
			dev->capability |= SF_I2S_INTERFACE_I2S0;
			dev_dbg(dev->dev, "%s module connect to interface %s\n",
				(dev->capability & SF_I2S_MASTER) == SF_I2S_MASTER ? "master" : "slave",
				"i2s0");
		}else if(!strcmp(interface, "i2s1")){
			dev->capability |= SF_I2S_INTERFACE_I2S1;
			dev_dbg(dev->dev, "%s module connect to interface %s\n",
				(dev->capability & SF_I2S_MASTER) == SF_I2S_MASTER ? "master" : "slave",
				"i2s1");
		}else{
			dev_err(&pdev->dev, "Unsupport i2s %s interface!\n", interface);
			return -EINVAL;
		}

		dev->i2s_clk_cfg = &sf_i2s_clk_cfg;
		ret = sf_configure_dai_by_dt(dev, sf_i2s_dai, res);
	}
	if (ret < 0)
		return ret;

	if (dev->capability & SF_I2S_MASTER) {
		if (pdata) {
			dev->i2s_clk_cfg = pdata->i2s_clk_cfg;
			if (!dev->i2s_clk_cfg) {
				dev_err(&pdev->dev, "no clock configure method\n");
				return -ENODEV;
			}
		}
		/*Master mode*/
		spin_lock(&i2s_lock);
		if((dev->capability & SF_I2S_INTERFACE_I2S0) == SF_I2S_INTERFACE_I2S0)
			mode = I2S_MASTER_I2S0_MODE;

		if((dev->capability & SF_I2S_INTERFACE_I2S1) == SF_I2S_INTERFACE_I2S1)
			mode = I2S_SLAVE_I2S0_MODE;

		i2s_write_reg(dev->i2s_base, IMCR, mode);
		spin_unlock(&i2s_lock);
		/*set cdclk to 12MHz*/
		i2s_write_reg(dev->i2s_base, ICDR, I2S_CDCLK_DIV);

	}else{
		/*slave mode*/
		spin_lock(&i2s_lock);
		if((dev->capability & SF_I2S_INTERFACE_I2S0) == SF_I2S_INTERFACE_I2S0)
			mode = I2S_SLAVE_I2S0_MODE;

		if((dev->capability & SF_I2S_INTERFACE_I2S1) == SF_I2S_INTERFACE_I2S1)
			mode = I2S_MASTER_I2S0_MODE;

		i2s_write_reg(dev->i2s_base, ICDR, I2S_CDCLK_DIV);
		spin_unlock(&i2s_lock);
	}

	dev->clk = devm_clk_get(&pdev->dev, NULL);

	if (IS_ERR(dev->clk))
		return PTR_ERR(dev->clk);

	ret = clk_prepare_enable(dev->clk);
	if (ret < 0)
		return ret;

	dev_set_drvdata(&pdev->dev, dev);
	ret = devm_snd_soc_register_component(&pdev->dev, &sf_i2s_component,
					 sf_i2s_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register dai\n");
		goto err_clk_disable;
	}

	if (!pdata) {
		ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
		if (ret) {
			dev_err(&pdev->dev,
				"Could not register PCM: %d\n", ret);
			goto err_clk_disable;
		}
	}
	return 0;

err_clk_disable:
	if (dev->capability & (SF_I2S_MASTER | SF_I2S_SLAVE))
		clk_disable_unprepare(dev->clk);
	return ret;
}

static int sf_i2s_remove(struct platform_device *pdev)
{
	struct sf_i2s_dev *dev = dev_get_drvdata(&pdev->dev);

	if (dev->capability & (SF_I2S_MASTER | SF_I2S_SLAVE))
		clk_disable_unprepare(dev->clk);

	if(hold_reset(SF_IIS_SOFT_RESET))
		return -EFAULT;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sf_i2s_of_match[] = {
	{ .compatible = "siflower,sfax8-i2s",	 },
	{},
};

MODULE_DEVICE_TABLE(of, sf_i2s_of_match);
#endif

static struct platform_driver sf_i2s_driver = {
	.probe		= sf_i2s_probe,
	.remove		= sf_i2s_remove,
	.driver		= {
		.name	= "sfax8-i2s",
		.of_match_table = of_match_ptr(sf_i2s_of_match),
	},
};

module_platform_driver(sf_i2s_driver);

MODULE_DESCRIPTION("SFAX8 I2S SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sfax8_i2s");
