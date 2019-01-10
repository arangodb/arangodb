Format, part of the collection of [Boost C++ Libraries](http://github.com/boostorg), provides a type-safe mechanism for formatting arguments according to a printf-like format-string.  User-defined types are supported by providing a `std::ostream operator <<` implementation for them.

### License

Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).

### Properties

* C++03
* Header-only

### Build Status

Branch          | Travis | Appveyor | Coverity Scan | codecov.io | Deps | Docs | Tests |
:-------------: | ------ | -------- | ------------- | ---------- | ---- | ---- | ----- |
[`master`](https://github.com/boostorg/format/tree/master) | [![Build Status](https://travis-ci.org/boostorg/format.svg?branch=master)](https://travis-ci.org/boostorg/format) | [![Build status](https://ci.appveyor.com/api/projects/status/tkcumf8nu6tb697d/branch/master?svg=true)](https://ci.appveyor.com/project/jeking3/format-bhjc4/branch/master) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/14007/badge.svg)](https://scan.coverity.com/projects/boostorg-format) | [![codecov](https://codecov.io/gh/boostorg/format/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/format/branch/master) | [![Deps](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/format.html) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](http://www.boost.org/doc/libs/master/doc/html/format.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/format.html)
[`develop`](https://github.com/boostorg/format/tree/develop) | [![Build Status](https://travis-ci.org/boostorg/format.svg?branch=develop)](https://travis-ci.org/boostorg/format) | [![Build status](https://ci.appveyor.com/api/projects/status/tkcumf8nu6tb697d/branch/develop?svg=true)](https://ci.appveyor.com/project/jeking3/format-bhjc4/branch/develop) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/14007/badge.svg)](https://scan.coverity.com/projects/boostorg-format) | [![codecov](https://codecov.io/gh/boostorg/format/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/format/branch/develop) | [![Deps](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/format.html) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](http://www.boost.org/doc/libs/develop/doc/html/format.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/format.html)

### Directories

| Name        | Purpose                        |
| ----------- | ------------------------------ |
| `benchmark` | benchmark tool                 |
| `doc`       | documentation                  |
| `examples`  | use case examples              |
| `include`   | headers                        |
| `test`      | unit tests                     |
| `tools`     | development tools              |

### More information

* [Ask questions](http://stackoverflow.com/questions/ask?tags=c%2B%2B,boost,boost-format): Be sure to read the documentation first as Boost.Format, like any other string formatting library, has its own rules.
* [Report bugs](https://github.com/boostorg/format/issues): Be sure to mention Boost version, platform and compiler you're using. A small compilable code sample to reproduce the problem is always good as well.
* [Submit Pull Requests](https://github.com/boostorg/format/pulls) against the **develop** branch. Note that by submitting patches you agree to license your modifications under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).  Be sure to include tests proving your changes work properly.
* Discussions about the library are held on the [Boost developers mailing list](http://www.boost.org/community/groups.html#main). Be sure to read the [discussion policy](http://www.boost.org/community/policy.html) before posting and add the `[format]` tag at the beginning of the subject line.

