![Boost Generic Image Library (GIL)](https://raw.githubusercontent.com/boostorg/gil/develop/doc/_static/gil.png)

[![Language](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-BSL-blue.svg)](https://opensource.org/licenses/BSL-1.0)
[![Documentation](https://img.shields.io/badge/gil-documentation-blue.svg)](http://boostorg.github.com/gil/)
[![Wiki](https://img.shields.io/badge/gil-wiki-blue.svg)](https://github.com/boostorg/gil/wiki)
[![Mailing List](https://img.shields.io/badge/gil-mailing%20list-4eb899.svg)](https://lists.boost.org/mailman/listinfo.cgi/boost-gil)
[![Gitter](https://img.shields.io/badge/gil-chat%20on%20gitter-4eb899.svg)](https://gitter.im/boostorg/gil)
[![Try it online](https://img.shields.io/badge/on-wandbox-blue.svg)](https://wandbox.org/permlink/isNgnMuqWcqTqzy7)
[![Conan](https://img.shields.io/badge/on-conan-blue.svg)](https://bintray.com/bincrafters/public-conan/boost_gil%3Abincrafters)
[![Vcpkg](https://img.shields.io/badge/on-vcpkg-blue.svg)](https://github.com/Microsoft/vcpkg/tree/master/ports/boost-gil)

   <br />  | AppVeyor        | Azure Pipelines | Travis CI       | CircleCI        | Regression
-----------|-----------------|-----------------|-----------------|-----------------|------------
**develop**| [![AppVeyor](https://ci.appveyor.com/api/projects/status/w4k19d8io2af168h/branch/develop?svg=true)](https://ci.appveyor.com/project/stefanseefeld/gil/branch/develop) | [![Azure](https://dev.azure.com/boostorg/gil/_apis/build/status/boostorg.gil?branchName=develop)](https://dev.azure.com/boostorg/gil/_build/latest?definitionId=4?branchName=develop) | [![Travis](https://travis-ci.org/boostorg/gil.svg?branch=develop)](https://travis-ci.org/boostorg/gil) | [![CircleCI](https://circleci.com/gh/boostorg/gil/tree/develop.svg?style=shield)](https://circleci.com/gh/boostorg/workflows/gil/tree/develop) | [![gil](https://img.shields.io/badge/gil-develop-blue.svg)](http://www.boost.org/development/tests/develop/developer/gil.html)
**master** | [![AppVeyor](https://ci.appveyor.com/api/projects/status/w4k19d8io2af168h?svg=true)](https://ci.appveyor.com/project/stefanseefeld/gil/branch/master) | [![Azure](https://dev.azure.com/boostorg/gil/_apis/build/status/boostorg.gil?branchName=master)](https://dev.azure.com/boostorg/gil/_build/latest?definitionId=4?branchName=master) | [![Travis](https://travis-ci.org/boostorg/gil.svg?branch=master)](https://travis-ci.org/boostorg/gil) | [![CircleCI](https://circleci.com/gh/boostorg/gil/tree/master.svg?style=shield)](https://circleci.com/gh/boostorg/workflows/gil/tree/master) | [![gil](https://img.shields.io/badge/gil-master-blue.svg)](http://www.boost.org/development/tests/master/developer/gil.html)
 msvc++    | VS2017 | VS2017, VS2015 |   |   |
 clang     |   | Xcode 9.4.1 | 3.9, 5.0, Xcode 9.4.1 | 3.9, 4.0, 5.0 |
 gcc       |   | 5.4, 8.1 | 5.5, 6.5, 7.4 | 4.8-9, 5.1-5, 6.1-4, 7.1-3, 8.2 |
 C++       | 14 | 11, 14, 17 | 11 | 11 |
 <br />    | Boost.Build, CMake | CMake, Boost 1.68 | Boost.Build, UBSan | Boost.Build |

# Boost.GIL

- [Introduction](#introduction)
- [Documentation](#documentation)
- [Requirements](#requirements)
- [Branches](#branches)
- [Community](#community)
- [Contributing](#contributing-we-need-your-help)
- [License](#license)

## Introduction

Boost.GIL is a part of the [Boost C++ Libraries](http://github.com/boostorg).

The Boost Generic Image Library (GIL) is a C++ library that abstracts image
representations from algorithms and allows writing code that can work on a
variety of images with performance similar to hand-writing for a specific image type.

## Documentation

- Latest release: https://boost.org/libs/gil
- Development: http://boostorg.github.io/gil/

See [CONTRIBUTING.md](CONTRIBUTING.md) for instructions about how to build and
run tests, examples.

See [example/README.md](example/README.md) for available GIL examples.

## Requirements

**NOTE:** The library source code is currently being modernized for C++11.

The Boost Generic Image Library (GIL) requires:

- C++11 compiler
- Boost header-only libraries

Optionally, in order to build and run tests and examples:

- Boost.Filesystem
- Boost.Test

## Branches

The official repository contains the following branches:

* [**master**](https://github.com/boostorg/gil/tree/master) This
  holds the most recent snapshot with code that is known to be stable.

* [**develop**](https://github.com/boostorg/gil/tree/develop) This
  holds the most recent snapshot. It may contain unstable code.

## Community

There is number of communication channels to ask questions and discuss Boost.GIL issues:

- Mailing list [boost-gil](https://lists.boost.org/mailman/listinfo.cgi/boost-gil) ([archive](https://lists.boost.org/boost-gil/)) as well as official Boost lists, [boost-users](https://lists.boost.org/mailman/listinfo.cgi/boost-users) and
[boost](https://lists.boost.org/mailman/listinfo.cgi/boost).
- Gitter chat room [boostorg//gil](https://gitter.im/boostorg/gil).
- [cpplang.slack.com](https://cpplang.slack.com) chat rooms [#boost_user](https://cpplang.slack.com/messages/CEWTCFDN0/) and [#boost](https://cpplang.slack.com/messages/C27KZLB0X/).
- IRC channel [#boost](irc://chat.freenode.net/#osgeo-geos) on FreeNode.
- You can also ask questions via GitHub issue.

## Contributing (We Need Your Help!)

If you would like to contribute to Boost.GIL, help us improve the library
and maintain high quality, there is number of ways to do it.

If you would like to test the library, contribute new feature or a bug fix,
see the [CONTRIBUTING.md](CONTRIBUTING.md) where the whole development
infrastructure and the contributing workflow is explained in details.

You may consider performing code reviews on active
[pull requests](https://github.com/boostorg/gil/pulls) or help
with solving reported issues, especially those labelled with:

- [status/need-help](https://github.com/boostorg/gil/labels/status%2Fneed-help)
- [status/need-feedback](https://github.com/boostorg/gil/labels/status%2Fneed-feedback)
- [need-minimal-example ](https://github.com/boostorg/gil/labels/status%2Fneed-minimal-example)

Any feedback from users and developers, even simple questions about how things work
or why they were done a certain way, carries value and can be used to improve the library.

## License

Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).
