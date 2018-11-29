#
# Makefile for the Bitmain network device drivers.
#

obj-$(CONFIG_DWC_ETH_QOS) += DWC_ETH_QOS.o
DWC_ETH_QOS-y += DWC_ETH_QOS_dev.o \
			DWC_ETH_QOS_drv.o \
			DWC_ETH_QOS_desc.o \
			DWC_ETH_QOS_ethtool.o \
			DWC_ETH_QOS_mdio.o \
			DWC_ETH_QOS_eee.o

DWC_ETH_QOS-$(CONFIG_DEBUGFS_OBJ)	+= DWC_ETH_QOS_debug_operation.o
DWC_ETH_QOS-$(CONFIG_PGTEST_OBJ)	+= DWC_ETH_QOS_pktgen.o
DWC_ETH_QOS-$(CONFIG_PTPSUPPORT_OBJ)	+= DWC_ETH_QOS_ptp.o

obj-$(CONFIG_DWCEQOS_PCI)		+= DWC_ETH_QOS_pci.o
obj-$(CONFIG_DWCEQOS_BITMAIN)		+= DWC_ETH_QOS_bm.o
