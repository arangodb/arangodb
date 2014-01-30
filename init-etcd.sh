#!/bin/sh
ETCD=$HOME/etcd/bin/etcd
killall etcd
cd /tmp
rm -f etcd.log
rm -Rf machine1 machine2 machine3
mkdir -p machine1 machine2 machine3
$ETCD -data-dir machine1 -name machine1 -addr=127.0.0.1:4001 -peer-addr=127.0.0.1:7001 >> /tmp/etcd.log &
sleep 1
$ETCD -data-dir machine2 -name machine2 -addr=127.0.0.1:4002 -peer-addr=127.0.0.1:7002 -peers=127.0.0.1:7001 >> /tmp/etcd.log &
$ETCD -data-dir machine3 -name machine3 -addr=127.0.0.1:4003 -peer-addr=127.0.0.1:7003 -peers=127.0.0.1:7001 >> /tmp/etcd.log &

