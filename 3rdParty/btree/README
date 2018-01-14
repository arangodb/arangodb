This library is a C++ template library and, as such, there is no
library to build and install.  Copy the .h files and use them!

See http://code.google.com/p/cpp-btree/wiki/UsageInstructions for
details.

----

To build and run the provided tests, however, you will need to install
CMake, the Google C++ Test framework, and the Google flags package.

Download and install CMake from http://www.cmake.org

Download and build the GoogleTest framework from
http://code.google.com/p/googletest

Download and install gflags from https://code.google.com/p/gflags

Set GTEST_ROOT to the directory where GTEST was built.
Set GFLAGS_ROOT to the directory prefix where GFLAGS is installed.

export GTEST_ROOT=/path/for/gtest-x.y
export GFLAGS_ROOT=/opt

cmake . -Dbuild_tests=ON

For example, to build on a Unix system with the clang++ compiler,

export GTEST_ROOT=$(HOME)/src/googletest
export GFLAGS_ROOT=/opt
cmake . -G "Unix Makefiles" -Dbuild_tests=ON -DCMAKE_CXX_COMPILER=clang++
