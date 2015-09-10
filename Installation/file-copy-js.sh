#!/bin/bash
set -e

SRCDIR=$1
DSTDIR=$2

${SRCDIR}/Installation/file-list-js.sh ${SRCDIR} | while read a; do
  if test -f "$DSTDIR/$a";  then
    echo "$DSTDIR/$a: already exists, giving up"
    exit 1
  fi

  FROM="${SRCDIR}/$a"
  TO="${DSTDIR}/$a"
  DIR=`dirname "${TO}"`
  
  chmod a+r "${FROM}"
  test -d "${DIR}" || mkdir -p "${DIR}"
  cp -n "${FROM}" "${TO}"
done
