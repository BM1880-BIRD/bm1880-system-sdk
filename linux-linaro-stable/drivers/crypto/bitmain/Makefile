obj-$(CONFIG_CRYPTO_DEV_BCE) := bce.o
ccflags-y += -I$(src)
bce-objs += ce_cipher.o ce_hash.o ce_inst.o ce_dev.o ce_soft_hash.o
ifdef CONFIG_CRYPTO_DEV_BCE_DUMMY
bce-objs += ce_dummy.o
else
bce-objs += ce.o
endif
