/*
 *
 * Copyright (C) 2016 Shanghai Siflower Communication Technology Co., Ltd.
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <sound/soc.h>

struct sf_audio_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
};

static int sf_audio_probe(struct platform_device *pdev)
{
	struct device_node *audio_np, *codec_np, *np = pdev->dev.of_node;
	struct sf_audio_data *data;
	int ret = 0;

	dev_dbg(&pdev->dev, "Enter %s\n", __func__);
	audio_np = of_parse_phandle(np, "audio-controller", 0);
	if (!audio_np) {
		dev_err(&pdev->dev, "failed to find audio-controller\n");
		ret = -EINVAL;
		goto end;
	}

	codec_np = of_parse_phandle(np, "audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "failed to find audio-codec\n");
		ret = -EINVAL;
		goto end;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto end;
	}

	data->dai.name = "HIFI";
	data->dai.stream_name = "HIFI";
	data->dai.codec_dai_name = "dit-hifi";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_of_node = audio_np;
	data->dai.platform_of_node = audio_np;
	data->dai.playback_only = true;
	data->dai.capture_only = true;

	if (of_property_read_bool(np, "audio-out"))
		data->dai.capture_only = false;

	if (of_property_read_bool(np, "audio-in"))
		data->dai.playback_only = false;

	if (!data->dai.playback_only && !data->dai.capture_only) {
		dev_err(&pdev->dev, "no enabled AUDIO DAI link\n");
		goto end;
	}

	data->card.dev = &pdev->dev;
	data->card.dai_link = &data->dai;
	data->card.num_links = 1;

	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto end;

	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed: %d\n", ret);
		goto end;
	}

	platform_set_drvdata(pdev, data);

end:
	if (audio_np)
		of_node_put(audio_np);

	return ret;
}

static const struct of_device_id sfax8_of_match[] = {
	{ .compatible = "siflower,siflower-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, sfax8_of_match);

static struct platform_driver sf_snd_card_driver = {
	.driver = {
		.name = "siflower-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = sfax8_of_match,
	},
	.probe = sf_audio_probe,
};

module_platform_driver(sf_snd_card_driver);

MODULE_AUTHOR("Qi Zhang<qi.zhang@siflower.com.cn>");
MODULE_DESCRIPTION("Siflower Ax8 series machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:siflower-audio");
