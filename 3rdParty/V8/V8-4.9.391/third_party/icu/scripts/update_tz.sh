#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Download the 4 files below from the ICU data repository ($baseurl below) and
# put them in source/data/misc to update the IANA timezone database in ICU.
#
#   metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
#
# For IANA Time zone database, see https://www.iana.org/time-zones

if [ $# -lt 1 ];
then
  echo "Usage: $0 version (e.g. '2015b')"
  exit 1
fi

version=$1
baseurl="http://source.icu-project.org/repos/icu/data/trunk/tzdata/icunew/"
outputdir="$(dirname "$0")/../source/data/misc"

# The latest ICU version for which the timezone data format changed in
# an incompatible manner.
# For a given IANA tz db version (e.g. 2015b),
#  http://source.icu-project.org/repos/icu/data/trunk/tzdata/icunew/${version}
# has subdirectories for different ICU data versions. As of April 2015, 44
# is the latest even though the latest ICU release is 55.
icudataversion=44

sourcedirurl="${baseurl}/${version}/${icudataversion}"

for f in metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
do
  wget -O "${outputdir}/${f}" "${sourcedirurl}/${f}"
done
