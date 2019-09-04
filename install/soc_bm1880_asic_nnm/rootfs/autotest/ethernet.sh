#!/bin/bash

ERROR=0

bm_error_check() {
#  echo "test result = $?"
  if [ $? == 0 ]; then
    echo "$1 passed"
  else
    ERROR=$[ $ERROR | 0x1 ]
    echo "$1 failed ERROR=$ERROR"
  fi
}

if [ ! -e /sys/kernel/debug/stmmaceth/eth0 ];then
  ifconfig eth0 192.168.0.3 up
  sleep 10
else
  echo 'network exist'
  sleep 10
fi

#ping 8.8.8.8 -c 1 | grep -o '0% packet loss'
ping $1 -c 1
bm_error_check "$0:"
