#!/bin/bash

# User config
# ====================================================================
CHIP=${CHIP:-bm1880}
SUBTYPE=${SUBTYPE:-asic}
BOARD=${BOARD:-edb}
DEBUG=${DEBUG:-0}
TAG=${TAG:-}
# ====================================================================

# to be compatible with non-board projects
uBOARD=
if [ ! -z $BOARD ]; then
    uBOARD="_$BOARD"
fi

# security feature configuration
IMG_ENC=0
IMG_ENC_KSRC=dev
#    keyfile : read key from key file. IMG_ENC_KPATH should contain the path
#    dev     : dev mode, using key = 0
IMG_ENC_KPATH=

function gettop
{
  local TOPFILE=build/envsetup_edb.sh
  if [ -n "$TOP" -a -f "$TOP/$TOPFILE" ] ; then
    # The following circumlocution ensures we remove symlinks from TOP.
    (cd $TOP; PWD= /bin/pwd)
  else
    if [ -f $TOPFILE ] ; then
      # The following circumlocution (repeated below as well) ensures
      # that we record the true directory name and not one that is
      # faked up with symlink names.
      PWD= /bin/pwd
    else
      local HERE=$PWD
      T=
      while [ \( ! \( -f $TOPFILE \) \) -a \( $PWD != "/" \) ]; do
        \cd ..
        T=`PWD= /bin/pwd -P`
      done
      \cd $HERE
      if [ -f "$T/$TOPFILE" ]; then
        echo $T
      fi
    fi
  fi
}


function build_kernel()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi
  if [ ! -d $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER ] ; then
    mkdir -p $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  fi

  pushd $KERNEL_PATH
  if [ ! -x "$KERNEL_OUTPUT_FOLDER" ]; then
    mkdir -p $KERNEL_OUTPUT_FOLDER
  fi
  if [ ! -z "$TAG" ]; then
      echo "kernel checkout tag $TAG"
      git checkout $TAG
  fi

  make ARCH=arm64 O=./$KERNEL_OUTPUT_FOLDER bitmain_${PROJECT_FULLNAME}_defconfig
  pushd ./$KERNEL_OUTPUT_FOLDER
  make ARCH=arm64 -j8 Image dtbs
  make ARCH=arm64 modules modules_install INSTALL_MOD_PATH="./modules"
  make ARCH=arm64 headers_install
  KERNELRELEASE=$(make ARCH=arm64 kernelrelease)
  popd

  ret=$?
  popd
  if [ $ret -ne 0 ]; then
    echo "making kernel failed"
    return $ret
  fi

  # copy kernel image and device tree
  cp $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/arch/arm64/boot/Image $OUTPUT_DIR/
  cp $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/arch/arm64/boot/dts/bitmain/${PROJECT_FULLNAME}.dtb $OUTPUT_DIR/
  cp $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/vmlinux $OUTPUT_DIR/
  cp $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/arch/arm64/boot/Image $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  cp $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/arch/arm64/boot/dts/bitmain/${PROJECT_FULLNAME}.dtb $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER

  # copy modules and header files to ramdisk
  if [ ! -d $RAMDISK_PATH/target/lib/modules/$KERNELRELEASE ] ; then
    mkdir -p $RAMDISK_PATH/target/lib/modules/$KERNELRELEASE
  fi
  cp -r $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/modules/lib/modules/$KERNELRELEASE/kernel $RAMDISK_PATH/target/lib/modules/$KERNELRELEASE
  cp -r $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/modules/lib/modules/$KERNELRELEASE/modules.* $RAMDISK_PATH/target/lib/modules/$KERNELRELEASE
  cp -r $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/usr/include $RAMDISK_PATH/prebuild/include/linux/

  # creat fixed link for out-of-tree module build
  pushd $KERNEL_PATH/linux/
  ln -sf $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER ${CHIP}_${SUBTYPE}
  popd
}

function clean_kernel()
{
  rm -rf $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER
  mkdir -p $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER
  cp -rf $BUILD_PATH/overlay/$KERNEL_OBJ_OVERLAY_PATH/* $KERNEL_PATH/$KERNEL_OUTPUT_FOLDER/
  rm -f $OUTPUT_DIR/Image
  rm -f $OUTPUT_DIR/vmlinux
  rm -f $OUTPUT_DIR/${PROJECT_FULLNAME}.dtb
  rm -f $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER/Image
  rm -f $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER/${PROJECT_FULLNAME}.dtb
  rm -rf $RAMDISK_PATH/target/lib/modules/
  rm -rf $RAMDISK_PATH/prebuild/include/linux
}

function build_recovery()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi
  if [ ! -d $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER ] ; then
    mkdir -p $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  fi

  pushd $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  rm -f recovery_files.txt

  cat $RAMDISK_PATH/scripts/recovery_fixed_files.txt > recovery_files.txt
  $RAMDISK_PATH/scripts/gen_initramfs_list.sh $RAMDISK_PATH/target/etc | sed 's/\//\/etc\//' >> recovery_files.txt
  $RAMDISK_PATH/scripts/gen_init_cpio recovery_files.txt > recovery.cpio

  if [ -f $RAMDISK_PATH/scripts/${PROJECT_FULLNAME}_multi.its ]; then
    cp $RAMDISK_PATH/scripts/${PROJECT_FULLNAME}_multi.its ./multi.its
  else
    cp $RAMDISK_PATH/scripts/multi.its ./multi.its
  fi
  sed -i "s/data = \/incbin\/(\".\/rootfs.cpio\");/data = \/incbin\/(\".\/recovery.cpio\");/g" multi.its
  sed -i "s/data = \/incbin\/(\".\/bitmain.dtb\");/data = \/incbin\/(\".\/${PROJECT_FULLNAME}.dtb\");/g" multi.its

  $TOP_DIR/build/mkimage -f multi.its -k $VBOOT_PATH recovery.itb
  cp ./recovery.itb $OUTPUT_DIR/
  popd
}


function build_emmcboot()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi
  if [ ! -d $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER ] ; then
    mkdir -p $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  fi

  pushd $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  rm -f emmcboot_files.txt

  cat $RAMDISK_PATH/scripts/emmcboot_fixed_files.txt >> emmcboot_files.txt
  $RAMDISK_PATH/scripts/gen_init_cpio emmcboot_files.txt > emmcboot.cpio

  if [ -f $RAMDISK_PATH/scripts/${PROJECT_FULLNAME}_multi.its ]; then
    cp $RAMDISK_PATH/scripts/${PROJECT_FULLNAME}_multi.its ./multi.its
  else
    cp $RAMDISK_PATH/scripts/multi.its ./multi.its
  fi

  sed -i "s/data = \/incbin\/(\".\/rootfs.cpio\");/data = \/incbin\/(\".\/emmcboot.cpio\");/g" multi.its
  
  sed -i "s/data = \/incbin\/(\".\/bitmain.dtb\");/data = \/incbin\/(\".\/${PROJECT_FULLNAME}.dtb\");/g" multi.its

  $TOP_DIR/build/mkimage -f multi.its -k $VBOOT_PATH emmcboot.itb
  cp ./emmcboot.itb $OUTPUT_DIR/
  popd
}


function build_sdboot()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi
  if [ ! -d $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER ] ; then
    mkdir -p $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  fi

  pushd $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  rm -f sdboot_files.txt

  cat $RAMDISK_PATH/scripts/sdboot_fixed_files.txt >> sdboot_files.txt
  $RAMDISK_PATH/scripts/gen_init_cpio sdboot_files.txt > sdboot.cpio

  if [ -f $RAMDISK_PATH/scripts/${PROJECT_FULLNAME}_multi.its ]; then
    cp $RAMDISK_PATH/scripts/${PROJECT_FULLNAME}_multi.its ./multi.its
  else
    cp $RAMDISK_PATH/scripts/multi.its ./multi.its
  fi
  sed -i "s/data = \/incbin\/(\".\/rootfs.cpio\");/data = \/incbin\/(\".\/sdboot.cpio\");/g" multi.its
  sed -i "s/data = \/incbin\/(\".\/bitmain.dtb\");/data = \/incbin\/(\".\/${PROJECT_FULLNAME}.dtb\");/g" multi.its

  $TOP_DIR/build/mkimage -f multi.its -k $VBOOT_PATH sdboot.itb
  cp ./sdboot.itb $OUTPUT_DIR/
  internal_enc_helper $OUTPUT_DIR/sdboot.itb
  popd
}

function build_rootfs()
{
  if [ ! -d $ROOTFS_DIR ] ; then
    mkdir -p $ROOTFS_DIR
  fi
  if [ ! -d $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER ] ; then
    mkdir -p $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  fi

  pushd $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  rm -f rootfs_files.txt

  # customization
  cat $RAMDISK_PATH/scripts/ramboot_fixed_files.txt > rootfs_files.txt

  $RAMDISK_PATH/scripts/gen_initramfs_list.sh $RAMDISK_PATH/target >> rootfs_files.txt
  $RAMDISK_PATH/scripts/gen_init_cpio rootfs_files.txt > rootfs.cpio
  popd

  pushd $ROOTFS_DIR
  cat $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER/rootfs.cpio | fakeroot cpio -i
  popd

  if [ -d $ROOTFS_OVERLAY ]; then
    echo copy overlay files...
    cp -rpf $ROOTFS_OVERLAY/* $ROOTFS_DIR
  fi
}

pack_emmc_image(){
  build_emmcboot
  build_recovery
  cp $OUTPUT_DIR/recovery.itb $BUILD_PATH/image_tool/images/
  cp $OUTPUT_DIR/emmcboot.itb $BUILD_PATH/image_tool/images/
  cp $OUTPUT_DIR/fip.bin $BUILD_PATH/image_tool/images/
  cp -r -f $ROOTFS_DIR $BUILD_PATH/image_tool/images/
  pushd $BUILD_PATH/image_tool
  ./creat_pack_for_release.sh auto
  mv ./bm_update.img $OUTPUT_DIR/
  popd

}

function clean_ramdisk()
{
  rm -rf $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER
  rm -f $OUTPUT_DIR/*.cpio
  rm -f $OUTPUT_DIR/*.itb
  rm -f $BUILD_PATH/image_tool/images/*.itb
}

function clean_rootfs()
{
  rm -f $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER/rootfs_files.txt
  rm -f $RAMDISK_PATH/$RAMDISK_OUTPUT_FOLDER/rootfs.cpio
  mv $ROOTFS_DIR/system $TOP_DIR/
  rm -rf $ROOTFS_DIR/*
  mv $TOP_DIR/system $ROOTFS_DIR/
  rm -rf $BUILD_PATH/image_tool/images/rootfs
}

function enc_image()
{
  if [ "$1" == "keyfile" ] ; then
    if [ $# != 4 ] ; then echo "command shoud be \"enc_image keyfile [keyfile path] [input file path] [output file path]\"" && return ; fi
    if [ ! -e $2 ] ; then echo $2 "file not found" && return ; fi
    if [ ! -e $3 ] ; then echo $3 "file not found" && return ; fi
    echo para num=$#, keyfile path=$2, input file path=$3, output file path=$4
    key_output=$(cat $2 | base64 -d)
    input_file=$3
    output_file=$4
  elif [ "$1" == "dev" ] ; then
    if [ $# != 3 ] ; then echo "command shoud be \"enc_image dev [input file path] [output file path]\"" && return ; fi
    if [ ! -e $2 ] ; then echo $2 "file not found" && return ; fi
    key_output=0000000000000000000000000000000000000000000000000000000000000000
    input_file=$2
    output_file=$3
  else
    echo "command should be \"enc_image keyfile\" or \"enc_image dev\"" && return
  fi
  echo "Image encryption is enabled. Image encrypting..."
  openssl enc -aes-256-cbc -nosalt -in $input_file -out $output_file -K $key_output -iv 0000000000000000
  echo "Image encryption finish"
}

function internal_enc_helper()
{
  if [ $IMG_ENC == 1 ] ; then
    if [ "$IMG_ENC_KSRC" == "keyfile" ] ; then
      enc_image $IMG_ENC_KSRC $IMG_ENC_KPATH $1 $1.enc
    elif [ "$IMG_ENC_KSRC" == "dev" ] ; then
      enc_image $IMG_ENC_KSRC $1 $1.enc
    else echo "wrongly invoke internal_enc_helper()"
    fi
    mv -f $1.enc $1
    rm -rf $1.enc
  fi
}

function build_all()
{
  build_kernel
  build_sdboot
  build_rootfs
  pack_emmc_image
}

function clean_all()
{
  clean_kernel
  clean_ramdisk
  clean_rootfs
}


TOP_DIR=$(gettop)
PROJECT_FULLNAME=${CHIP}_${SUBTYPE}${uBOARD}

# output folder path
OUTPUT_DIR=$TOP_DIR/install/soc_$PROJECT_FULLNAME
ROOTFS_DIR=$OUTPUT_DIR/rootfs

# source file folders
VBOOT_PATH=$TOP_DIR/build/vboot
KERNEL_PATH=$TOP_DIR/linux-linaro-stable
RAMDISK_PATH=$TOP_DIR/ramdisk
ROOTFS_OVERLAY=${RAMDISK_PATH}/overlay/soc_${PROJECT_FULLNAME}
BUILD_PATH=$TOP_DIR/build

# subfolder path for buidling, chosen accroding to .gitignore rules
KERNEL_OUTPUT_FOLDER=linux/$PROJECT_FULLNAME
RAMDISK_OUTPUT_FOLDER=build/$PROJECT_FULLNAME
KERNEL_OBJ_OVERLAY_PATH=/linux/$PROJECT_FULLNAME

# toolchain
export CROSS_COMPILE_64=${CROSS_COMPILE_64:-$TOP_DIR/host-tools/gcc/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-}
export CROSS_COMPILE=$CROSS_COMPILE_64

# print key options
echo "TOP=$TOP_DIR"
echo "CHIP=$CHIP, SUBTYPE=$SUBTYPE, BOARD=$BOARD, DEBUG=$DEBUG, TAG=$TAG"
echo "Project: $PROJECT_FULLNAME"
echo "cross comipler path: CROSS_COMPILE=$CROSS_COMPILE"
