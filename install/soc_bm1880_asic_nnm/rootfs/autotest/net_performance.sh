#!/bin/bash

#$1 is target ip address
#$2 is compare value of throughput
#ex. ./net_performance.sh 192.168.0.11 900

if [ ! -e /sys/kernel/debug/stmmaceth/eth0 ];then
  ifconfig eth0 192.168.0.3 up
  sleep 6
else
  echo 'network exist'
#  sleep 6
fi

export SUM=0
for i in {1..2}
do
  T=$(netperf -c -C -H $1 -t TCP_MAERTS | sed '1,6d'| awk '{print int($5)}')
  echo 'Round'$i' Throughput= '$T'bps'
  let SUM=$SUM+$T
done

  let AVG=$SUM/2
  awk 'BEGIN{print "avg throuphout= '$AVG'bps";if (int("'$AVG'") > int("'$2'")) {print "'$0': passed"} \
  else {print "'$0': failed"}}'
