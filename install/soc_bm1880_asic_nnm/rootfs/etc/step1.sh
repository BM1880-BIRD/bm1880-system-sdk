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
  echo "Partition is identical"

  sleep 2

  read -t 5 -p "Do you want to format rootfs/data parition(enter YES to format)?" answer

  if [ "$answer" == "YES" ] ;then

      echo Format rootfs partition;
      busybox mke2fs -T ext4 /dev/mmcblk0p3;

      echo Format data partition;
      busybox mke2fs -T ext4 /dev/mmcblk0p6;
  fi
fi

mkdir /data
mount -t ext4 /dev/mmcblk0p6 /data
rm -rf /data/emmc.tar.gz /data/emmc_nnmc_pkg

mkdir /rootfs
mount -t ext4 /dev/mmcblk0p3 /rootfs

cd /data
sfdisk -d /dev/mmcblk0
mdev -s

flag=`devmem 0x04000384`
if [ $flag == 0x626D7462 ]; then
    echo "ramdisk network mode(tftp)"
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
