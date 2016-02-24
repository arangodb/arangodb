#!/bin/bash

IN="$1"
OUT="$2"
VERSION="$3"

section=`echo $IN | sed -e 's:.*\([0-9]\):\1:'`
command=`echo $IN | sed -e 's:.*/\([^\.]*\).[0-9]:\1:'`

sed \
  -f `dirname $0`/../Documentation/Scripts/man.sed \
  -e "s/<SECTION>/$section/g" \
  -e "s/<COMMAND>/$command/g" \
  -e "s/<DATE>/`date "+%d %b %Y"`/g" \
  -e "s/<VERSION>/$VERSION/g" \
  < $IN > $OUT

