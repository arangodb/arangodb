#!/bin/bash

TIMEOUT=3600s

PORTDIR=/var/tmp/ports
mkdir -p $PORTDIR

INCR=40

port=30000

find $PORTDIR -type f -ctime +$TIMEOUT -exec rm "{}" ";"

while ! ((set -o noclobber ; date > $PORTDIR/$port) 2> /dev/null); do
    sleep 1
    port=`expr $port + $INCR`
done

echo "using port range $port - `expr $port + $INCR - 1`"
echo $port > PORTFILE
