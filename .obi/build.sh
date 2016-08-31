#!/bin/bash

build_dir="../arangodb-build"

echo "delete old build"
rm -fr "$build_dir/"*
echo "create dir if neccessary"
[ -d $build_dir ] || mkdir -p $build_dir
echo "change into build dir"
cd $build_dir || exit

#asan="-fsanitize=address -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=vptr"

compiler=clang
build_type="Debug"
debug_flags="-g -O0"

case "$compiler" in
    *clang*)
        flags="$debug_flags"
        cxx=/usr/local/bin/clang++
        cc=/usr/local/bin/clang

    ;;
    *gcc*)
        flags="$debug_flags -lpthread -gdwarf-4"
        cxx="$HOME/.bin/g++-6"
        cc="$HOME/.bin/g++-6"
        export LD_LIBRARY_PATH=/opt/gcc_stable_2016-07-06/lib64
    ;;
esac

cxx_flags="$flags -std=c++11"

CXX=$cxx \
CC=$cc \
CXXFLAGS="$cxx_flags" \
CFLAGS="$flags" \
    cmake -DCMAKE_BUILD_TYPE=$build_type \
          -DUSE_MAINTAINER_MODE=On \
          -DUSE_BOOST_UNITTESTS=On \
          ../arangodb

cat  << here
COMMAND: CXX=$cxx
CC=$cc
CXXFLAGS="$cxx_flags"
CFLAGS="$flags"
    cmake -DCMAKE_BUILD_TYPE=$build_type
          -DUSE_MAINTAINER_MODE=On
          -DUSE_BOOST_UNITTESTS=On
          ../arangodb
here

if [[ $? ]]; then
    echo "configuration successful"
    pwd
else
    echo "failed to configure"
    pwd
fi
