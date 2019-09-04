#! /bin/sh

if [ ! -f "/tmp/edb_init" ];then

echo 1 > /tmp/edb_init
fi


#
#default pinmux setting.
#
#echo 1 > /sys/devices/platform/spi0-mux/bm_mux

#
#Auto run demo program.
#
#/demo/fdfr_demo.sh
#/demo/spoofing_demo.sh
