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

source $TEST_DIR/bin/library.sh

TEST_JSDIR=${TEST_JSDIR:-$TEST_DIR/tests/mozilla.org/js}

usage()
{
    cat <<EOF
usage: runtests.sh -p products -b branches -e extra\\
                   -T  buildtypes -B buildcommands  \\
                   [-v] [-S] [-X excludetests] [-I includetests] [-c] [-t] \\
                   [-J javascriptoptions]

variable            description
===============     ============================================================
-p products         space separated list of js, firefox, fennec
-b branches         space separated list of supported branches. see library.sh
-e extra            optional. extra qualifier to pick build tree and mozconfig.
-T buildtypes       space separated list of build types opt debug
-B buildcommands    optional space separated list of build commands
                    clean, checkout, build. If not specified, defaults to
                    'clean checkout build'.
                    If you wish to skip any build steps, simply specify a value
                    not containing any of the build commands, e.g. 'none'.
-v                  optional. verbose - copies log file output to stdout.
-S                  optional. summary - output tailered for use with
                    Buildbot|Tinderbox
-X excludetests     optional. By default the test will exclude the
                    tests listed in spidermonkey-n-\$branch.tests,
                    performance-\$branch.tests. excludetests is a list of either
                    individual tests, manifest files or sub-directories which
                    will override the default exclusion list.
-I includetests     optional. By default the test will include the
                    JavaScript tests appropriate for the branch. includetests is a
                    list of either individual tests, manifest files or
                    sub-directories which will override the default inclusion
                    list.
-c                  optional. By default the test will exclude tests
                    which crash on this branch, test type, build type and
                    operating system. -c will include tests which crash.
                    Typically this should only be used in combination with -R.
                    This has no effect on shell based tests which execute crash
                    tests regardless.
-t                  optional. By default the test will exclude tests
                    which time out on this branch, test type, build type and
                    operating system. -t will include tests which timeout.
-J jsoptions        optional. Set JavaScript options:
                    -Z n Set gczeal to n. Currently, only valid for
                    debug builds of Gecko 1.8.1.15, 1.9.0 and later.
                    -z optional. use split objects in the shell.
                    -j optional. use JIT in the shell. Only available on 1.9.1 and later

if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

verbose=0

while getopts "p:b:T:B:e:X:I:J:vSct" optname;
do
    case $optname in
        p) products=$OPTARG;;
        b) branches=$OPTARG;;
        T) buildtypes=$OPTARG;;
        e) extra="$OPTARG"
            extraflag="-e $OPTARG";;
        B) buildcommands=$OPTARG;;
        v) verbose=1
            verboseflag="-v";;
        S) summary=1;;
        X) excludetests=$OPTARG;;
        I) includetests=$OPTARG;;
        J) javascriptoptions=$OPTARG;;
        c) crashes=1;;
        t) timeouts=1;;
    esac
done

 # javascriptoptions will be passed by environment to test.sh

if [[ -z "$products" || -z "$branches" || -z "$buildtypes" ]]; then
    usage
fi

if [[ -z "$buildcommands" ]]; then
    buildcommands="clean checkout build"
fi

case $buildtypes in
    nightly)
        buildtypes="nightly-$OSID"
        ;;
    opt|debug|opt*debug)
        if [[ -n "$buildcommands" ]]; then
            builder.sh -p "$products" -b "$branches" $extraflag -B "$buildcommands" -T "$buildtypes" "$verboseflag"
        fi
        ;;
esac

testlogfilelist=`mktemp /tmp/TESTLOGFILES.XXXX`
trap "_exit; rm -f $testlogfilelist" EXIT

export testlogfiles
export testlogfile

# because without pipefail, the pipe will not return a non-zero
# exit code, we must pipe stderr from tester.sh to stdout and then
# look into the testlogfilelist for the error

branchesextra=`combo.sh -d - "$branches" "$extra"`

# can't test tester.sh's exit code to see if there was
# an error since we are piping it and can't count on pipefail
tester.sh -t "$TEST_JSDIR/test.sh" $verboseflag "$products" "$branchesextra" "$buildtypes" 2>&1 | tee -a $testlogfilelist
testlogfiles="`grep '^log:' $testlogfilelist|sed 's|^log: ||'`"

fatalerrors=`grep 'FATAL ERROR' $testlogfiles | cat`
if [[ -n "$fatalerrors" ]]; then
    testlogarray=( $testlogfiles )
    let itestlog=${#testlogarray[*]}-1
    error "`tail -n 20 ${testlogarray[$itestlog]}`" $LINENO
fi

for testlogfile in $testlogfiles; do

    if [[ -n "$DEBUG" ]]; then
        dumpvars testlogfile
    fi

    case "$testlogfile" in
        *,js,*) testtype=shell;;
        *,firefox,*) testtype=browser;;
        *,thunderbird,*) testtype=browser;;
        *,fennec,*) testtype=browser;;
        *) error "unknown testtype in logfile $testlogfile" $LINENO;;
    esac

    case "$testlogfile" in
        *,opt,*) buildtype=opt;;
        *,debug,*) buildtype=debug;;
        *,nightly*) buildtype=opt;;
        *) error "unknown buildtype in logfile $testlogfile" $LINENO;;
    esac

    branch=`echo $testlogfile | sed 's|.*,\([0-9]\.[0-9]*\.[0-9]*\).*|\1|'`


    repo=`grep -m 1 '^environment: TEST_MOZILLA_HG=' $testlogfile | sed 's|.*TEST_MOZILLA_HG=http://hg.mozilla.org.*/\([^\/]*\)|\1|'`
    if [[ -z "$repo" ]]; then
        repo=CVS
    fi
    debug "repo=$repo"

    outputprefix=$testlogfile

    if [[ -n "$DEBUG" ]]; then
        dumpvars branch buildtype testtype OSID testlogfile TEST_PROCESSORTYPE TEST_KERNEL outputprefix
    fi

    if ! $TEST_DIR/tests/mozilla.org/js/known-failures.pl \
        -b $branch \
        -T $buildtype \
        -R $repo \
        -t $testtype \
        -J "$javascriptoptions" \
        -o "$OSID" \
        -K "$TEST_KERNEL" \
        -A "$TEST_PROCESSORTYPE" \
        -M "$TEST_MEMORY" \
        -z `date +%z` \
        -l $testlogfile \
        -r $TEST_JSDIR/failures.txt \
        -O $outputprefix; then
        error "known-failures.pl" $LINENO
    fi

    if [[ -n "$summary" ]]; then

        # use let to work around mac problem where numbers were
        # output with leading characters.
        # if let's arg evaluates to 0, let will return 1
        # so we need to test

        if let npass="`grep TEST_RESULT=PASSED ${outputprefix}-results-all.log | wc -l`"; then true; fi
        if let nfail="`cat ${outputprefix}-results-failures.log | wc -l`"; then true; fi
        if let nfixes="`cat ${outputprefix}-results-possible-fixes.log | wc -l`"; then true; fi
        if let nregressions="`cat ${outputprefix}-results-possible-regressions.log | wc -l`"; then true; fi

        echo -e "\nJavaScript Tests $branch $buildtype $testtype\n"
        echo -e "\nFailures:\n"
        cat "${outputprefix}-results-failures.log"
        echo -e "\nPossible Fixes:\n"
        cat "${outputprefix}-results-possible-fixes.log"
        echo -e "\nPossible Regressions:\n"
        cat "${outputprefix}-results-possible-regressions.log"
        echo -e "\nTinderboxPrint:<div title=\"$testlogfile\">\n"
        echo -e "\nTinderboxPrint:js tests<br/>$branch $buildtype $testtype<br/>$npass/$nfail<br/>F:$nfixes R:$nregressions"
        echo -e "\nTinderboxPrint:</div>\n"
    fi

done
