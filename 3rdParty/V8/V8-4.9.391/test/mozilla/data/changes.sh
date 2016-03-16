#!/bin/bash
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla JavaScript Testing Utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bclary@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# usage: changes.sh [prefix]
# 
# combines the {prefix}*possible-fixes.log files into {prefix}possible-fixes.log
# and {prefix}*possible-regressions.log files into 
# possible-regressions.log.
#
# This script is useful in cases where log files from different machines, branches
# and builds are being investigated.

export LC_ALL=C

if cat /dev/null | sed -r 'q' > /dev/null 2>&1; then
   SED="sed -r"
elif cat /dev/null | sed -E 'q' > /dev/null 2>&1; then
   SED="sed -E"
else
   echo "Neither sed -r or sed -E is supported"
   exit 2
fi

workfile=`mktemp work.XXXXXXXX`
if [ $? -ne 0 ]; then
    echo "Unable to create working temp file"
    exit 2
fi

for f in ${1}*results-possible-fixes.log*; do
    case $f in
	*.log)
	    CAT=cat
	    ;;
	*.log.bz2)
	    CAT=bzcat
	    ;;
	*.log.gz)
	    CAT=zcat
	    ;;
	*.log.zip)
	    CAT="unzip -c"
	    ;;
	*)
	    echo "unknown log type: $f"
	    exit 2
	    ;;
    esac

    $CAT $f | $SED "s|$|:$f|" >> $workfile

done

sort -u $workfile > ${1}possible-fixes.log

rm $workfile


for f in ${1}*results-possible-regressions.log*; do
    case $f in
	*.log)
	    CAT=cat
	    ;;
	*.log.bz2)
	    CAT=bzcat
	    ;;
	*.log.gz)
	    CAT=zcat
	    ;;
	*.log.zip)
	    CAT="unzip -c"
	    ;;
	*)
	    echo "unknown log type: $f"
	    exit 2
	    ;;
    esac
    $CAT $f >> $workfile
done

sort -u $workfile > ${1}possible-regressions.log

rm $workfile



