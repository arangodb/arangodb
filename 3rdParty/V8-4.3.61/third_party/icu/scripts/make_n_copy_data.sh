#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

TOPSRC=$(egrep '^top_srcdir =' Makefile | cut -d ' ' -f 3)
echo $TOPSRC

cd data
make

# A work-around for http://bugs.icu-project.org/trac/ticket/10570
sed -i 's/css3transform.res/root.res/' out/tmp/icudata.lst
make

if [ $? ]; then
  echo "Failed to make data";
  exit 1;
fi

cp out/tmp/icudt[5-9][0-9]l.dat "${TOPSRC}/data/in/icudtl.dat"
cp out/tmp/icudt[5-9][0-9]l_dat.S "${TOPSRC}/../linux/icudtl_dat.S"

cd "${TOPSRC}/../scripts"
sh make_mac_assembly.sh
