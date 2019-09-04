#!/bin/bash
#Gate watchdog reset signal to SOC
#otherwise, during kdump, the wdt will reset SOC.
devmem 0x50010008 32 0x0;
#Run kdump cmds
kexec -p ./Image --initrd=./kdump.cpio --dtb=./bm1880_asic_edb_no_wdt.dtb --append="1 maxcpus=1 reset_devices console=ttyS0,115200 earlycon debug user_debug=31 loglevel=10"
