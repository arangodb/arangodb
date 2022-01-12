[![Boost.JSON](https://raw.githubusercontent.com/CPPAlliance/json/master/doc/images/repo-logo-3.png)](http://master.json.cpp.al/)

Branch          | [`master`](https://github.com/CPPAlliance/json/tree/master) | [`develop`](https://github.com/CPPAlliance/json/tree/develop) |
--------------- | ----------------------------------------------------------- | ------------------------------------------------------------- |
[Azure](https://azure.microsoft.com/en-us/services/devops/pipelines/) | [![Build Status](https://img.shields.io/azure-devops/build/vinniefalco/2571d415-8cc8-4120-a762-c03a8eda0659/8/master)](https://vinniefalco.visualstudio.com/json/_build/latest?definitionId=5&branchName=master) | [![Build Status](https://img.shields.io/azure-devops/build/vinniefalco/2571d415-8cc8-4120-a762-c03a8eda0659/8/develop)](https://vinniefalco.visualstudio.com/json/_build/latest?definitionId=8&branchName=develop)
Docs            | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](https://www.boost.org/doc/libs/master/libs/json/) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](https://www.boost.org/doc/libs/develop/libs/json/)
[Drone](https://drone.io/) | [![Build Status](https://drone.cpp.al/api/badges/boostorg/json/status.svg?ref=refs/heads/master)](https://drone.cpp.al/boostorg/json) | [![Build Status](https://drone.cpp.al/api/badges/boostorg/json/status.svg?ref=refs/heads/develop)](https://drone.cpp.al/boostorg/json)
Matrix          | [![Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/json.html) | [![Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/json.html)
Fuzzing         | --- |  [![fuzz](https://github.com/boostorg/json/workflows/fuzz/badge.svg?branch=develop)](https://github.com/boostorg/json/actions?query=workflow%3Afuzz+branch%3Adevelop)
[Appveyor](https://ci.appveyor.com/) | [![Build status](https://ci.appveyor.com/api/projects/status/8csswcnmfm798203?branch=master&svg=true)](https://ci.appveyor.com/project/vinniefalco/cppalliance-json/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/8csswcnmfm798203?branch=develop&svg=true)](https://ci.appveyor.com/project/vinniefalco/cppalliance-json/branch/develop)
[codecov.io](https://codecov.io) | [![codecov](https://codecov.io/gh/boostorg/json/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/json/branch/master) | [![codecov](https://codecov.io/gh/boostorg/json/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/json/branch/develop)

# Boost.JSON

## Overview

Boost.JSON is a portable C++ library which provides containers and
algorithms that implement
[JavaScript Object Notation](https://json.org/), or simply "JSON",
a lightweight data-interchange format. This format is easy for humans to
read and write, and easy for machines to parse and generate. It is based
on a subset of the JavaScript Programming Language
([Standard ECMA-262](https://www.ecma-international.org/ecma-262/10.0/index.html)).
JSON is a text format that is language-independent but uses conventions
that are familiar to programmers of the C-family of languages, including
C, C++, C#, Java, JavaScript, Perl, Python, and many others. These
properties make JSON an ideal data-interchange language.

This library focuses on a common and popular use-case: parsing
and serializing to and from a container called `value` which
holds JSON types. Any `value` which you build can be serialized
and then deserialized, guaranteeing that the result will be equal
to the original value. Whatever JSON output you produce with this
library will be readable by most common JSON implementations
in any language.

The `value` container is designed to be well suited as a
vocabulary type appropriate for use in public interfaces and
libraries, allowing them to be composed. The library restricts
the representable data types to the ranges which are almost
universally accepted by most JSON implementations, especially
JavaScript. The parser and serializer are both highly performant,
meeting or exceeding the benchmark performance of the best comparable
libraries. Allocators are very well supported. Code which uses these
types will be easy to understand, flexible, and performant.

Boost.JSON offers these features:

* Fast compilation
* Require only C++11
* Fast streaming parser and serializer
* Constant-time key lookup for objects
* Options to allow non-standard JSON
* Easy and safe modern API with allocator support
* Compile without Boost, define `BOOST_JSON_STANDALONE` (*deprecated*)
* Optional header-only, without linking to a library

## Requirements

The library relies heavily on these well known C++ types in
its interfaces (henceforth termed _standard types_):

* `string_view`
* `memory_resource`, `polymorphic_allocator`
* `error_category`, `error_code`, `error_condition`, `system_error`

The requirements for Boost.JSON depend on whether the library is used
as part of Boost, or in the standalone flavor (without Boost):

### Using Boost

* Requires only C++11
* The default configuration
* Aliases for standard types use their Boost equivalents
* Link to a built static or dynamic Boost library, or use header-only (see below)
* Supports -fno-exceptions, detected automatically

### Without Boost

> Warning: standalone use is deprecated and will be removed in a future release
> of Boost.JSON.

* Requires C++17
* Aliases for standard types use their `std` equivalents
* Obtained when defining the macro `BOOST_JSON_STANDALONE`
* Link to a built static or dynamic standalone library, or use header-only (see below)
* Supports -fno-exceptions: define `BOOST_NO_EXCEPTIONS` and `boost::throw_exception` manually

When using without Boost, support for `<memory_resource>` is required.
In particular, if using libstdc++ then version 8.3 or later is needed.

### Header-Only

To use as header-only; that is, to eliminate the requirement to
link a program to a static or dynamic Boost.JSON library, simply
place the following line in exactly one new or existing source
file in your project.
```
#include <boost/json/src.hpp>
```

### Standalone Shared Library

> Warning: standalone use is deprecated and will be removed in a future release
> of Boost.JSON.

To build a standalone shared library, it is necessary to define the
macros `BOOST_JSON_DECL` and `BOOST_JSON_CLASS_DECL` as appropriate
for your toolchain. Example for MSVC:
```
// When building the DLL
#define BOOST_JSON_DECL       __declspec(dllexport)
#define BOOST_JSON_CLASS_DECL __declspec(dllexport)

// When building the application which uses the DLL
#define BOOST_JSON_DECL       __declspec(dllimport)
#define BOOST_JSON_CLASS_DECL __declspec(dllimport)
```

### Embedded

Boost.JSON works great on embedded devices. The library uses local
stack buffers to increase the performance of some operations. On
Intel platforms these buffers are large (4KB), while on non-Intel
platforms they are small (256 bytes). To adjust the size of the
stack buffers for embedded applications define this macro when
building the library or including the function definitions:
```
#define BOOST_JSON_STACK_BUFFER_SIZE 1024
#include <boost/json/src.hpp>
```

#### Note
    This library uses separate inline namespacing for the standalone
    mode to allow libraries which use different modes to compose
    without causing link errors. Linking to both modes of Boost.JSON
    (Boost and standalone) is possible, but not recommended.

### Supported Compilers

Boost.JSON has been tested with the following compilers:

* clang: 3.8, 4, 5, 6, 7, 8, 9, 10, 11
* gcc: 4.8, 4.9, 5, 6, 7, 8, 9, 10
* msvc: 14.0, 14.1, 14.2

### Quality Assurance

The development infrastructure for the library includes
these per-commit analyses:

* Coverage reports
* Benchmark performance comparisons
* Compilation and tests on Drone.io, Azure Pipelines, Appveyor
* Fuzzing using clang-llvm and machine learning

## Visual Studio Solution

    cmake -G "Visual Studio 16 2019" -A Win32 -B bin -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/msvc.cmake
    cmake -G "Visual Studio 16 2019" -A x64 -B bin64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/msvc.cmake

## License

Distributed under the Boost Software License, Version 1.0.
(See accompanying file [LICENSE_1_0.txt](LICENSE_1_0.txt) or copy at
https://www.boost.org/LICENSE_1_0.txt)
