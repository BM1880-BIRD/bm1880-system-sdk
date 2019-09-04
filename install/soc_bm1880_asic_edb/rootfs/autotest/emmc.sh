#!/bin/bash

#$1 is R/W test size

#echo parameter number is "$#"

if test "$#" -ne 1; then
    echo Illegal number of parameters
    echo "$0: failed"
    exit 1
fi

folder="/testemmc"

if [ ! -e "$folder" ]
then
    echo "Create test folder!"
    mkdir "$folder"
    pwd
else
    echo "test folder exists!"
fi

cd "$folder"

echo "Testing write..."
dd of="$folder"/testemmc.bin if=/dev/urandom bs=1M count="$1"

echo "Testing read..."
dd if=/dev/mmcblk0 of=/dev/null bs=1M count="$1"

cd -

echo "$0: passed"
