#!/bin/bash

alg_define_file=alg.def
ori_src_dir=../../../../bmtest/bm1684/test/ce
src_file_list='ce.c ce.h'
backup_dir=backup

cipher_list='des des_ede3 aes sm4'
mode_list='ecb cbc ctr'
hash_list='sha1 sha256'

echo 'generating algorithm definition file ' "$alg_define_file"
rm -rf $alg_define_file
echo ''

for cipher in $cipher_list; do
    for mode in $mode_list; do
        echo -n " >> "
        echo "ALG($mode, $cipher)" | tr [a-z] [A-Z] | tee -a $alg_define_file
    done
done

for hashalg in $hash_list; do
    echo -n " >> "
    echo "HASH($hashalg)" | tr [a-z] [A-Z] | tee -a $alg_define_file
done

echo ''

exit 0

function sync_file()
{
    mkdir -p $backup_dir
    if [ $# -ne 1 ]; then
        echo 'bad usage'
        exit
    fi

    if [ -f $1 ]; then
        touch $1    #update time stamp
        mv $1 $backup_dir/$1.$(sha1sum $1 | awk '{print $1}')
    fi
    cp -v $ori_src_dir/$1 ./
}

echo 'syncing bare metal driver source files'
#sync bare metal driver from bmtest
for file in $src_file_list; do
    sync_file $file
done
