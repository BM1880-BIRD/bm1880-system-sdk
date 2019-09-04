cd /data
DATA_FREE_SPACE=`df -k /data | tail -n1|awk '{print $4}'`
DATA_FREE_SPACE_MB=`expr $DATA_FREE_SPACE / 1024`
echo "Data partition free space(MB):"
echo $DATA_FREE_SPACE_MB

EMMC_TAR_GZ_SIZE_MB=`du -m emmc.tar.gz | awk '{print $1}'`
echo "emmc.tar.gz size(MB)"
echo $EMMC_TAR_GZ_SIZE_MB

DATA_NEED_SPACE_MB=`expr $EMMC_TAR_GZ_SIZE_MB \* 2`
echo "Data partition need space(MB)"
echo $DATA_NEED_SPACE_MB

if [ $DATA_NEED_SPACE_MB -lt $DATA_FREE_SPACE_MB ]; then
  echo "It is able to upgrade"
else
  NEED_SPACE=`expr $DATA_NEED_SPACE_MB - $DATA_FREE_SPACE_MB`
  echo "No enough free space, stop upgrade"
  echo "Need more free space(MB)"
  echo $NEED_SPACE
  exit
fi

tar zxvf emmc.tar.gz
cd emmc_nnmc_pkg

ROOTFS_FREE_SPACE=`df -k /rootfs | tail -n1|awk '{print $4}'`
ROOTFS_FREE_SPACE_MB=`expr $ROOTFS_FREE_SPACE / 1024`
echo "rootfs partition free space(MB) :"
echo $ROOTFS_FREE_SPACE_MB

ROOTFS_TAR_GZ_SIZE_MB=`du -m rootfs.tar.gz | awk '{print $1}'`
echo "rootfs.tar.gz size(MB)"
echo $ROOTFS_TAR_GZ_SIZE_MB

ROOTFS_NEED_SPACE_MB=`expr $ROOTFS_TAR_GZ_SIZE_MB \* 2`
echo "rootfs partition need space(MB)"
echo $ROOTFS_NEED_SPACE_MB

if [ $ROOTFS_NEED_SPACE_MB -lt $ROOTFS_FREE_SPACE_MB ]; then
  echo "It is able to upgrade"
else
  NEED_SPACE=`expr $ROOTFS_NEED_SPACE_MB - $ROOTFS_FREE_SPACE_MB`
  echo "No enough free space, stop upgrade rootfs partition"
  echo "Need more free space(MB)"
  echo $NEED_SPACE
  exit
fi

dd if=emmcboot.itb of=/dev/mmcblk0p1
dd if=recovery.itb of=/dev/mmcblk0p2
ls /
mount
cp rootfs.tar.gz /rootfs/
cd /rootfs


tar zxvf rootfs.tar.gz --strip-components=1
rm /rootfs/rootfs.tar.gz
rm -rf /data/emmc.tar.gz /data/emmc_nnmc_pkg
sync
