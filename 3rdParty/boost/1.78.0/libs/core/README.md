Boost.Core
==========

Boost.Core, part of collection of the [Boost C++ Libraries](https://github.com/boostorg), is a collection of core utilities used by other Boost libraries.
The criteria for inclusion is that the utility component be:

* simple,
* used by other Boost libraries, and
* not dependent on any other Boost modules except Core itself, Config, Assert, Static Assert, or Predef.

### Build Status

Branch   | GitHub Actions | AppVeyor | Test Matrix | Dependencies |
---------|----------------|--------- | ----------- | ------------ |
Develop  | [![GitHub Actions](https://github.com/boostorg/core/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/boostorg/filesystem/actions?query=branch%3Adevelop) | [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/boostorg/core?branch=develop&svg=true)](https://ci.appveyor.com/project/pdimov/core) | [![Tests](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/core.html) | [![Dependencies](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/core.html)
Master   | [![GitHub Actions](https://github.com/boostorg/core/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/boostorg/filesystem/actions?query=branch%3Amaster) | [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/boostorg/core?branch=master&svg=true)](https://ci.appveyor.com/project/pdimov/core) | [![Tests](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/core.html) | [![Dependencies](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/core.html)

### Directories

* **doc** - Documentation of the components
* **include** - Interface headers
* **test** - Unit tests

### More information

* [Documentation](https://boost.org/libs/core)
* [Report bugs](https://svn.boost.org/trac/boost/newticket?component=core;version=Boost%20Release%20Branch). Be sure to mention Boost version, platform and compiler you're using. A small compilable code sample to reproduce the problem is always good as well.
* Submit your patches as pull requests against **develop** branch. Note that by submitting patches you agree to license your modifications under the [Boost Software License, Version 1.0](https://www.boost.org/LICENSE_1_0.txt).

### License

Distributed under the [Boost Software License, Version 1.0](https://boost.org/LICENSE_1_0.txt).
