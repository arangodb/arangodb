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

TEST_JSSHELL_TIMEOUT=${TEST_JSSHELL_TIMEOUT:-480}
TEST_JSEACH_TIMEOUT=${TEST_JSEACH_TIMEOUT:-485}
TEST_JSEACH_PAGE_TIMEOUT=${TEST_JSEACH_PAGE_TIMEOUT:-480}
TEST_JSALL_TIMEOUT=${TEST_JSALL_TIMEOUT:-21600}
TEST_WWW_JS=`echo $TEST_JSDIR|sed "s|$TEST_DIR||"`

#
# options processing
#
usage()
{
    cat <<EOF
usage: test.sh -p product -b branch -T buildtype -x executablepath -N profilename \\
               [-X excludetests] [-I includetests] [-c] [-t] [-F] [-d datafiles]

variable            description
===============     ============================================================
-p product          required. firefox|thunderbird|js|fennec
-b branch           required. one of supported branches. see library.sh.
-s jsshellsourcepath       required for shell. path to js shell source directory mozilla/js/src
-T buildtype        required. one of opt debug
-x executablepath   required for browser. directory-tree containing executable 'product'
-N profilename      required for browser. profile name
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
-F                  optional. Just generate file lists without running any tests.
-d datafiles        optional. one or more filenames of files containing
                    environment variable definitions to be included.

                    note that the environment variables should have the same
                    names as in the "variable" column.

if an argument contains more than one value, it must be quoted.
EOF
    exit 2
}

while getopts "p:b:s:T:x:N:d:X:I:J:RctF" optname
do
    case $optname in
        p)
            product=$OPTARG;;
        b)
            branch=$OPTARG;;
        T)
            buildtype=$OPTARG;;
        s)
            jsshellsourcepath=$OPTARG;;
        N)
            profilename=$OPTARG;;
        x)
            executablepath=$OPTARG;;
        X)
            excludetests=$OPTARG;;
        I)
            includetests=$OPTARG;;
        c)
            crashes=1;;
        t)
            timeouts=1;;
        F)
            filesonly=1;;
        J)
            javascriptoptions=$OPTARG
            ;;
        d) datafiles=$OPTARG;;
    esac
done

if [[ -n "$javascriptoptions" ]]; then
    unset OPTIND
    while getopts "Z:jz" optname $javascriptoptions; do
        case $optname in
            Z)
                gczealshell="-Z $OPTARG"
                gczealbrowser=";gczeal=$OPTARG"
                ;;
            z)
                splitobjects="-z"
                ;;
            j)
                jitshell="-j"
                jitbrowser=";jit"
                ;;
        esac
    done
fi

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do
        source $datafile
    done
fi

if [[ -n "$gczeal" && "$buildtype" != "debug" ]]; then
    error "gczeal is supported for buildtype debug and not $buildtype"
fi

dumpvars product branch buildtype jsshellsourcepath profilename executablepath excludetests includetests crashes timeouts filesonly javascriptoptions datafiles | sed "s|^|arguments: |"

pushd $TEST_JSDIR

case $product in
    js)
        if [[ -z "$branch" || -z "$buildtype"  || -z "$jsshellsourcepath" ]]; then
            usage
        fi
        source config.sh
        testtype=shell
        executable="$jsshellsourcepath/$JS_OBJDIR/js$EXE_EXT"
        ;;

    firefox|thunderbird|fennec)
        if [[ -z "$branch" || -z "$buildtype"  || -z "$executablepath" || -z "$profilename" ]]; then
            usage
        fi
        testtype=browser
        executable=`get_executable $product $branch $executablepath`

        ;;
    *)
        echo "Unknown product: $product"
        usage
        ;;
esac

function shellfileversion()
{
    local jsfile=$1
    local version

    case $jsfile in
        ecma/*)
            version=150;;
        ecma_2/*)
            version=150;;
        ecma_3/*)
            version=150;;
        ecma_3_1/*)
            version=180;;
        js1_1/*)
            version=150;;
        js1_2/*)
            version=150;;
        js1_3/*)
            version=150;;
        js1_4/*)
            version=150;;
        js1_5/*)
            version=150;;
        js1_6/*)
            version=160;;
        js1_7/*)
            version=170;;
        js1_8/*)
            version=180;;
        js1_8_1/*)
            version=180;;
        js1_9/*)
            version=190;;
        js2_0/*)
            version=200;;
        *)
            version=150;;
    esac
    echo $version
}

function browserfileversion()
{
    local jsfile=$1
    local version

    case $jsfile in
        ecma/*)
            version=1.5;;
        ecma_2/*)
            version=1.5;;
        ecma_3/*)
            version=1.5;;
        ecma_3_1/*)
            version=1.8;;
        js1_1/*)
            version=1.1;;
        js1_2/*)
            version=1.5;;
        js1_3/*)
            version=1.5;;
        js1_4/*)
            version=1.5;;
        js1_5/*)
            version=1.5;;
        js1_6/*)
            version=1.6;;
        js1_7/*)
            version=1.7;;
        js1_8/*)
            version=1.8;;
        js1_8_1/*)
            version=1.8;;
        js1_9/*)
            version=1.9;;
        js2_0/*)
            version=2.0;;
        *)
            version=1.5;;
    esac
    echo $version
}

rm -f finished-$branch-$testtype-$buildtype

if ! make failures.txt; then
    error "during make failures.txt" $LINENO
fi

includetestsfile="included-$branch-$testtype-$buildtype.tests"
rm -f $includetestsfile
touch $includetestsfile

if [[ -z "$includetests" ]]; then
    # by default include tests appropriate for the branch
    includetests="e4x ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6"

    case "$branch" in
        1.8.0)
            ;;
        1.8.1)
            includetests="$includetests js1_7"
            ;;
        1.9.0)
            includetests="$includetests js1_7 js1_8"
            ;;
        1.9.1)
            includetests="$includetests js1_7 js1_8 ecma_3_1 js1_8_1"
            ;;
        1.9.2)
            includetests="$includetests js1_7 js1_8 ecma_3_1 js1_8_1"
            ;;
        1.9.3)
            includetests="$includetests js1_7 js1_8 ecma_3_1 js1_8_1"
            ;;
    esac
fi

for i in $includetests; do
    if [[ -f "$i" ]]; then
        echo "# including $i" >> $includetestsfile
        if echo $i | grep -q '\.js$'; then
            echo $i >> $includetestsfile
        else
            cat $i >> $includetestsfile
        fi
    elif [[ -d "$i" ]]; then
        find $i -name '*.js' -print | egrep -v '(shell|browser|template|jsref|userhook.*|\.#.*)\.js' | sed 's/^\.\///' | sort >> $includetestsfile
    fi
done

excludetestsfile="excluded-$branch-$testtype-$buildtype.tests"
rm -f $excludetestsfile
touch $excludetestsfile

if [[ -z "$excludetests" ]]; then
    excludetests="spidermonkey-n-$branch.tests performance-$branch.tests"
fi

for e in $excludetests; do
    if [[ -f "$e" ]]; then
        echo "# excluding $e" >> $excludetestsfile
        if echo $e | grep -q '\.js$'; then
            echo $e >> $excludetestsfile
        else
            cat $e >> $excludetestsfile
        fi
    elif [[ -d "$e" ]]; then
        find $e -name '*.js' -print | egrep -v '(shell|browser|template|jsref|userhook.*|\.#.*)\.js' | sed 's/^\.\///' | sort >> $excludetestsfile
    fi
done

if [[ -z "$TEST_MOZILLA_HG" ]]; then
    repo=CVS
else
    repo=`basename $TEST_MOZILLA_HG`
fi
debug "repo=$repo"

pattern="TEST_BRANCH=($branch|[.][*]), TEST_REPO=($repo|[.][*]), TEST_BUILDTYPE=($buildtype|[.][*]), TEST_TYPE=($testtype|[.][*]), TEST_OS=($OSID|[.][*]), TEST_KERNEL=($TEST_KERNEL|[.][*]), TEST_PROCESSORTYPE=($TEST_PROCESSORTYPE|[.][*]), TEST_MEMORY=($TEST_MEMORY|[.][*]),"

if [[ -z "$timeouts" ]]; then
    echo "# exclude tests that time out" >> $excludetestsfile
    egrep "$pattern .*TEST_EXITSTATUS=[^,]*TIMED OUT[^,]*," failures.txt | \
        sed 's/.*TEST_ID=\([^,]*\),.*/\1/' | sort -u >> $excludetestsfile
fi

if [[ -z "$crashes" ]]; then
    echo "# exclude tests that crash" >> $excludetestsfile
    egrep "$pattern .*TEST_EXITSTATUS=[^,]*(CRASHED|ABNORMAL)[^,]*" failures.txt  | \
        sed 's/.*TEST_ID=\([^,]*\),.*/\1/' | sort -u >> $excludetestsfile

fi

cat $includetestsfile | sed 's|^|include: |'
cat $excludetestsfile | sed 's|^|exclude: |'

case $testtype in
    shell)
        echo "JavaScriptTest: Begin Run"
        cat $includetestsfile | while read jsfile
        do
            if echo $jsfile | grep -q '^#'; then
                continue
            fi

            if ! grep -q $jsfile $excludetestsfile; then

                version=`shellfileversion $jsfile`

                subsuitetestdir=`dirname $jsfile`
                suitetestdir=`dirname $subsuitetestdir`
                echo "JavaScriptTest: Begin Test $jsfile"
                if [[ -z "$NARCISSUS" ]]; then
                    if eval $TIMECOMMAND timed_run.py $TEST_JSEACH_TIMEOUT \"$jsfile\" \
                        $EXECUTABLE_DRIVER \
                        $executable -v $version \
                        -S 524288 \
                        $gczealshell \
                        $splitobjects \
                        $jitshell \
                        -f ./shell.js \
                        -f $suitetestdir/shell.js \
                        -f $subsuitetestdir/shell.js \
                        -f ./$jsfile \
                        -f ./js-test-driver-end.js; then
                        true
                    else
                        rc=$?
                    fi
                else
                    if eval $TIMECOMMAND timed_run.py $TEST_JSEACH_TIMEOUT \"$jsfile\" \
                        $EXECUTABLE_DRIVER \
                        $executable -v $version \
                        -S 524288 \
                        $gczealshell \
                        $splitobjects \
                        $jitshell \
                        -f $NARCISSUS \
                        -e "evaluate\(\'load\(\\\"./shell.js\\\"\)\'\)" \
                        -e "evaluate\(\'load\(\\\"$suitetestdir/shell.js\\\"\)\'\)" \
                        -e "evaluate\(\'load\(\\\"$subsuitetestdir/shell.js\\\"\)\'\)" \
                        -e "evaluate\(\'load\(\\\"./$jsfile\\\"\)\'\)" \
                        -e "evaluate\(\'load\(\\\"./js-test-driver-end.js\\\"\)\'\)"; then
                        true
                    else
                        rc=$?
                    fi
                fi
                if [[ $rc == 99 ]]; then
                    # note that this loop is executing in a sub-process
                    # error will terminate the sub-process but will transfer
                    # control to the next statement following the loop which
                    # in this case is the "End Run" output which incorrectly
                    # labels the test run as completed.
                    error "User Interrupt"
                fi
                echo "JavaScriptTest: End Test $jsfile"
            fi
        done
        rc=$?
        if [[ $rc != 0 ]]; then
            error ""
        fi
        echo "JavaScriptTest: End Run"
        ;;

    browser)
        urllist="urllist-$branch-$testtype-$buildtype.tests"
        urlhtml="urllist-$branch-$testtype-$buildtype.html"

        rm -f $urllist $urlhtml

        cat > $urlhtml <<EOF
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>JavaScript Tests</title>
</head>
<body>
<ul>
EOF

        cat $includetestsfile | while read jsfile
        do
            if echo $jsfile | grep -q '^#'; then
                continue
            fi

            if ! grep -q $jsfile $excludetestsfile; then

                version=";version=`browserfileversion $jsfile`"

                echo "http://$TEST_HTTP/$TEST_WWW_JS/js-test-driver-standards.html?test=$jsfile;language=type;text/javascript$version$gczealbrowser$jitbrowser" >> $urllist
                echo "<li><a href='http://$TEST_HTTP/$TEST_WWW_JS/js-test-driver-standards.html?test=$jsfile;language=type;text/javascript$version$gczealbrowser$jitbrowser'>$jsfile</a></li>" >> $urlhtml
            fi
        done

        cat >> $urlhtml <<EOF
</ul>
</body>
</html>
EOF

        chmod a+r $urlhtml

        if [[ -z "$filesonly" ]]; then
            echo "JavaScriptTest: Begin Run"
            cat "$urllist" | while read url;
            do
                edit-talkback.sh -p "$product" -b "$branch" -x "$executablepath" -i "$url"
                jsfile=`echo $url | sed "s|http://$TEST_HTTP/$TEST_WWW_JS/js-test-driver-standards.html?test=\([^;]*\);.*|\1|"`
                echo "JavaScriptTest: Begin Test $jsfile"
                if eval $TIMECOMMAND timed_run.py $TEST_JSEACH_TIMEOUT \"$jsfile\" \
                    $EXECUTABLE_DRIVER \
                    \"$executable\" -P \"$profilename\" \
                    -spider -start -quit \
                    -uri \"$url\" \
                    -depth 0 -timeout \"$TEST_JSEACH_PAGE_TIMEOUT\" \
                    -hook \"http://$TEST_HTTP/$TEST_WWW_JS/userhookeach.js\"; then
                    true
                else
                    rc=$?
                fi
                if [[ $rc == 99 ]]; then
                    error "User Interrupt"
                fi
                echo "JavaScriptTest: End Test $jsfile"
            done
            echo "JavaScriptTest: End Run"
        fi
        ;;
    *)
        ;;
esac

popd
