# Copyright 2014 Mario Mulansky
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or
# copy at http://www.boost.org/LICENSE_1_0.txt)

# make sure BOOST_ROOT is pointing to your boost directory
# otherwise, set it here:
# BOOST_ROOT = /path/to/boost
# you also need NT2s SIMD libary available set the include path here:
# SIMD_INCLUDE = /path/to/simd/include

INCLUDES = -I$(BOOST_ROOT) -I${SIMD_INCLUDE}

# INTEL COMPILER
# change this if you want to cross-compile
ARCH = Host
# ARCH = AVX
# ARCH = SSE4.2

CXX = icpc
CC = icpc
CXXFLAGS = -O3 -x${ARCH} -std=c++0x -fno-alias -inline-forceinline -DNDEBUG ${INCLUDES}
# -ip

# GCC COMPILER
# change this if you want to cross-compile
# ARCH = native
# # ARCH = core-avx-i

# CXX = g++
# CC = g++
# CXXFLAGS = -O3 -ffast-math -mtune=${ARCH} -march=${ARCH} -std=c++0x -DNDEBUG ${INCLUDES}
