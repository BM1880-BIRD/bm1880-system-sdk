echo "step1_sqsh:checking partition..."
sfdisk -d /dev/mmcblk0 > emmc_partition
diff emmc_partition /etc/emmc_sqsh.sfdisk
if [ $? == 1 ]; then
  echo "Format eMMC..."
  sfdisk /dev/mmcblk0 < /etc/emmc_sqsh.sfdisk
  mdev -s
  busybox mke2fs -T ext4 /dev/mmcblk0p4
  busybox mke2fs -T ext4 /dev/mmcblk0p6
else

  flag_update=`devmem 0x04000384`
  if [ $flag_update == 0x626D7462 ]; then
    echo "force format eMMC"
    sfdisk /dev/mmcblk0 < /etc/emmc.sfdisk.sqsh
    mdev -s
    busybox mke2fs -T ext4 /dev/mmcblk0p4
    busybox mke2fs -T ext4 /dev/mmcblk0p6
  fi

  echo "partition is identical"
fi
mkdir /data
mount -t ext4 /dev/mmcblk0p6 /data
rm -rf /data/emmc.tar.gz /data/emmc_nnmc_pkg

cd /data
sfdisk -d /dev/mmcblk0
mdev -s
pwd

flag=`devmem 0x04000384`
if [ $flag == 0x626D7462 ]; then
    echo "ramdisk network mode"
    ifconfig eth0 192.168.0.199
    ifconfig
elif [ $flag == 0x626D7262 ]; then
    echo "ramdisk force usb-dev mode"
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
elif [ $flag == 0x626D7371 ]; then
    echo "ramdisk force usb-dev mode"
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
else
    echo "ramdisk default usb-dev mode"
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
fi
