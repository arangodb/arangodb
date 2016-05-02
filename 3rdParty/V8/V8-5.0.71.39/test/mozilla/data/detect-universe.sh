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

if [[ -z "$TEST_DIR" ]]; then
    cat <<EOF
`basename $0`: error

TEST_DIR, the location of the Sisyphus framework,
is required to be set prior to calling this script.
EOF
    exit 2
fi

if [[ ! -e $TEST_DIR/bin/library.sh ]]; then
    echo "TEST_DIR=$TEST_DIR"
    echo ""
    echo "This script requires the Sisyphus testing framework. Please "
    echo "cvs check out the Sisyphys framework from mozilla/testing/sisyphus"
    echo "and set the environment variable TEST_DIR to the directory where it"
    echo "located."
    echo ""

    exit 2
fi

#
# options processing
#
usage()
{
    cat <<EOF
usage: detect-universe.sh -p products -b branches -R repositories -T buildtypes

Outputs to stdout the universe data for this machine.

variable            description
===============     ============================================================
-p products         required. one or more of firefox, thunderbird, fennec, js
-b branches         required. one or more of supported branches. set library.sh
-R repositories     required. one or more of CVS, mozilla-central, ...
-T buildtype        required. one or more of opt debug

if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

while getopts "p:b:R:T:" optname
do
    case $optname in
        p)
            products=$OPTARG;;
        b)
            branches=$OPTARG;;
        R)
            repos=$OPTARG;;
        T)
            buildtypes=$OPTARG;;
    esac
done

if [[ -z "$products" || -z "$branches" || -z "$buildtypes" ]]; then
    usage
fi

source $TEST_DIR/bin/library.sh

(for product in $products; do
    for branch in $branches; do
        for repo in $repos; do

            if [[ ("$branch" != "1.8.0" && "$branch" != "1.8.1" && "$branch" != "1.9.0") && $repo == "CVS" ]]; then
                continue;
            fi

            if [[ ("$branch" == "1.8.0" || "$branch" == "1.8.1" || "$branch" == "1.9.0") && $repo != "CVS" ]]; then
                continue
            fi

            for buildtype in $buildtypes; do
                if [[ $product == "js" ]]; then
                    testtype=shell
                else
                    testtype=browser
                fi
                echo "TEST_OS=$OSID, TEST_KERNEL=$TEST_KERNEL, TEST_PROCESSORTYPE=$TEST_PROCESSORTYPE, TEST_MEMORY=$TEST_MEMORY, TEST_TIMEZONE=$TEST_TIMEZONE, TEST_BRANCH=$branch, TEST_REPO=$repo, TEST_BUILDTYPE=$buildtype, TEST_TYPE=$testtype"
            done
        done
    done
done) | sort -u
