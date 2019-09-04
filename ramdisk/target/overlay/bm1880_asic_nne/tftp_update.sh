#version 1.1.4.ver8.0 (2019.08.20) change fullinstall saving ipaddr to uboot FOR RELEASE 1.1.4
usage=" Using sh tftp_update.sh var to trigger the tftp update
       [Pre-setting]: setup your PC host ip an boad ip ,put the file/image to  tftp folder,
                      run sh tftp_updte.sh init first to ensure the network.

       [Usage example]:
       sh tftp_update.sh  : trigger the emmc.tar.gz(rootfs/kernel uimage) update
       sh tftp_update.sh init : setup the board ip to 192.168.0.199
       sh tftp_update.sh [filename] : get the [filename] from tftp host to edge board /data/ folder
       sh tftp_update.sh  upload [filename] : upload [filename] from edge to PC host tftp folder
       sh tftp_update.sh fullinstall :reboot and erase partition and update full image from PC tftp folder.
                                    please make sure fip.bin prg.bin ramboot_mini.itb emmc.tar.gz exist in tftp folder
       "

#BOARD_IP=${BOARD_IP:-$(ifconfig | awk '/t addr:/{gsub(/.*:/,"",$2);print$2}')}
BOARD_IP=${BOARD_IP:-$(ifconfig | grep -A 1 'eth0' | tail -1 | cut -d ':' -f 2 | cut -d ' ' -f 1)}
HOST_IP=${HOST_IP:-192.168.0.200}
GATEWAY_IP=${GATEWAY_IP:-192.168.0.200}
DEBUG=${DEBUG:-0}
ifconfig eth0 ${BOARD_IP}
route add -host ${BOARD_IP} gw ${GATEWAY_IP}
ifconfig
mkdir -p /data
CMD=0
#FILE_NAME=""
#CMD = 0  #update image  default command
#CMD = 1  #update file from Host to board
#CMD = 2  #upload#upload file from board to Host
#CMD = 3  #set default board ip 192.168.0.199
#CMD = 4  #fullinstall#set default board ip
#CMD = 5  #show the help msg
#0x0400038C  //board ip
#0x04000390  //server ip
#0x04000394  //getway
#save ip from ifconfig
#BOARD_IP2=$(ifconfig | awk '/t addr:/{gsub(/.*:/,"",$2);print$2}')
#echo "SHOW BOARD_IP2=${BOARD_IP2}"
#echo "BOARD_IP get=${BOARD_IP}"
#echo "get finish"
#ip="$(ifconfig | grep -A 1 'eth0' | tail -1 | cut -d ':' -f 2 | cut -d ' ' -f 1)"
#echo "get another ip=${ip}"
#echo "get another finish"

#parsing board ip start .................................
#echo $BOARD_IP | awk -F. '{print $a,$b,$c,$d}'
AA=$(echo $BOARD_IP  | awk 'BEGIN {FS="."}; {print $1}')
BB=$(echo $BOARD_IP  | awk 'BEGIN {FS="."}; {print $2}')
CC=$(echo $BOARD_IP  | awk 'BEGIN {FS="."}; {print $3}')
DD=$(echo $BOARD_IP  | awk 'BEGIN {FS="."}; {print $4}')
echo "ip=$AA.$BB.$CC.$DD"
AA=$(($AA<<24))
BB=$(($BB<<16))
CC=$(($CC<<8))
#DD=$(($DD<<0))
BOARD_IP_SAVE=$(expr $AA + $BB)
BOARD_IP_SAVE=$(expr $BOARD_IP_SAVE + $CC)
BOARD_IP_SAVE=$(expr $BOARD_IP_SAVE + $DD)
echo "board_ip_reg=${BOARD_IP_SAVE}"
devmem 0x0400038C 32 $BOARD_IP_SAVE
#devmem 0x0400038C 32 $BOARD_IP
#parsing board ip end ..................................



#//parsing server ip start ................................
AA=$(echo $HOST_IP  | awk 'BEGIN {FS="."}; {print $1}')
BB=$(echo $HOST_IP  | awk 'BEGIN {FS="."}; {print $2}')
CC=$(echo $HOST_IP  | awk 'BEGIN {FS="."}; {print $3}')
DD=$(echo $HOST_IP  | awk 'BEGIN {FS="."}; {print $4}')
AA=$(($AA<<24))
BB=$(($BB<<16))
CC=$(($CC<<8))
#DD=$(($DD<<0))
SERVER_IP_SAVE=$(expr $AA + $BB)
SERVER_IP_SAVE=$(expr $SERVER_IP_SAVE + $CC)
SERVER_IP_SAVE=$(expr $SERVER_IP_SAVE + $DD)
echo "server_ip_reg=${SERVER_IP_SAVE}"
devmem 0x04000390 32 $SERVER_IP_SAVE
#devmem 0x04000390 32 $SERVER_IP
#//parsing server ip end ................................

#//parsing gateway ip start ................................
AA=$(echo $GATEWAY_IP  | awk 'BEGIN {FS="."}; {print $1}')
BB=$(echo $GATEWAY_IP  | awk 'BEGIN {FS="."}; {print $2}')
CC=$(echo $GATEWAY_IP  | awk 'BEGIN {FS="."}; {print $3}')
DD=$(echo $GATEWAY_IP  | awk 'BEGIN {FS="."}; {print $4}')
AA=$(($AA<<24))
BB=$(($BB<<16))
CC=$(($CC<<8))
#DD=$(($DD<<0))
GATEWAY_IP_SAVE=$(expr $AA + $BB)
GATEWAY_IP_SAVE=$(expr $GATEWAY_IP_SAVE + $CC)
GATEWAY_IP_SAVE=$(expr $GATEWAY_IP_SAVE + $DD)
echo "gatway_ip_reg=${GATEWAY_IP_SAVE}"
devmem 0x04000394 32 $GATEWAY_IP_SAVE
#//parsing gateway ip end ................................


echo "HOST_IP = ${HOST_IP}"
echo "BOARD_IP = ${BOARD_IP}"
echo "GATEWAY_IP = ${GATEWAY_IP}"

if [ -z "$1" ]; then
  CMD=0
  echo "v0"
else
  if [ "$1" == "upload" ] && [ $# > 1 ]; then
    CMD=2
    echo "v2"
  elif [ "$1" == "init" ] && [ $# > 1 ]; then
    CMD=3
    echo "v3"
  elif [ "$1" == "fullinstall" ] && [ $# > 1 ]; then
    CMD=4
    echo "v4"
  elif [ "$1" == "-h" ] && [ $# > 1 ]; then
    CMD=5
    echo "v5"
  else
    CMD=1
    echo "v1"
  fi
fi

function update_image(){
    busybox mdev -s
    #/etc/step1.sh
    #check partition
    echo "turn off powersaving"
    orgvar=`devmem 0x50010804`
    devmem 0x50010804 32 0xFFFFFFFF
    echo "checking partition..."
    sfdisk -d /dev/mmcblk0 > emmc_partition
    diff emmc_partition /etc/emmc.sfdisk
    if [ $? == 1 ]; then
        echo "[Warning]cannt force format eMMC in linux system"
        #sfdisk /dev/mmcblk0 < /etc/emmc.sfdisk
        #mdev -s
        #busybox mke2fs -T ext4 /dev/mmcblk0p3
        #busybox mke2fs -T ext4 /dev/mmcblk0p4
        #busybox mke2fs -T ext4 /dev/mmcblk0p6
    else
        echo "partition is identical"
    fi
    devmem 0x04000384 32 0x10000000
    mkdir /data
    mount /dev/mmcblk0p6 /data
    rm -rf /data/emmc.tar.gz /data/emmc_nnmc_pkg
    mkdir /rootfs
    mount /dev/mmcblk0p3 /rootfs
    cd /data
    sfdisk -d /dev/mmcblk0
    mdev -s
    #settup network ip
    flag=`devmem 0x04000384`
    ifconfig eth0 ${BOARD_IP}
    route add -host 192.168.0.101 gw 192.168.0.202
    ifconfig
    echo "..................................................."

    #start tftp transmition
    devmem 0x04000384 32 0x11000000
    echo "bmtftp transmition start .................plz wait..."
    bmtftp -g -r emmc.tar.gz ${HOST_IP} -l /data/emmc.tar.gz
    if [ "$?" == "0" ]; then
        echo "bmtftp transmition success"
        devmem 0x04000384 32 0x11100000
    else
        echo "bmtftp failure [Plz check emmc.tar.gz  exist / check connection ok?]"
        devmem 0x04000384 32 0x00000FFF
        exit
    fi

    #start untar file and update emmc
    #/etc/step3.sh
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

    #Clear bmtb flag and reboot ..remount
    devmem 0x04000384 32 0x00000111
    echo "bmtftp download finished!! "
    echo "save back  powersaving"
    devmem 0x50010804 32 $orgvar
    echo "rebooting ..."
    reboot -f
}

case $CMD in
    0)
    echo "Start update image ...."
    echo "make sure the emmc.tar.gz is in the  tftp host side "

    update_image

    ;;

    1)
    echo "Update file  $1 from Host"
    bmtftp -g -r $1  ${HOST_IP} -l /data/$1
    if [ "$?" == "0" ]; then
        echo "bmtftp transmition success"
    else
        echo "bmtftp failure [Plz check file exist / check connection ok?]"
    fi
    echo "..bmtftp finished!  in /data/ folder"
    ###touch download_complete
    ###chmod 777 download_complete
    ###tftp -p -l download_complete ${HOST_IP}
    i=1

    while [ "$i" -le "$#" ]; do
    eval "arg=\${$i}"
    printf '%s\n' "Arg $i: $arg"
    bmtftp -g -r $arg  ${HOST_IP} -l /data/$arg
    if [ "$?" == "0" ]; then
        echo "bmtftp transmition success"
    else
        echo "bmtftp failure [Plz check file exist / check connection ok?]"
    fi
    i=$((i + 1))
    done

    ;;
    2 )
    echo "Upload file to Host"
    #tftp -p -l $2 ${HOST_IP}
    j=2
    while [ "$j" -le "$#" ]; do
    eval "arg=\${$j}"
    printf '%s\n' "Arg $j: $arg"
    bmtftp -p -l $arg ${HOST_IP}
    if [ "$?" == "0" ]; then
        echo "bmtftp transmition success"
    else
        echo "bmtftp failure [Plz check file exist / check connection ok?]"
    fi
    j=$((j + 1))
    done

    ;;
    3 )
    echo "set default ip "
    ifconfig eth0 ${BOARD_IP}
    route add -host ${BOARD_IP} gw ${GATEWAY_IP}
    ifconfig

    ;;
    4 )
    echo "fullinstall(clean update)"
    devmem 0x50010804 32 0xFFFFFFFF
    devmem 0x04000384
    devmem 0x04000384 32 0x626D7463
    reboot -f
    ;;

    5 )
    echo "-h"
    echo "$usage"
    ;;
esac





