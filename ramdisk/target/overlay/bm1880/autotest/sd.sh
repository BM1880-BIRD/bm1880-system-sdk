#!/bin/bash

#$1 is R/W test size

#echo parameter number is "$#"

if test "$#" -ne 1; then
    echo Illegal number of parameters
    echo "$0: failed"
    exit 1
fi

device="/dev/mmcblk1p2"
folder="/testsd"

if [ ! -e "$folder" ]
then
    echo "Create test folder!"
    mkdir "$folder"
    pwd
else
    echo "test folder exists!"
fi

cd "$folder"


if [ ! -e "$device" ]
then
    echo "No device!"
    ls /dev/mmcblk*
    echo "$0: failed"
    exit 1
else
    echo "Found device!"
fi

if mount | grep "$device" > /dev/null; 
then
    echo "Already mount"
else
    mount "$device" "$folder"
    if [ $? == 0 ]
    then
        echo "Mount successfully"
    else
        echo "$0: failed"
        exit 1
    fi
fi

echo "Testing write..."
dd of="$folder"/testimg.bin if=/dev/urandom bs=1M count="$1"

echo "Testing read..."
dd if="$device" of=/dev/null bs=1M count="$1"

cd -

umount "$device"

echo "$0: passed"
