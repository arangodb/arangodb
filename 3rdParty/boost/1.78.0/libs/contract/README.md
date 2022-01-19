Boost.Contract
==============

Contract programming for C++.
All contract programming features are supported: Subcontracting, class invariants (also static and volatile), postconditions (with old and return values), preconditions, customizable actions on assertion failure (e.g., terminate or throw), optional compilation and checking of assertions, disable assertions while already checking other assertions (to avoid infinite recursion), etc.

### License

Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).

### Properties

* C++11 (C++03 possible but not recommended without lambda functions and variadic macros, see documentation for more information).
* Shared Library / DLL with `BOOST_CONTRACT_DYN_LINK` (static library with `BOOST_CONTRACT_STATIC_LINK`, header-only also possible but not recommended, see `BOOST_CONTRACT_HEADER_ONLY` documentation for more information).

### Build Status

Branch          | Travis | Appveyor | Coverity Scan | codecov.io | Deps | Docs | Tests |
:-------------: | ------ | -------- | ------------- | ---------- | ---- | ---- | ----- |
[`master`](https://github.com/boostorg/contract/tree/master) | [![Build Status](https://travis-ci.com/boostorg/contract.svg?branch=master)](https://travis-ci.com/boostorg/contract) | [![Build status](https://ci.appveyor.com/api/projects/status/FILL-ME-IN/branch/master?svg=true)](https://ci.appveyor.com/project/lcaminiti/contract/branch/master) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/18555/badge.svg)](https://scan.coverity.com/projects/boostorg-contract) | [![codecov](https://codecov.io/gh/boostorg/contract/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/contract/branch/master)| [![Deps](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/contract.html) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](https://www.boost.org/doc/libs/master/libs/contract/doc/html/index.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/contract.html)
[`develop`](https://github.com/boostorg/contract/tree/develop) | [![Build Status](https://travis-ci.com/boostorg/contract.svg?branch=develop)](https://travis-ci.com/boostorg/contract) | [![Build status](https://ci.appveyor.com/api/projects/status/FILL-ME-IN/branch/develop?svg=true)](https://ci.appveyor.com/project/lcaminiti/contract/branch/develop) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/18555/badge.svg)](https://scan.coverity.com/projects/boostorg-contract) | [![codecov](https://codecov.io/gh/boostorg/contract/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/contract/branch/develop) | [![Deps](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/contract.html) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](https://www.boost.org/doc/libs/develop/libs/contract/doc/html/index.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/contract.html)

### Directories

| Name        | Purpose                        |
| ----------- | ------------------------------ |
| `build`     | Build                          |
| `doc`       | Documentation                  |
| `example`   | Examples                       |
| `include`   | Header code                    |
| `meta`      | Integration with Boost         |
| `src`       | Source code                    |
| `test`      | Unit tests                     |

### More Information

* [Ask questions](http://stackoverflow.com/questions/ask?tags=c%2B%2B,boost,boost-contract).
* [Report bugs](https://github.com/boostorg/contract/issues): Be sure to mention Boost version, platform and compiler you are using. A small compilable code sample to reproduce the problem is always good as well.
* Submit your patches as pull requests against **develop** branch. Note that by submitting patches you agree to license your modifications under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).
* Discussions about the library are held on the [Boost developers mailing list](http://www.boost.org/community/groups.html#main). Be sure to read the [discussion policy](http://www.boost.org/community/policy.html) before posting and add the `[contract]` text at the beginning of the subject line.

