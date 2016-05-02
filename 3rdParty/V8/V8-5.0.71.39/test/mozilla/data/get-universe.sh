#!/bin/bash -e
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
# Portions created by the Initial Developer are Copyright (C) 2007
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

# usage: get-universe.sh logfile(s) > universe.data
#
# get-universe.sh reads the processed javascript logs and writes to
# stdout the unique set of fields to be used as the "universe" of test
# run data. These values are used by pattern-expander.pl and
# pattern-extracter.pl to encode the known failure files into regular
# expressions.

export LC_ALL=C # handle all character sets

(for f in $@; do
    grep -h -m 1 TEST_ID $f | tr -dc '[\040-\177\n]' | sed 's|^TEST_ID=[^,]*, \(TEST_BRANCH=[^,]*, TEST_REPO=[^,]*, TEST_BUILDTYPE=[^,]*, TEST_TYPE=[^,]*\), \(TEST_OS=[^,]*, TEST_KERNEL=[^,]*, TEST_PROCESSORTYPE=[^,]*, TEST_MEMORY=[^,]*, TEST_TIMEZONE=[^,]*, TEST_OPTIONS=[^,]*\),.*|\2, \1|' 
done) | sort -u
