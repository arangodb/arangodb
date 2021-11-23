#!/bin/sh

export VARIANT=ubasan
export TOOLSET=clang
export TRAVIS=0
export BOOST_ROOT="`pwd`"

"$1"
