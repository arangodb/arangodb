#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Download the 4 files below from the ICU trunk and put them in
# source/data/misc to update the IANA timezone database.
#
#   metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
#
# For IANA Time zone database, see https://www.iana.org/time-zones

datapath="source/data/misc"
sourcedirurl="https://github.com/unicode-org/icu/trunk/icu4c/${datapath}"
cd "$(dirname "$0")/../${datapath}"

for f in metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
do
  echo "${sourcedirurl}/${f}"
  svn --force export "${sourcedirurl}/${f}"
done
