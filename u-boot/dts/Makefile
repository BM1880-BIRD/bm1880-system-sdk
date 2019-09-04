#
# Copyright (c) 2011 The Chromium OS Authors.
#
# SPDX-License-Identifier:	GPL-2.0+
#

# This Makefile builds the internal U-Boot fdt if CONFIG_OF_CONTROL is
# enabled. See doc/README.fdt-control for more details.

ifeq ($(CONFIG_OF_BOARD),y)
DEVICE_TREE ?= $(subst $\",, $(CONFIG_DEFAULT_DEVICE_TREE))
else
DEVICE_TREE ?= $(CONFIG_DEFAULT_DEVICE_TREE:"%"=%)
endif
ifeq ($(DEVICE_TREE),)
DEVICE_TREE := unset
endif

ARCH_PATH := arch/$(ARCH)/dts
dtb_depends := arch-dtbs

ifneq ($(EXT_DTB),)
DTB := $(EXT_DTB)
else
ifeq ($(CONFIG_OF_BOARD),y)
DTB := $(patsubst %, $(ARCH_PATH)/%.dtb, $(DEVICE_TREE))
else
DTB := $(ARCH_PATH)/$(DEVICE_TREE).dtb
endif
dtb_depends += $(DTB:.dtb=.dts)
endif

ifeq ($(CONFIG_OF_BOARD),y)
$(obj)/dt.dtb: $(DTB) FORCE
	@$(objtree)/tools/dtb_combine $(DTB) > $@
else
$(obj)/dt.dtb: $(DTB) FORCE
	$(call if_changed,shipped)
endif

targets += dt.dtb

$(DTB): $(dtb_depends)
ifeq ($(EXT_DTB),)
	$(Q)$(MAKE) $(build)=$(ARCH_PATH) $@
endif
	$(Q)test -e $@ || (						\
	echo >&2;							\
	echo >&2 "Device Tree Source is not correctly specified.";	\
	echo >&2 "Please define 'CONFIG_DEFAULT_DEVICE_TREE'";		\
	echo >&2 "or build with 'DEVICE_TREE=<device_tree>' argument";	\
	echo >&2;							\
	/bin/false)

arch-dtbs:
	$(Q)$(MAKE) $(build)=$(ARCH_PATH) dtbs

.SECONDARY: $(obj)/dt.dtb.S

obj-$(CONFIG_OF_EMBED) := dt.dtb.o

dtbs: $(obj)/dt.dtb
	@:

clean-files := dt.dtb.S

# Let clean descend into dts directories
subdir- += ../arch/arm/dts ../arch/microblaze/dts ../arch/mips/dts ../arch/sandbox/dts ../arch/x86/dts
