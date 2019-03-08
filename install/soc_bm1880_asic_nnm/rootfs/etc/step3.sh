cd /data
tar zxvf emmc.tar.gz
cd emmc_nnmc_pkg
dd if=emmcboot.itb of=/dev/mmcblk0p1
dd if=recovery.itb of=/dev/mmcblk0p2
ls /
mount
cp rootfs.tar.gz /rootfs/
cd /rootfs; tar zxvf rootfs.tar.gz --strip-components=1
rm /rootfs/rootfs.tar.gz
rm -rf /data/emmc.tar.gz /data/emmc_nnmc_pkg
sync
