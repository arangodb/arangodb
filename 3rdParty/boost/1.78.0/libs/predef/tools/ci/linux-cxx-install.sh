#!/bin/sh

# Usage:
# LLVM_OS: LLVM OS release to obtain clang binaries. Only needed for clang install.
# LLVM_VER: The LLVM toolset version to point the repo at.
# PACKAGES: Compiler packages to install.

set -e
echo ">>>>>"
echo ">>>>> APT: REPO.."
echo ">>>>>"
sudo -E apt-add-repository -y "ppa:ubuntu-toolchain-r/test"
if test -n "${LLVM_OS}" -a -n "${LLVM_VER}" ; then
	wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
	sudo -E apt-add-repository "deb http://apt.llvm.org/${LLVM_OS}/ llvm-toolchain-${LLVM_OS}-${LLVM_VER} main"
fi
echo ">>>>>"
echo ">>>>> APT: UPDATE.."
echo ">>>>>"
sudo -E apt-get -o Acquire::Retries=3 update
echo ">>>>>"
echo ">>>>> APT: INSTALL ${PACKAGES}.."
echo ">>>>>"
sudo -E apt-get -o Acquire::Retries=3 -yq --no-install-suggests --no-install-recommends install ${PACKAGES}

# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020.
