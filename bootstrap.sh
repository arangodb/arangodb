#!/bin/bash

# usage:
# ./boostrap.sh [--git] [--asan] [--clang] source_destination
# @author Jan Christoph Uhde - 2017

arg_git=false
arg_asan=false

https_url="https://github.com/arangodb/arangodb.git"
git_url="git@github.com:arangodb/arangodb.git"

build_type="RelWithDebInfo"
compiler=gcc

args=()
for arg in "$@"; do
    if [[ $arg == "--" ]]; then
        break
    fi
    case $arg in
        --git)
            arg_git=true
        ;;
        --asan)
            arg_asan=true
        ;;
        --clang)
            compiler=clang
        ;;
        --gcc-arango)
            compiler=gcc-arango
        ;;
        *)
            args+=( "$arg" )
        ;;
    esac
done

configure_build(){
    local source_dir="$1"

    local asan=""
    if $arg_asan; then
        asan="-fsanitize=address -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=vptr"
    fi

    case "$compiler" in
        clang)
            flags="$asan"
            cxx="/usr/bin/clang++"
            cc="/usr/bin/clang"
        ;;
        gcc-arango)
            flags=" -lpthread $asan"
            cxx="/opt/arangodb/bin/g++"
            cc="/opt/arangodb/bin/gcc"
        ;;
        gcc)
            flags=" -lpthread $asan"
            cxx="/usr/bin/g++"
            cc="/usr/bin/gcc"
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
              -DUSE_FAILURE_TESTS=On \
              -DUSE_ENTERPRISE=OFF \
              "$source_dir"

    if [[ $? -eq 0 ]]; then
        echo "configuration successful (in $(pwd))"
        make -j $(nproc)
    else
        echo "failed to configure (in $(pwd))"
    fi
}

bootstrap(){
    local branch="$2"

    local source_dir="$1"
    local build_dir="${source_dir}-build"
    mkdir -p "$build_dir"

    local url="$https_url"
    if $git; then
        url="$git_url"
    fi

    #store path
    current_dir="$(pwd)"

    if [[ -e $source_dir ]]; then
        echo "A file/directory already exists in the chosen location!"
        exit 1
    fi

    echo "cloning '$url' into '$source_dir'"
    git clone "$url" "$source_dir"

    echo "changing into '$source_dir'"
    cd $source_dir || { echo "can not change into $source_dir"; exit 1; }

    if [[ -n "$branch" ]]; then
        echo "checking out '$branch'"
        git checkout "$branch" || { echo "can not change into $source_dir"; exit 1; }
    fi

    # TODO more error handling instead of: 'set -e'
    set -e
    echo "initialize submodules"
    git submodule init
    git submodule update --recursive
    cd 3rdParty/V8/v8
    git submodule init
    git submodule update --recursive

    echo "linking $current_dir/$build_dir to build"
    ln -s "$current_dir/$build_dir" "$current_dir/$source_dir/build"

    cd "$current_dir/$build_dir"
    set +e

    configure_build "$current_dir/$source_dir"

    # in a better language the build command should be here but by
    # putting it at the end of configure_build unnecessary parsing is
    # avoided.
}

bootstrap "${args[@]}"
