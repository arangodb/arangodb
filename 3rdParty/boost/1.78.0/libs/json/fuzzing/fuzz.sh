#!/bin/sh
#
# builds the fuzzers, runs old crashes etc
#
# Optional: set environment variable CLANG, otherwise clang is auto detected.
#
# By Paul Dreik 2019-2020 for the boost json project
# License: Boost 1.0

set -e

fuzzdir=$(dirname $0)
me=$(basename $0)

cd $fuzzdir

if [ -z $CLANG ] ; then
    #see if we can find clang
    for clangver in -10 -9 -8 -7 -6 -6.0 "" ;   do
	CLANG=clang++$clangver
	if which $CLANG >/dev/null; then
	    break
	fi
    done
fi

if ! which $CLANG >/dev/null; then
    if ! -x $CLANG; then
	echo $me: sorry, could not find clang $CLANG
	exit 1
    fi
fi
echo "$me: will use this compiler: $CLANG"

# set the maximum size of the input, to avoid
# big inputs which blow up the corpus size
MAXLEN="-max_len=4000"

# If doing fuzzing locally (not in CI), adjust this to utilize more
# of your cpu.
#JOBS="-jobs=32"
JOBS=

# set a timelimit (you may want to adjust this if you run locally)
MAXTIME="-max_total_time=30"

variants="basic_parser parse parser"

for variant in $variants; do

srcfile=fuzz_$variant.cpp
fuzzer=./fuzzer_$variant

if [ ! -e $fuzzer -o $srcfile -nt $fuzzer ] ; then
    # explicitly set BOOST_JSON_STACK_BUFFER_SIZE small so interesting
    # code paths are taken also for small inputs (see https://github.com/CPPAlliance/json/issues/333)
    $CLANG \
        -std=c++17 \
        -O3 \
        -g \
        -fsanitize=fuzzer,address,undefined \
        -fno-sanitize-recover=undefined \
        -DBOOST_JSON_STANDALONE \
        -I../include \
        -DBOOST_JSON_STACK_BUFFER_SIZE=64  \
        -o $fuzzer \
        ../src/src.cpp \
        $srcfile
fi

# make sure ubsan stops in case anything is found
export UBSAN_OPTIONS="halt_on_error=1"

# make sure the old crashes pass without problems
if [ -d old_crashes/$variant ]; then
  find old_crashes/$variant -type f -print0 |xargs -0 --no-run-if-empty $fuzzer
fi

# make an initial corpus from the test data already in the repo
seedcorpus=seedcorpus/$variant
if [ ! -d $seedcorpus ] ; then
    mkdir -p $seedcorpus
    find ../test -name "*.json" -type f -print0 |xargs -0 --no-run-if-empty cp -f -t $seedcorpus/
fi

# if an old corpus exists, use it
# get it with curl -O --location -J https://bintray.com/pauldreik/boost.json/download_file?file_path=corpus%2Fcorpus.tar
if [ -e corpus.tar ] ; then
  mkdir -p oldcorpus
  tar xf corpus.tar -C oldcorpus || echo "corpus.tar was broken! ignoring it"
  OLDCORPUS=oldcorpus/cmin/$variant
  # in case the old corpus did not have this variant (when adding/renaming a new fuzzer)
  mkdir -p $OLDCORPUS
else
  OLDCORPUS=
fi


# run the fuzzer for a short while
mkdir -p out/$variant
$fuzzer out/$variant $OLDCORPUS $seedcorpus/ $MAXTIME $MAXLEN $JOBS

# minimize the corpus
mkdir -p cmin/$variant
$fuzzer cmin/$variant $OLDCORPUS out/$variant $seedcorpus/ -merge=1 $MAXLEN
done

