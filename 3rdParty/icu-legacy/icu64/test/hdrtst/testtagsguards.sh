#/usr/bin/env bash
# Copyright © 2019 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# Run this script from $ICU_ROOT/icu4c.
# ~/icu/mine/src/icu4c$ source/test/hdrtst/testtagsguards.sh

# set -x # echo on

: ${DEF:=-DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1}
: ${INCL:="-Isource/common -Isource/i18n -Isource/io"}
: ${TMPDIR:=`mktemp -d`}
: ${DIFF:="diff -u --minimal"}
: ${CXX:="clang++"}

echo "*** testtagsguards.sh TMPDIR=$TMPDIR"

for file in source/common/unicode/*.h source/i18n/unicode/*.h source/io/unicode/*.h ; do
    base=`basename $file`
    echo $file
    echo '#include "unicode/'$base'"' > $TMPDIR/ht-$base.cpp
    # Preprocess only.
    $CXX $INCL -C -E $DEF -o $TMPDIR/ht-$base-normal.i $TMPDIR/ht-$base.cpp

    # When hiding @draft, none should be in the output.
    TAG=draft
    GUARD=DRAFT
    echo "    @$TAG"
    $CXX $INCL -C -E -DU_HIDE_${GUARD}_API=1 -DU_FORCE_HIDE_${GUARD}_API=1 $DEF -o $TMPDIR/ht-$base-$TAG.i $TMPDIR/ht-$base.cpp
    if grep "@$TAG" -C 5 $TMPDIR/ht-$base-$TAG.i; then
        echo "*** error: @$TAG not hidden in $TMPDIR/ht-$base-$TAG.i"
        exit 1
    fi
    # Only @draft should be hidden.
    # Except: Ok to hide nested @internal/@system/@obsolete.
    $DIFF $TMPDIR/ht-$base-normal.i $TMPDIR/ht-$base-$TAG.i > $TMPDIR/ht-$base-normal-$TAG.txt
    if egrep '^-.*@(stable|deprecated)' -C 5 $TMPDIR/ht-$base-normal-$TAG.txt; then
        echo "*** error: Non-@$TAG hidden in $TMPDIR/ht-$base-$TAG.i see $TMPDIR/ht-$base-normal-$TAG.txt"
        cat $TMPDIR/ht-$base-normal-$TAG.txt
        exit 1
    fi

    # @deprecated
    TAG=deprecated
    GUARD=DEPRECATED
    echo "    @$TAG"
    $CXX $INCL -C -E -DU_HIDE_${GUARD}_API=1 -DU_FORCE_HIDE_${GUARD}_API=1 $DEF -o $TMPDIR/ht-$base-$TAG.i $TMPDIR/ht-$base.cpp
    if grep "@$TAG\b.*\bICU\b" -C 5 $TMPDIR/ht-$base-$TAG.i; then
        echo "*** error: @$TAG not hidden in $TMPDIR/ht-$base-$TAG.i"
        exit 1
    fi
    # In the egrep: All tags except $TAG and @internal & similar.
    $DIFF $TMPDIR/ht-$base-normal.i $TMPDIR/ht-$base-$TAG.i > $TMPDIR/ht-$base-normal-$TAG.txt
    if egrep '^-.*@(stable|draft)' -C 5 $TMPDIR/ht-$base-normal-$TAG.txt; then
        echo "*** error: Non-@$TAG hidden in $TMPDIR/ht-$base-$TAG.i see $TMPDIR/ht-$base-normal-$TAG.txt"
        cat $TMPDIR/ht-$base-normal-$TAG.txt
        exit 1
    fi

    # TODO: @internal
    # Hiding some @internal definitions, in particular in platform.h and similar,
    # tends to break even preprocessing of other headers.

    # @system
    TAG=system
    GUARD=SYSTEM
    echo "    @$TAG"
    $CXX $INCL -C -E -DU_HIDE_${GUARD}_API=1 -DU_FORCE_HIDE_${GUARD}_API=1 $DEF -o $TMPDIR/ht-$base-$TAG.i $TMPDIR/ht-$base.cpp
    if grep "@$TAG" -C 5 $TMPDIR/ht-$base-$TAG.i; then
        echo "*** error: @$TAG not hidden in $TMPDIR/ht-$base-$TAG.i"
        exit 1
    fi
    # @system is orthogonal to @stable / @deprecated etc.,
    # so we don't check that none of those are hidden.

    # @obsolete
    TAG=obsolete
    GUARD=OBSOLETE
    echo "    @$TAG"
    $CXX $INCL -C -E -DU_HIDE_${GUARD}_API=1 -DU_FORCE_HIDE_${GUARD}_API=1 $DEF -o $TMPDIR/ht-$base-$TAG.i $TMPDIR/ht-$base.cpp
    if grep "@$TAG" -C 5 $TMPDIR/ht-$base-$TAG.i; then
        echo "*** error: @$TAG not hidden in $TMPDIR/ht-$base-$TAG.i"
        exit 1
    fi
    # In the egrep: All tags except $TAG and @internal & similar.
    $DIFF $TMPDIR/ht-$base-normal.i $TMPDIR/ht-$base-$TAG.i > $TMPDIR/ht-$base-normal-$TAG.txt
    if egrep '^-.*@(stable|draft|deprecated)' -C 5 $TMPDIR/ht-$base-normal-$TAG.txt; then
        echo "*** error: Non-@$TAG hidden in $TMPDIR/ht-$base-$TAG.i see $TMPDIR/ht-$base-normal-$TAG.txt"
        cat $TMPDIR/ht-$base-normal-$TAG.txt
        exit 1
    fi
done

echo "pass"
rm -rf $TMPDIR

