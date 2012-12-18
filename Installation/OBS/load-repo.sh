#!/bin/bash

if test "$1" == "";  then
  echo "usage: $0 <obs-server>"
  exit 1
fi

wget -r -np "http://$1/repositories/home:/fceller/"
mv "$1" download
