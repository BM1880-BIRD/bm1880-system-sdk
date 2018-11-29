# Bitmain Machine Support
snd-soc-bitmain-adau1372-objs := bitmain_adau1372.o

obj-$(CONFIG_SND_SOC_BITMAIN_ADAU1372) += snd-soc-bitmain-adau1372.o

# Bitmain Platform Support
# snd-soc-bitmain-i2s-objs := bitmain_i2s.o
# snd-soc-bitmain-pcm-objs := bitmain_pcm.o

obj-$(CONFIG_SND_BITMAIN_I2S) += bitmain_i2s.o
ifdef CONFIG_SND_BITMAIN_PCM
obj-$(CONFIG_SND_BITMAIN_I2S) += bitmain_pcm.o
endif