![logo](https://raw.githubusercontent.com/boostorg/python/develop/doc/images/bpl.png)

# Synopsis

Welcome to Boost.Python, a C++ library which enables seamless interoperability between C++ and the Python programming language. The library includes support for:

* References and Pointers
* Globally Registered Type Coercions
* Automatic Cross-Module Type Conversions
* Efficient Function Overloading
* C++ to Python Exception Translation
* Default Arguments
* Keyword Arguments
* Manipulating Python objects in C++
* Exporting C++ Iterators as Python Iterators
* Documentation Strings

See the [Boost.Python](http://boostorg.github.io/python) documentation for details.

# Building ![Build Status](https://travis-ci.org/boostorg/python.svg?branch=develop)

While Boost.Python is part of the Boost C++ Libraries super-project, and thus can be compiled as part of Boost, it can also be compiled and installed stand-alone, i.e. against a pre-installed Boost package.

## Prerequisites

* [Python](http://www.python.org)
* [Boost](http://www.boost.org)
* [SCons](http://www.scons.org)

## Configure

Simply run

```
scons config [options]
```
to prepare a build. See `scons -h` for a description of the available options. For example
```
scons config --boost=/path/to/boost --python=/path/to/python
```
will configure Boost.Python to be built against the two specific versions of Boost and Python.

## Build

Run

```
scons
```
to build the library.

## Test

Run

```
scons test
```
to run the tests.

## Build docs

Run

```
scons doc
```
to build the documentation.