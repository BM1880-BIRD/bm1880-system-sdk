echo "checking partition..."
sfdisk -d /dev/mmcblk0 > emmc_partition
diff emmc_partition /etc/emmc.sfdisk
if [ $? == 1 ]; then
  echo "format eMMC"
  sfdisk /dev/mmcblk0 < /etc/emmc.sfdisk
  mdev -s
  busybox mke2fs -T ext4 /dev/mmcblk0p3
  busybox mke2fs -T ext4 /dev/mmcblk0p4
  busybox mke2fs -T ext4 /dev/mmcblk0p6
else
  echo "partition is identical"
fi
mkdir /data
mount /dev/mmcblk0p6 /data
rm -rf /data/emmc.tar.gz /data/emmc_nnmc_pkg

mkdir /rootfs
mount /dev/mmcblk0p3 /rootfs

cd /data
sfdisk -d /dev/mmcblk0
mdev -s
ls /dev/bmusb_tx > /dev/null
while [ $? == 1 ]; do
  mdev -s
  ls /dev/bmusb_tx > /dev/null
done
ls /dev/bmusb_rx > /dev/null
while [ $? == 1 ]; do
  mdev -s
  ls /dev/bmusb_rx > /dev/null
done
