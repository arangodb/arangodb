![boosttest logo](doc/html/images/boost.test.logo.png)

# What is Boost.Test?
Boost.Test is a C++03 and C++11/14 unit testing library, available on a wide range of platforms and compilers.

The library is part of [Boost](www.boost.org). The latest release
of the library is available from the boost web site.

Full instructions for use of this library can be accessed from
http://www.boost.org/doc/libs/release/libs/test/

# Key features

* Easy to get started with:     
    1. download and deflate the latest boost archive
    1. create a test module with this (header version):
        ```
        #define BOOST_TEST_MODULE your_test_module
        #include <boost/test/included/unit_test.hpp>
        ```
    1. done
* powerful and unique test assertion macro [`BOOST_TEST`](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/testing_tools/boost_test_universal_macro.html), that understands floating points, collections, strings... and uses appropriate comparison paradigm
* self-registering test cases, organize cases in test suites, apply fixtures on test cases, suites or globally
* provide assertion [context](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/test_output/test_tools_support_for_logging/contexts.html) for advanced diagnostic on failure
* powerful and extensible [dataset](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/tests_organization/test_cases/test_case_generation.html) tests
* add [decoration](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/tests_organization/decorators.html) to test cases and suites for [advanced description](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/tests_organization/semantic.html), [group/label](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/tests_organization/tests_grouping.html), and [dependencies](http://www.boost.org/doc/libs/release/libs/test/doc/html/boost_test/tests_organization/tests_dependencies.html)
* powerful command line options and test case filters
* extensible logging, XML and JUNIT outputs for third-party tools (eg. cont. integration)
* various usage (shared/static library) for faster compilation/build cycles, smaller binaries

# Copyright and license
Copyright 2001-2014, Gennadiy Rozental.  
Copyright 2013-2018, Boost.Test team.

Distributed under the Boost Software License, Version 1.0.
(Get a copy at www.boost.org/LICENSE_1_0.txt)

# Contribute
Please read [this document](CONTRIBUTE.md) to get started.

### Build Status

Branch          | Travis | Appveyor | Coverity Scan | codecov.io | Deps | Docs | Tests |
:-------------: | ------ | -------- | ------------- | ---------- | ---- | ---- | ----- |
[`master`](https://github.com/boostorg/test/tree/master) | [![Build Status](https://travis-ci.org/boostorg/test.svg?branch=master)](https://travis-ci.org/boostorg/test) | [![Build status](https://ci.appveyor.com/api/projects/status/nuihr6s92fjb9gwy/branch/master?svg=true)](https://ci.appveyor.com/project/maintainer/test-xyzzy/branch/master) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/99999/badge.svg)](https://scan.coverity.com/projects/boostorg-test) | [![codecov](https://codecov.io/gh/boostorg/test/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/test/branch/master)| [![Deps](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/test.html) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](http://www.boost.org/doc/libs/master/doc/html/test.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/test.html)
[`develop`](https://github.com/boostorg/test/tree/develop) | [![Build Status](https://travis-ci.org/boostorg/test.svg?branch=develop)](https://travis-ci.org/boostorg/test) | [![Build status](https://ci.appveyor.com/api/projects/status/nuihr6s92fjb9gwy/branch/develop?svg=true)](https://ci.appveyor.com/project/maintainer/test-xyzzy/branch/develop) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/99999/badge.svg)](https://scan.coverity.com/projects/boostorg-test) | [![codecov](https://codecov.io/gh/boostorg/test/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/test/branch/develop) | [![Deps](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/test.html) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](http://www.boost.org/doc/libs/develop/doc/html/test.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/test.html)

