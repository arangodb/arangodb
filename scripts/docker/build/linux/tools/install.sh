#!/bin/bash

# Set links for GCC
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${COMPILER_VERSION} 10 \
  --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-${COMPILER_VERSION} \
  --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-${COMPILER_VERSION} \
  --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-${COMPILER_VERSION} \
  --slave /usr/bin/gcov gcov /usr/bin/gcov-${COMPILER_VERSION}

update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${COMPILER_VERSION} 10

update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30
update-alternatives --set cc /usr/bin/gcc

update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30
update-alternatives --set c++ /usr/bin/g++

# Compile openssl library
build_openssl() (
  local branch="$1" patch="$2"
  local version="${branch}.${patch}"
  local tarball="openssl-${version}.tar.gz"

  cd /tmp || exit 1
  curl -L --output "$tarball" "https://www.openssl.org/source/${tarball}" || exit 1
  tar xzf "$tarball" || exit 1
  cd "openssl-${version}" || exit 1
  ./config --prefix=/opt no-async enable-fips || exit 1
  make -j"$(nproc)" || exit 1
  make install_dev || exit 1

  cd /tmp
  rm -rf "$tarball" "openssl-${version}"

  # Clean up any strange cores
  rm -rf /core.*
); build_openssl "$1" "$2" || exit 1

# Build the FIPS provider module
build_fips() (
  local version="3.1.2"
  local sha256="a0ce69b8b97ea6a35b96875235aa453b966ba3cba8af2de23657d8b6767d6539"
  local tarball="openssl-${version}.tar.gz"

  cd /tmp || exit 1
  curl -L --output "$tarball" "https://www.openssl.org/source/old/3.1/${tarball}" || exit 1
  echo "${sha256}  ${tarball}" | sha256sum -c - || exit 1
  tar xzf "$tarball" || exit 1
  cd "openssl-${version}" || exit 1

  ./config --prefix=/opt enable-fips || exit 1
  make -j"$(nproc)" || exit 1

  mkdir -p /opt/lib/ossl-modules
  cp providers/fips.so /opt/lib/ossl-modules/

  cd /tmp
  rm -rf "$tarball" "openssl-${version}"

  rm -rf /core.*
); build_fips || exit 1
