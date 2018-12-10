# S2 Geometry Library

[![Build Status](https://travis-ci.org/google/s2geometry.svg?branch=master)](https://travis-ci.org/google/s2geometry)

## Overview

This is a package for manipulating geometric shapes. Unlike many geometry
libraries, S2 is primarily designed to work with _spherical geometry_, i.e.,
shapes drawn on a sphere rather than on a planar 2D map. This makes it
especially suitable for working with geographic data.

If you want to learn more about the library, start by reading the
[overview](http://s2geometry.io/about/overview) and [quick start
document](http://s2geometry.io/devguide/cpp/quickstart), then read the
introduction to the [basic types](http://s2geometry.io/devguide/basic_types).

S2 documentation can be found on [s2geometry.io](http://s2geometry.io).

## Requirements for End Users

* [CMake](http://www.cmake.org/)
* A C++ compiler with C++11 support, such as [g++](https://gcc.gnu.org/)
  \>= 4.7.
* [OpenSSL](https://github.com/openssl/openssl) (for its bignum library)
* [gflags command line flags](https://github.com/gflags/gflags), optional
* [glog logging module](https://github.com/google/glog), optional
* [googletest testing framework](https://github.com/google/googletest)
  (to build tests and example programs, optional)

On Ubuntu, all of these can be installed via apt-get:

```
sudo apt-get install cmake libgflags-dev libgoogle-glog-dev libgtest-dev libssl-dev
```

Otherwise, you may need to install some from source.

On macOS, use [MacPorts](http://www.macports.org/) or
[Homebrew](http://brew.sh/).  For MacPorts:

```
sudo port install cmake gflags google-glog openssl
```

Do not install `gtest` from MacPorts; instead download [release
1.8.0](https://github.com/google/googletest/releases/tag/release-1.8.0), unpack,
and substitute

```
cmake -DGTEST_ROOT=/...absolute path to.../googletest-release-1.8.0/googletest ..
```

in the build instructions below.

Thorough testing has only been done on Ubuntu 14.04.3 and macOS 10.12.

## Build and Install

You may either download the source as a ZIP archive, or [clone the git
repository](https://help.github.com/articles/cloning-a-repository/).

### Via ZIP archive

Download [ZIP file](https://github.com/google/s2geometry/archive/master.zip)

```
cd [parent of directory where you want to put S2]
unzip [path to ZIP file]/s2geometry-master.zip
cd s2geometry-master
```

### Via `git clone`

```
cd [parent of directory where you want to put S2]
git clone https://github.com/google/s2geometry.git
cd s2geometry
```

### Building

From the appropriate directory depending on how you got the source:

```
mkdir build
cd build
# You can omit -DGTEST_ROOT to skip tests; see above for macOS.
cmake -DGTEST_ROOT=/usr/src/gtest ..
make
make test  # If GTEST_ROOT specified above.
sudo make install
```

Enable gflags and glog with `cmake -DWITH_GFLAGS=ON -DWITH_GLOG=ON ...`.

Disable building of shared libraries with `-DBUILD_SHARED_LIBS=OFF`.

## Python

If you want the Python interface, you will also need:

* [SWIG](https://github.com/swig/swig) (for Python support, optional)

which can be installed via

```
sudo apt-get install swig
```

or on macOS:

```
sudo port install swig
```
Expect to see some warnings if you build with swig 2.0.

## Other S2 implementations

* [Go](https://github.com/golang/geo) (Approximately 40% complete.)
* [Java](https://github.com/google/s2-geometry-library-java) (Older version;
  last updated in 2011.)

## Disclaimer

This is not an official Google product.
