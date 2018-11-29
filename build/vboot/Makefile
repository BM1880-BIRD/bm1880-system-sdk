host_arch:=$(shell uname -m)

ifeq ($(host_arch),x86_64)
mkimage?=./mkimage-amd64
else
mkimage?=./mkimage-i386
endif


ifeq ($(strip $V),1)
    Q=
    E=
else
    Q=@
    E=1>/dev/null 2>&1
endif

cmd=	if [ ! -f $(mkimage) ] && ! which $(mkimage) ; then	\
		echo 'mkimage tools not found at $(mkimage)'; 	\
		exit 1;						\
	elif [ ! -x $(1) ]; then				\
		chmod +x $(1);					\
		$(1) $(2);					\
		chmod -x $(1);					\
	else							\
		$(1) $(2);					\
	fi

all:vboot.bin vboot.crt vboot.itb $(uboot_dtb)
vboot.bin:
	$(Q)echo $@
	$(Q)echo 'HelloWorld' > $@
vboot.crt:vboot.key
	$(Q)echo $@
	$(Q)touch ~/.rnd $(E)
	$(Q)openssl req -batch -new -x509 -key $< -out $@ $(E)
vboot.itb:vboot.its $(mkimage)
	$(Q)echo $@
	$(Q)$(call cmd,$(mkimage),-f $< -k . $@) $(E)

ifneq ($(uboot_dtb),)
$(uboot_dtb):$(mkimage) vboot.itb
	echo 'update dtb'
	$(Q)$(call cmd,$(mkimage),-F -k . -K $@ -r vboot.itb) $(E)
endif

.PHONY:clean
clean:
	$(Q)echo $@
	$(Q)rm -rf vboot.bin vboot.crt vboot.itb $(E)
