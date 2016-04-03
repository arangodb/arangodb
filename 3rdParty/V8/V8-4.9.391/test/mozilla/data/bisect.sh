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
    cat <<EOF
TEST_DIR=$TEST_DIR"

This script requires the Sisyphus testing framework. Please
cvs check out the Sisyphys framework from mozilla/testing/sisyphus
and set the environment variable TEST_DIR to the directory where it
located.

EOF
    exit 2
fi

source $TEST_DIR/bin/library.sh

usage()
{
    cat <<EOF
usage: bisect.sh -p product -b branch -e extra\\
                   -T  buildtype \\
                   -t test \\
                   -S string \\
                   -G good -B bad \\
                   [-J javascriptoptions]

    variable            description
    ===============     ============================================================
    -p bisect_product     one of js, firefox
    -b bisect_branches    one of supported branches. see library.sh
    -e bisect_extra       optional. extra qualifier to pick build tree and mozconfig.
    -T bisect_buildtype   one of build types opt debug
    -t bisect_test        Test to be bisected.
    -S bisect_string      optional. String containing a regular expression which
                          can be used to distinguish different failures for the given
                          test. The script will search the failure log for the pattern
                          "\$bisect_test.*\$string" to determine if a test failure
                          matches the expected failure.
                          bisect_string can contain any extended regular expressions
                          supported by egrep.
    -G bisect_good        For branches 1.8.0, 1.8.1, 1.9.0, date test passed
                          For branch 1.9.1 and later, revision test passed
    -B bisect_bad         For branches, 1.8.0, 1.8.1, 1.9.0 date test failed
                          For branch 1.9.1 and later, revision test failed.

       If the good revision (test passed) occurred  prior to the bad revision
       (test failed), the script will search for the first bad revision which
       caused the test to regress.

       If the bad revision (test failed) occurred prior to the good revision
       (test passed), the script will search for the first good revision which
       fixed the failing test.

    -D By default, clobber builds will be used. For mercurial based builds
       on branch 1.9.1 and later, you may specify -D to use depends builds.
       This may achieve significant speed advantages by eliminating the need
       to perform clobber builds for each bisection.

    -J javascriptoptions  optional. Set JavaScript options:
         -Z n Set gczeal to n. Currently, only valid for
              debug builds of Gecko 1.8.1.15, 1.9.0 and later.
         -z optional. use split objects in the shell.
         -j optional. use JIT in the shell. Only available on 1.9.1 and later
EOF

    exit 2
}

verbose=0

while getopts "p:b:T:e:t:S:G:B:J:D" optname;
do
    case $optname in
        p) bisect_product=$OPTARG;;
        b) bisect_branch=$OPTARG;;
        T) bisect_buildtype=$OPTARG;;
        e) bisect_extra="$OPTARG"
            bisect_extraflag="-e $OPTARG";;
        t) bisect_test="$OPTARG";;
        S) bisect_string="$OPTARG";;
        G) bisect_good="$OPTARG";;
        B) bisect_bad="$OPTARG";;
        J) javascriptoptions=$OPTARG;;
        D) bisect_depends=1;;
    esac
done

# javascriptoptions will be passed by environment to runtests.sh

if [[ -z "$bisect_product" || -z "$bisect_branch" || -z "$bisect_buildtype" || -z "$bisect_test" || -z "$bisect_good" || -z "$bisect_bad" ]]; then
    echo "bisect_product: $bisect_product, bisect_branch: $bisect_branch, bisect_buildtype: $bisect_buildtype, bisect_test: $bisect_test, bisect_good: $bisect_good, bisect_bad: $bisect_bad"
    usage
fi

OPTIND=1

# evaluate set-build-env.sh in this process to pick up the necessary environment
# variables, but restore the PATH and PYTHON to prevent their interfering with
# Sisyphus.

savepath="$PATH"
savepython="$PYTHON"
eval source $TEST_DIR/bin/set-build-env.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype > /dev/null
PATH=$savepath
PYTHON=$savepython

# TEST_JSDIR must be set after set-build-env.sh is called
# on Windows since TEST_DIR can be modified in set-build-env.sh
# from the pure cygwin path /work/... to a the cygwin windows path
# /c/work/...
TEST_JSDIR=${TEST_JSDIR:-$TEST_DIR/tests/mozilla.org/js}

if [[ "$bisect_branch" == "1.8.0" || "$bisect_branch" == "1.8.1" || \
    "$bisect_branch" == "1.9.0" ]]; then

    #
    # binary search using CVS
    #

    # convert dates to seconds for ordering
    localgood=`dateparse.pl $bisect_good`
    localbad=`dateparse.pl $bisect_bad`

    # if good < bad, then we are searching for a regression,
    # i.e. the first bad changeset
    # if bad < good, then we are searching for a fix.
    # i.e. the first good changeset. so we reverse the nature
    # of good and bad.

    if (( $localgood < $localbad )); then
        cat <<EOF

searching for a regression between $bisect_good and $bisect_bad
the bisection is searching for the transition from test failure not found, to test failure found.

EOF

        searchtype="regression"
        bisect_start=$bisect_good
        bisect_stop=$bisect_bad
    else
        cat <<EOF

searching for a fix between $bisect_bad and $bisect_good
the bisection is searching for the transition from test failure found to test failure not found.

EOF

        searchtype="fix"
        bisect_start=$bisect_bad
        bisect_stop=$bisect_good
    fi

    let seconds_start="`dateparse.pl $bisect_start`"
    let seconds_stop="`dateparse.pl $bisect_stop`"

    echo "checking that the test fails in the bad revision $bisect_bad"
    eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "clobber" > /dev/null
    export MOZ_CO_DATE="$bisect_bad"
    eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "checkout" > /dev/null
    bisect_log=`eval $TEST_JSDIR/runtests.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -I $bisect_test -B "build" -c -t -X /dev/null 2>&1 | grep '_js.log $' | sed 's|log: \([^ ]*\) |\1|'`
    if [[ -z "$bisect_log" ]]; then
        echo "test $bisect_test not run."
    else
        if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
            echo "test failure $bisect_test.*$bisect_string found, bad revision $bisect_bad confirmed"
            bad_confirmed=1
        else
            echo "test failure $bisect_test.*$bisect_string not found, bad revision $bisect_bad *not* confirmed"
        fi
    fi

    if [[ "$bad_confirmed" != "1" ]]; then
        error "bad revision not confirmed";
    fi

    echo "checking that the test passes in the good revision $bisect_good"
    eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "clobber" > /dev/null
    export MOZ_CO_DATE="$bisect_good"
    eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "checkout" > /dev/null

    bisect_log=`eval $TEST_JSDIR/runtests.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -I $bisect_test -B "build" -c -t -X /dev/null 2>&1 | grep '_js.log $' | sed 's|log: \([^ ]*\) |\1|'`
    if [[ -z "$bisect_log" ]]; then
        echo "test $bisect_test not run."
    else
        if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
            echo "test failure $bisect_test.*$bisect_string found, good revision $bisect_good *not* confirmed"
        else
            echo "test failure $bisect_test.*$bisect_string not found, good revision $bisect_good confirmed"
            good_confirmed=1
        fi
    fi

    if [[ "$good_confirmed" != "1" ]]; then
        error "good revision not confirmed";
    fi

    echo "bisecting $bisect_start to $bisect_stop"

    #
    # place an array of dates of checkins into an array and
    # perform a binary search on those dates.
    #
    declare -a seconds_array date_array

    # load the cvs checkin dates into an array. the array will look like
    # date_array[i] date value
    # date_array[i+1] time value

    pushd $BUILDTREE/mozilla
    date_array=(`cvs -q -z3 log -N -d "$bisect_start<$bisect_stop" | grep "^date: " | sed 's|^date: \([^;]*\).*|\1|' | sort -u`)
    popd

    let seconds_index=0 1
    let date_index=0 1

    while (( $date_index < ${#date_array[@]} )); do
        seconds_array[$seconds_index]=`dateparse.pl "${date_array[$date_index]} ${date_array[$date_index+1]} UTC"`
        let seconds_index=$seconds_index+1
        let date_index=$date_index+2
    done

    let seconds_index_start=0 1
    let seconds_index_stop=${#seconds_array[@]}

    while true; do

        if (( $seconds_index_start+1 >= $seconds_index_stop )); then
            echo "*** date `perl -e 'print scalar localtime $ARGV[0];' ${seconds_array[$seconds_index_stop]}` found ***"
            break;
        fi

        unset result

        # clobber before setting new changeset.
        eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "clobber" > /dev/null

        let seconds_index_middle="($seconds_index_start + $seconds_index_stop)/2"
        let seconds_middle="${seconds_array[$seconds_index_middle]}"

        bisect_middle="`perl -e 'print scalar localtime $ARGV[0];' $seconds_middle`"
        export MOZ_CO_DATE="$bisect_middle"
        echo "testing $MOZ_CO_DATE"

        eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "checkout" > /dev/null

        bisect_log=`eval $TEST_JSDIR/runtests.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -I $bisect_test -B "build" -c -t -X /dev/null 2>&1 | grep '_js.log $' | sed 's|log: \([^ ]*\) |\1|'`
        if [[ -z "$bisect_log" ]]; then
            echo "test $bisect_test not run. Skipping changeset"
            let seconds_index_start=$seconds_index_start+1
        else
            if [[ "$searchtype" == "regression" ]]; then
                # searching for a regression, pass -> fail
                if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
                    echo "test failure $bisect_test.*$bisect_string found"
                    let seconds_index_stop=$seconds_index_middle;
                else
                    echo "test failure $bisect_test.*$bisect_string not found"
                    let seconds_index_start=$seconds_index_middle
                fi
            else
                # searching for a fix, fail -> pass
                if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
                    echo "test failure $bisect_test.*$bisect_string found"
                    let seconds_index_start=$seconds_index_middle
                else
                    echo "test failure $bisect_test.*$bisect_string not found"
                    let seconds_index_stop=$seconds_index_middle
                fi
            fi
        fi

    done
else
    #
    # binary search using mercurial
    #

    TEST_MOZILLA_HG_LOCAL=${TEST_MOZILLA_HG_LOCAL:-$BUILDDIR/hg.mozilla.org/`basename $TEST_MOZILLA_HG`}
    hg -R $TEST_MOZILLA_HG_LOCAL pull -r tip

    REPO=$BUILDTREE/mozilla
    hg -R $REPO pull -r tip

    # convert revision numbers to local revision numbers for ordering
    localgood=`hg -R $REPO id -n -r $bisect_good`
    localbad=`hg -R $REPO id -n -r $bisect_bad`

    # if good < bad, then we are searching for a regression,
    # i.e. the first bad changeset
    # if bad < good, then we are searching for a fix.
    # i.e. the first good changeset. so we reverse the nature
    # of good and bad.

    if (( $localgood < $localbad )); then
        cat <<EOF

searching for a regression between $localgood:$bisect_good and $localbad:$bisect_bad
the result is considered good when the test result does not appear in the failure log, bad otherwise.
the bisection is searching for the transition from test failure not found, to test failure found.

EOF

        searchtype="regression"
        bisect_start=$bisect_good
        bisect_stop=$bisect_bad
    else
        cat <<EOF

searching for a fix between $localbad:$bisect_bad and $localgood:$bisect_good
the result is considered good when the test result does appear in the failure log, bad otherwise.
the bisection is searching for the transition from test failure found to test failure not found.

EOF

        searchtype="fix"
        bisect_start=$bisect_bad
        bisect_stop=$bisect_good
    fi

    echo "checking that the test fails in the bad revision $localbad:$bisect_bad"
    if [[ -z "$bisect_depends" ]]; then
        eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "clobber" > /dev/null
    fi
    hg -R $REPO update -C -r $bisect_bad
    bisect_log=`eval $TEST_JSDIR/runtests.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -I $bisect_test -B "build" -c -t -X /dev/null 2>&1 | grep '_js.log $' | sed 's|log: \([^ ]*\) |\1|'`
    if [[ -z "$bisect_log" ]]; then
        echo "test $bisect_test not run."
    else
        if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
            echo "test failure $bisect_test.*$bisect_string found, bad revision $localbad:$bisect_bad confirmed"
            bad_confirmed=1
        else
            echo "test failure $bisect_test.*$bisect_string not found, bad revision $localbad:$bisect_bad *not* confirmed"
        fi
    fi

    if [[ "$bad_confirmed" != "1" ]]; then
        error "bad revision not confirmed";
    fi

    echo "checking that the test passes in the good revision $localgood:$bisect_good"
    if [[ -z "$bisect_depends" ]]; then
        eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "clobber" > /dev/null
    fi
    hg -R $REPO update -C -r $bisect_good
    bisect_log=`eval $TEST_JSDIR/runtests.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -I $bisect_test -B "build" -c -t -X /dev/null 2>&1 | grep '_js.log $' | sed 's|log: \([^ ]*\) |\1|'`
    if [[ -z "$bisect_log" ]]; then
        echo "test $bisect_test not run."
    else
        if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
            echo "test failure $bisect_test.*$bisect_string found, good revision $localgood:$bisect_good *not* confirmed"
        else
            echo "test failure $bisect_test.*$bisect_string not found, good revision $localgood:$bisect_good confirmed"
            good_confirmed=1
        fi
    fi

    if [[ "$good_confirmed" != "1" ]]; then
        error "good revision not confirmed";
    fi

    echo "bisecting $REPO $bisect_start to $bisect_stop"
    hg -q -R $REPO bisect --reset
    hg -q -R $REPO bisect --good $bisect_start
    hg -q -R $REPO bisect --bad $bisect_stop

    while true; do
        unset result

        # clobber before setting new changeset.
        if [[ -z "$bisect_depends" ]]; then
            eval $TEST_DIR/bin/builder.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -B "clobber" > /dev/null
        fi

        # remove extraneous in-tree changes
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=480680 for details
        # of why this is necessary.
        hg -R $REPO update -C

        hg -R $REPO bisect
        # save the revision id so we can update to it discarding local
        # changes in the working directory.
        rev=`hg -R $REPO id -i`

        bisect_log=`eval $TEST_JSDIR/runtests.sh -p $bisect_product -b $bisect_branch $bisect_extraflag -T $bisect_buildtype -I $bisect_test -B "build" -c -t -X /dev/null 2>&1 | grep '_js.log $' | sed 's|log: \([^ ]*\) |\1|'`
        # remove extraneous in-tree changes
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=480680 for details
        # of why this is necessary.
        hg -R $REPO update -C -r $rev

        if [[ -z "$bisect_log" ]]; then
            echo "test $bisect_test not run. Skipping changeset"
            hg -R $REPO bisect --skip
        else
            if [[ "$searchtype" == "regression" ]]; then
                # searching for a regression
                # the result is considered good when the test does not appear in the failure log
                if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
                    echo "test failure $bisect_test.*$bisect_string found, marking revision bad"
                    if ! result=`hg -R $REPO bisect --bad 2>&1`; then
                        echo "bisect bad failed"
                        error "$result"
                    fi
                else
                    echo "test failure $bisect_test.*$bisect_string not found, marking revision good"
                    if ! result=`hg -R $REPO bisect --good 2>&1`; then
                        echo "bisect good failed"
                        error "$result"
                    fi
                fi
            else
                # searching for a fix
                # the result is considered good when the test does appear in the failure log
                if egrep -q "$bisect_test.*$bisect_string" ${bisect_log}-results-failures.log; then
                    echo "test failure $bisect_test.*$bisect_string found, marking revision good"
                    if ! result=`hg -R $REPO bisect --good 2>&1`; then
                        echo "bisect good failed"
                        error "$result"
                    fi
                else
                    echo "test failure $bisect_test.*$bisect_string not found, marking revision bad"
                    if ! result=`hg -R $REPO bisect --bad 2>&1`; then
                        echo "bisect bad failed"
                        error "$result"
                    fi
                fi
            fi
        fi

        if echo $result | egrep -q "The first (good|bad) revision is:"; then
            result_revision=`echo $result | sed "s|The first .* revision is:.*changeset: [0-9]*:\([^ ]*\).*|\1|"`
            echo $result | sed "s|The first .* revision is:|$searchtype|"
            echo "*** revision $result_revision found ***"
            break
        fi

    done
fi
