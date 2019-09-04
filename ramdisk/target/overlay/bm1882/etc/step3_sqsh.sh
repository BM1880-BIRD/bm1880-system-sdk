echo 'Executing step3_sqsh.sh...'
cd /data
tar zxvf emmc_sq.tar.gz
cd emmc_sqsh_pkg
dd if=emmcboot.itb.sqsh of=/dev/mmcblk0p1
dd if=recovery.itb of=/dev/mmcblk0p2
dd if=rootfs.sqsh of=/dev/mmcblk0p3
ls /
mount
rm -rf /data/emmc_sq.tar.gz /data/emmc_sqsh_pkg
sync
