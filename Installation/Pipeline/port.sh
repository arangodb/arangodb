#!/bin/bash

TIMEOUT=3600s

PORTDIR=/var/tmp/ports
mkdir -p $PORTDIR

if test "$1" == "--clean"; then
    shift

    while test $# -gt 0; do
        echo "freeing port $1"
        rm -f $PORTDIR/$1
        shift
    done

    exit
fi

INCR=100
port=30000

find $PORTDIR -type f -ctime +$TIMEOUT -exec rm "{}" ";"

while ! ((set -o noclobber ; date > $PORTDIR/$port) 2> /dev/null); do
    sleep 1
    port=`expr $port + $INCR`
done

echo $port
