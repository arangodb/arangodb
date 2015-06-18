#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang, and then package the results up
# to a tgz file.

gcc_toolchain=

# Parse command line options.
while [[ $# > 0 ]]; do
  case $1 in
    --gcc-toolchain)
      shift
      if [[ $# == 0 ]]; then
        echo "--gcc-toolchain requires an argument."
        exit 1
      fi
      if [[ -x "$1/bin/gcc" ]]; then
        gcc_toolchain=$1
      else
        echo "Invalid --gcc-toolchain: '$1'."
        echo "'$1/bin/gcc' does not appear to be valid."
        exit 1
      fi
      ;;

    --help)
      echo "usage: $0 [--gcc-toolchain <prefix>]"
      echo
      echo "--gcc-toolchain: Set the prefix for which GCC version should"
      echo "    be used for building. For example, to use gcc in"
      echo "    /opt/foo/bin/gcc, use '--gcc-toolchain '/opt/foo"
      echo
      exit 1
      ;;
    *)
      echo "Unknown argument: '$1'."
      echo "Use --help for help."
      exit 1
      ;;
  esac
  shift
done


THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BOOTSTRAP_DIR="${THIS_DIR}/../../../third_party/llvm-bootstrap"
LLVM_BOOTSTRAP_INSTALL_DIR="${LLVM_DIR}/../llvm-bootstrap-install"
LLVM_BUILD_DIR="${THIS_DIR}/../../../third_party/llvm-build"
LLVM_BIN_DIR="${LLVM_BUILD_DIR}/Release+Asserts/bin"
LLVM_LIB_DIR="${LLVM_BUILD_DIR}/Release+Asserts/lib"
STAMP_FILE="${LLVM_DIR}/../llvm-build/cr_build_revision"

echo "Diff in llvm:" | tee buildlog.txt
svn stat "${LLVM_DIR}" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/tools/clang:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/tools/clang" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/tools/clang" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/compiler-rt:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/compiler-rt" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/compiler-rt" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/projects/libcxx:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/projects/libcxx" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/projects/libcxx" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/projects/libcxxabi:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/projects/libcxxabi" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/projects/libcxxabi" 2>&1 | tee -a buildlog.txt


echo "Starting build" | tee -a buildlog.txt

set -exu
set -o pipefail

# Do a clobber build.
rm -rf "${LLVM_BOOTSTRAP_DIR}"
rm -rf "${LLVM_BOOTSTRAP_INSTALL_DIR}"
rm -rf "${LLVM_BUILD_DIR}"
extra_flags=
if [[ -n "${gcc_toolchain}" ]]; then
  extra_flags="--gcc-toolchain ${gcc_toolchain}"
fi
"${THIS_DIR}"/update.sh --bootstrap --force-local-build --run-tests \
    ${extra_flags} 2>&1 | tee -a buildlog.txt

R=$(cat "${STAMP_FILE}")

PDIR=clang-$R
rm -rf $PDIR
mkdir $PDIR
mkdir $PDIR/bin
mkdir $PDIR/lib

GOLDDIR=llvmgold-$R
if [ "$(uname -s)" = "Linux" ]; then
  mkdir -p $GOLDDIR/lib
fi

if [ "$(uname -s)" = "Darwin" ]; then
  SO_EXT="dylib"
else
  SO_EXT="so"
fi

# Copy buildlog over.
cp buildlog.txt $PDIR/

# Copy clang into pdir, symlink clang++ to it.
cp "${LLVM_BIN_DIR}/clang" $PDIR/bin/
(cd $PDIR/bin && ln -sf clang clang++)
cp "${LLVM_BIN_DIR}/llvm-symbolizer" $PDIR/bin/
if [ "$(uname -s)" = "Darwin" ]; then
  cp "${LLVM_BIN_DIR}/libc++.1.${SO_EXT}" $PDIR/bin/
  (cd $PDIR/bin && ln -sf libc++.1.dylib libc++.dylib)
fi

# Copy libc++ headers.
if [ "$(uname -s)" = "Darwin" ]; then
  mkdir $PDIR/include
  cp -R "${LLVM_BOOTSTRAP_INSTALL_DIR}/include/c++" $PDIR/include
fi

# Copy plugins. Some of the dylibs are pretty big, so copy only the ones we
# care about.
cp "${LLVM_LIB_DIR}/libFindBadConstructs.${SO_EXT}" $PDIR/lib
cp "${LLVM_LIB_DIR}/libBlinkGCPlugin.${SO_EXT}" $PDIR/lib

# Copy gold plugin on Linux.
if [ "$(uname -s)" = "Linux" ]; then
  cp "${LLVM_LIB_DIR}/LLVMgold.${SO_EXT}" $GOLDDIR/lib
fi

if [[ -n "${gcc_toolchain}" ]]; then
  # Copy the stdlibc++.so.6 we linked Clang against so it can run.
  cp "${LLVM_LIB_DIR}/libstdc++.so.6" $PDIR/lib
fi

# Copy built-in headers (lib/clang/3.x.y/include).
# compiler-rt builds all kinds of libraries, but we want only some.
if [ "$(uname -s)" = "Darwin" ]; then
  # Keep only the OSX (ASan and profile) and iossim (ASan) runtime libraries:
  # Release+Asserts/lib/clang/*/lib/darwin/libclang_rt.{asan,profile}_*
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/darwin*' \
       ! -name '*asan_osx*' ! -name '*asan_iossim*' ! -name '*profile_osx*' | \
      xargs rm
  # Fix LC_ID_DYLIB for the ASan dynamic libraries to be relative to
  # @executable_path.
  # TODO(glider): this is transitional. We'll need to fix the dylib name
  # either in our build system, or in Clang. See also http://crbug.com/344836.
  ASAN_DYLIB_NAMES="libclang_rt.asan_osx_dynamic.dylib
    libclang_rt.asan_iossim_dynamic.dylib"
  for ASAN_DYLIB_NAME in $ASAN_DYLIB_NAMES
  do
    ASAN_DYLIB=$(find "${LLVM_LIB_DIR}/clang" \
                      -type f -path "*${ASAN_DYLIB_NAME}")
    install_name_tool -id @executable_path/${ASAN_DYLIB_NAME} "${ASAN_DYLIB}"
    strip -x "${ASAN_DYLIB}"
  done
else
  # Keep only
  # Release+Asserts/lib/clang/*/lib/linux/libclang_rt.{[atm]san,san,ubsan,profile}-*.a
  # , but not dfsan.
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/linux*' \
       ! -name '*[atm]san*' ! -name '*ubsan*' ! -name '*libclang_rt.san*' \
       ! -name '*profile*' | xargs rm -v
  # Strip the debug info from the runtime libraries.
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/linux*' ! -name '*.syms' | xargs strip -g
fi

cp -vR "${LLVM_LIB_DIR}/clang" $PDIR/lib

if [ "$(uname -s)" = "Darwin" ]; then
  tar zcf $PDIR.tgz -C $PDIR bin include lib buildlog.txt
else
  tar zcf $PDIR.tgz -C $PDIR bin lib buildlog.txt
fi

if [ "$(uname -s)" = "Linux" ]; then
  tar zcf $GOLDDIR.tgz -C $GOLDDIR lib
fi

if [ "$(uname -s)" = "Darwin" ]; then
  PLATFORM=Mac
else
  PLATFORM=Linux_x64
fi

echo To upload, run:
echo gsutil cp -a public-read $PDIR.tgz \
     gs://chromium-browser-clang/$PLATFORM/$PDIR.tgz
if [ "$(uname -s)" = "Linux" ]; then
  echo gsutil cp -a public-read $GOLDDIR.tgz \
       gs://chromium-browser-clang/$PLATFORM/$GOLDDIR.tgz
fi

# FIXME: Warn if the file already exists on the server.
