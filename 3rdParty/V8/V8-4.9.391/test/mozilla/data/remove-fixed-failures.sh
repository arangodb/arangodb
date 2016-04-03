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

if [[ ! -e "$1" || ! -e "$2" ]]; then
    cat	<<EOF
Usage: remove-fixed-failures.sh possible-fixes.log failures.log

possible-fixes.log contains the possible fixes from the most recent
test run.

failures.log contains the current known failures.

remove-fixed-failures.sh removes each pattern in possible-fixes.log
from failures.log.

The original failures.log is saved as failures.log.orig for safe keeping.

EOF
    exit 1
fi

fixes="$1"
failures="$2"

# save the original failures file in case of an error
cp $failures $failures.orig

# create a temporary file to contain the current list 
# of failures.
workfailures=`mktemp working-failures.XXXXX`
workfixes=`mktemp working-fixes.XXXXX`

trap "rm -f ${workfailures} ${workfailures}.temp ${workfixes}*;" EXIT

# create working copy of the failures file
cp $failures $workfailures
cp $fixes $workfixes

sed -i.bak 's|:[^:]*\.log||' $workfixes;

grep -Fv -f $workfixes ${workfailures} > ${workfailures}.temp

mv $workfailures.temp $workfailures

mv $workfailures $failures

