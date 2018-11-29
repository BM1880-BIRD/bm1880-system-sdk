/*
 * Machine driver for EVAL-ADAU1372 on bitmain BM1880
 *
 * Copyright 2018 Bitmain
 *
 * Author: EthanChen
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "../codecs/adau1372.h"

static const struct snd_soc_dapm_widget bm1880_adau1372_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line In0", NULL),
	SND_SOC_DAPM_LINE("Line In1", NULL),
	SND_SOC_DAPM_LINE("Line In2", NULL),
	SND_SOC_DAPM_LINE("Line In3", NULL),
	SND_SOC_DAPM_HP("Earpiece", NULL),
};

static const struct snd_soc_dapm_route bm1880_adau1372_dapm_routes[] = {
	{ "AIN0", NULL, "Line In0" },
	{ "AIN1", NULL, "Line In1" },
	{ "AIN2", NULL, "Line In2" },
	{ "AIN3", NULL, "Line In3" },

	{ "Earpiece", NULL, "HPOUTL" },
	{ "Earpiece", NULL, "HPOUTR" },
};

static int bm1880_adau1372_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret = 0;
	int pll_rate;

	switch (params_rate(params)) {
	case 48000:
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 96000:
	case 192000:
		pll_rate = 48000 * 1024;
		break;
	default:
		return -EINVAL;
	}

/*	Remarked due to ADAU1372 seems doesn't need to set PLL */
#if 0
	ret = snd_soc_dai_set_pll(codec_dai, ADAU1372_PLL1,
			ADAU1372_PLL_SRC_MCLK1, 12288000, pll_rate);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, ADAU1372_CLK_SRC_PLL1, pll_rate,
			SND_SOC_CLOCK_IN);
#endif
	return ret;
}

static int bm1880_adau1372_codec_init(struct snd_soc_pcm_runtime *rtd)
{
/*  ADAU1372 use external oscillator to generate clock */
#if 0
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_rate = 48000 * 1024;
	int ret;

	ret = snd_soc_dai_set_pll(codec_dai, ADAU1372_PLL1,
			ADAU1372_PLL_SRC_MCLK1, 12288000, pll_rate);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, ADAU1372_CLK_SRC_PLL1, pll_rate,
			SND_SOC_CLOCK_IN);

	return ret;
#endif
	return 0;
}
static struct snd_soc_ops bm1880_adau1372_ops = {
	.hw_params = bm1880_adau1372_hw_params,
};

static struct snd_soc_dai_link bm1880_adau1372_dai = {
	.name = "bm-i2s",
	.stream_name = "adau1372-aif",
	.cpu_dai_name = "58012000.i2s",
	.codec_dai_name = "adau1372-aif",
	.platform_name = "58012000.i2s",
	.codec_name = "adau1372.0-003c",
	.ops = &bm1880_adau1372_ops,
	.init = bm1880_adau1372_codec_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_IB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
};

static struct snd_soc_dai_link bm1880_adau1372_dai_1 = {
	.name = "bm-i2s",
	.stream_name = "adau1372-aif",
	.cpu_dai_name = "58014000.i2s",
	.codec_dai_name = "adau1372-aif",
	.platform_name = "58014000.i2s",
	.codec_name = "adau1372.1-003c",
	.ops = &bm1880_adau1372_ops,
	.init = bm1880_adau1372_codec_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_IB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
};

static struct snd_soc_card bm1880_adau1372 = {
	.name = "bm_sound_card_0",
	.owner = THIS_MODULE,
	.dai_link = &bm1880_adau1372_dai,
	.num_links = 1,

	.dapm_widgets		= bm1880_adau1372_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(bm1880_adau1372_dapm_widgets),
	.dapm_routes		= bm1880_adau1372_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(bm1880_adau1372_dapm_routes),
};

static struct snd_soc_card bm1880_adau1372_1 = {
	.name = "bm_sound_card_1",
	.owner = THIS_MODULE,
	.dai_link = &bm1880_adau1372_dai_1,
	.num_links = 1,

	.dapm_widgets		= bm1880_adau1372_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(bm1880_adau1372_dapm_widgets),
	.dapm_routes		= bm1880_adau1372_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(bm1880_adau1372_dapm_routes),
};

static const struct of_device_id bm_audio_match_ids[] = {
	{
		.compatible = "bitmain,audio",
		//.data = (void *) &bm1880_adau1372_dai,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, bm_audio_match_ids);

static int bm1880_adau1372_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card;
	struct device_node *np = pdev->dev.of_node;

	dev_info(&pdev->dev, "%s, dev name=%s\n", __func__, dev_name(&pdev->dev));

	if (!strcmp(dev_name(&pdev->dev), "sound0")) {
		card = &bm1880_adau1372;

		if (np) {
			bm1880_adau1372_dai.cpu_of_node = of_parse_phandle(np, "bitmain,i2s-controller", 0);
			if (!bm1880_adau1372_dai.cpu_of_node) {
				dev_err(&pdev->dev, "Property 'bitmain,i2s-controller' missing or invalid\n");
			ret = -EINVAL;
			}
		card->dev = &pdev->dev;
		platform_set_drvdata(pdev, card);

		return devm_snd_soc_register_card(&pdev->dev, &bm1880_adau1372);
		}
	} else if (!strcmp(dev_name(&pdev->dev), "sound1")) {
		card = &bm1880_adau1372_1;

		if (np) {
			bm1880_adau1372_dai.cpu_of_node = of_parse_phandle(np, "bitmain,i2s-controller", 0);
			if (!bm1880_adau1372_dai.cpu_of_node) {
				dev_err(&pdev->dev, "Property 'bitmain,i2s-controller' missing or invalid\n");
			ret = -EINVAL;
			}
		card->dev = &pdev->dev;
		platform_set_drvdata(pdev, card);

		return devm_snd_soc_register_card(&pdev->dev, &bm1880_adau1372_1);
		}
	}

	return 0;

}

static struct platform_driver bm1880_adau1372_driver = {
	.driver = {
		.name = "bm1880-adau1372",
		.pm = &snd_soc_pm_ops,
		.of_match_table = bm_audio_match_ids,
	},
	.probe = bm1880_adau1372_probe,
};

module_platform_driver(bm1880_adau1372_driver);

MODULE_AUTHOR("EthanChen");
MODULE_DESCRIPTION("ALSA SoC bm1880 adau1372 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bm1880-adau1372");
