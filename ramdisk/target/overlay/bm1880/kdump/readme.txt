linux
cf891e3 Repair 128M boundary issue in SDHCI
089ef3f clear_page workaround
cea39dd memset workaround.
e2a1c13 change CPU to 1.5GHz.
3714e47 support docker
778d017 add an extra clean up to aovid invalid pointer.
28757b4 add soc mode interrupt support
20c573e Merge "Change spin lock to mutex lock to avoid dead loop."
71595e6 Change spin lock to mutex lock to avoid dead loop.
4e0031c refine npu driver
6d33bed add support for bmdnn
4b97136 bm1682 npu: map efuse register to user
43de8c2 fix kconfig typo.
fa006b2 add a node to switch on/off WDT.
9b34f19 refine I2C.
7619d6f remove I2C slave config for EVB and Box.
d7bf1d6 add new projects for embedded box and HD server.
98b56d1 set I2C clock at TOP setting.
0090bd9 change PWM log level.
4703748 as aliase for ethernet node.
adc55ca Merge "Add sysdma driver base on synopsys DesignWare DMA."
05292aa Add sysdma driver base on synopsys DesignWare DMA.
5508ad2 bm1880: npu: add resource config

ramdisk
1e10657 Merge "auto reboot after kdump."
329f42a auto reboot after kdump.
43dfd0d Merge "Refine S00update script for somecases"
c378a92 Refine S00update script for somecases
5d330b0 Merge "Refine recovery size from 122M to 72M"
31e5dde Refine recovery size from 122M to 72M
8c3408e downgrade protobuf library to v2.6.1
14cd048 Merge "update python2.7 include files"
1d4f544 update python2.7 include files
9526ba7 Merge "add include and library for caffe cross-compile"
1b74df7 add include and library for caffe cross-compile
202ab83 1. fix wrong link of libz.so, use link for libz.so.1
080b4cb 1. revert to commit 28bfc6a551a350bf5686cc9fcf292aab42d82535
3a417c4 add arm python include and library for opencv python binding build
28bfc6a fix revoery file list.
c1514af add scripts for kdump kernel.
0159994 Merge "change emmc boot log to highlight root partition name."
22310b9 change emmc boot log to highlight root partition name.
171558a Merge "add project support for ramdisk building."
5baa37f add project support for ramdisk building.
d564b36 Merge "add recovery file system build"
411a318 add recovery file system build
bc867f2 update library from debian deb pacakge