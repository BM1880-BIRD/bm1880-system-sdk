#!/bin/sh
sleep 1
export LD_LIBRARY_PATH=/system/lib
cd /system/data
./load_jpu.sh
rm -rf /dev/bm-npu0
insmod bmnpu.ko
mdev -s
cd /system/bin
./test_libusb_fdfr ./fdfr.conf
