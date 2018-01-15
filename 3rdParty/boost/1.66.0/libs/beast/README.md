<img width="880" height = "80" alt = "Boost.Beast Title"
    src="https://raw.githubusercontent.com/boostorg/beast/master/doc/images/readme2.png">

# HTTP and WebSocket built on Boost.Asio in C++11

Branch      | Linux/OSX | Windows | Coverage | Documentation | Matrix
------------|-----------|---------|----------|---------------|--------
[master](https://github.com/boostorg/beast/tree/master)   | [![Build Status](https://travis-ci.org/boostorg/beast.svg?branch=master)](https://travis-ci.org/boostorg/beast)  | [![Build status](https://ci.appveyor.com/api/projects/status/g0llpbvhpjuxjnlw/branch/master?svg=true)](https://ci.appveyor.com/project/vinniefalco/beast/branch/master)   | [![codecov](https://codecov.io/gh/boostorg/Beast/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/beast/branch/master)   | [![Documentation](https://img.shields.io/badge/documentation-master-brightgreen.svg)](http://www.boost.org/doc/libs/master/libs/beast/doc/html/index.html)  | [![Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/beast.html)
[develop](https://github.com/boostorg/beast/tree/develop) | [![Build Status](https://travis-ci.org/boostorg/beast.svg?branch=develop)](https://travis-ci.org/boostorg/beast) | [![Build status](https://ci.appveyor.com/api/projects/status/g0llpbvhpjuxjnlw/branch/develop?svg=true)](https://ci.appveyor.com/project/vinniefalco/beast/branch/develop) | [![codecov](https://codecov.io/gh/boostorg/Beast/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/beast/branch/develop) | [![Documentation](https://img.shields.io/badge/documentation-develop-brightgreen.svg)](http://www.boost.org/doc/libs/develop/libs/beast/index.html) | [![Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/beast.html)

## Contents

- [Introduction](#introduction)
- [Appearances](#appearances)
- [Description](#description)
- [Requirements](#requirements)
- [Building](#building)
- [Usage](#usage)
- [License](#license)
- [Contact](#contact)
- [Contributing](#Contributing)

## Introduction

Beast is a C++ header-only library serving as a foundation for writing
interoperable networking libraries by providing **low-level HTTP/1,
WebSocket, and networking protocol** vocabulary types and algorithms
using the consistent asynchronous model of Boost.Asio.

This library is designed for:

* **Symmetry:** Algorithms are role-agnostic; build clients, servers, or both.

* **Ease of Use:** Boost.Asio users will immediately understand Beast.

* **Flexibility:** Users make the important decisions such as buffer or
  thread management.

* **Performance:** Build applications handling thousands of connections or more.

* **Basis for Further Abstraction.** Components are well-suited for building upon.

## Appearances

| <a href="https://raw.githubusercontent.com/vinniefalco/CppCon2017/master/Make%20Classes%20Great%20Again%20-%20Vinnie%20Falco%20-%20CppCon%202017.pdf">CppCon 2017</a> | <a href="http://cppcast.com/2017/01/vinnie-falco/">CppCast 2017</a> | <a href="https://raw.githubusercontent.com/vinniefalco/BeastAssets/master/CppCon2016.pdf">CppCon 2016</a> |
| ------------ | ------------ | ----------- |
| <a href="https://www.youtube.com/watch?v=WsUnnYEKPnI"><img width="320" height = "180" alt="Beast" src="https://raw.githubusercontent.com/vinniefalco/CppCon2017/master/CppCon2017.png"></a> | <a href="http://cppcast.com/2017/01/vinnie-falco/"><img width="180" height="180" alt="Vinnie Falco" src="https://avatars1.githubusercontent.com/u/1503976?v=3&u=76c56d989ef4c09625256662eca2775df78a16ad&s=180"></a> | <a href="https://www.youtube.com/watch?v=uJZgRcvPFwI"><img width="320" height = "180" alt="Beast" src="https://raw.githubusercontent.com/vinniefalco/BeastAssets/master/CppCon2016.png"></a> |

## Description

This software is in its first official release. Interfaces
may change in response to user feedback. For recent changes
see the [CHANGELOG](CHANGELOG.md).

* [Official Site](https://github.com/boostorg/beast)
* [Documentation](http://www.boost.org/doc/libs/master/libs/beast/) (master branch)
* [Autobahn|Testsuite WebSocket Results](https://vinniefalco.github.io/boost/beast/reports/autobahn/index.html)

## Requirements

This library is for programmers familiar with Boost.Asio. Users
who wish to use asynchronous interfaces should already know how to
create concurrent network programs using callbacks or coroutines.

* **C++11:** Robust support for most language features.
* **Boost:** Boost.Asio and some other parts of Boost.
* **OpenSSL:** Optional, for using TLS/Secure sockets.

When using Microsoft Visual C++, Visual Studio 2015 Update 3 or later is required.

One of these components is required in order to build the tests and examples:

* Properly configured bjam/b2
* CMake 3.5.1 or later (Windows only)

## Building

Beast is header-only. To use it just add the necessary `#include` line
to your source files, like this:
```C++
#include <boost/beast.hpp>
```

To build your program successfully, you'll need to add the Boost.System
library to link with. If you use coroutines you'll also need to link
with the Boost.Coroutine library. Please visit the Boost documentation
for instructions on how to do this for your particular build system.

To build the documentation, examples, tests, and benchmarks it is
necessary to first obtain the boost "superproject" along with all
of the boost libraries. Instructions for doing so may be found on
the [Boost Wiki](https://github.com/boostorg/boost/wiki/Getting-Started).
These commamnds will build the programs and documentation that come
with Beast (omit the cxxflags parameter when building using MSVC):

```
cd boost   # The directory containing the boost superproject and libraries
b2 libs/beast/test cxxflags="-std=c++11"    # bjam must be in your $PATH
b2 libs/beast/example cxxflags="-std=c++11"
b2 libs/beast/doc
```

On Windows platforms only, CMake may be used to generate a Visual Studio
solution and a set of Visual Studio project files using these commands:

```
cd boost   # The directory containing the boost superproject and libraries
cd libs/beast
mkdir bin
cd bin
cmake ..                                    # for 32-bit Windows builds, or
cmake -G"Visual Studio 14 2015 Win64" ..    # for 64-bit Windows builds (VS2015), or
cmake -G"Visual Studio 15 2017 Win64" ..    # for 64-bit Windows builds (VS2017)
```

The files in the repository are laid out thusly:

```
./
    bin/            Create this to hold executables and project files
    bin64/          Create this to hold 64-bit Windows executables and project files
    doc/            Source code and scripts for the documentation
    include/        Where the header files live
    extras/         Additional APIs, may change
    example/        Self contained example programs
    meta/           Metadata for Boost integration
    scripts/        Small scripts used with CI systems
    test/           Unit tests
```

## Usage

These examples are complete, self-contained programs that you can build
and run yourself (they are in the `example` directory).

http://www.boost.org/doc/libs/develop/libs/beast/doc/html/beast/quick_start.html

## License

Distributed under the Boost Software License, Version 1.0.
(See accompanying file [LICENSE_1_0.txt](LICENSE_1_0.txt) or copy at
http://www.boost.org/LICENSE_1_0.txt)

## Contact

Please report issues or questions here:
https://github.com/boostorg/beast/issues

---

## Contributing (We Need Your Help!)

If you would like to contribute to Beast and help us maintain high
quality, consider performing code reviews on active pull requests.
Any feedback from users and stakeholders, even simple questions about
how things work or why they were done a certain way, carries value
and can be used to improve the library. Code review provides these
benefits:

* Identify bugs
* Documentation proof-reading
* Adjust interfaces to suit use-cases
* Simplify code

You can look through the Closed pull requests to get an idea of how
reviews are performed. To give a code review just sign in with your
GitHub account and then add comments to any open pull requests below,
don't be shy!
<p>https://github.com/boostorg/beast/pulls</p>

Here are some resources to learn more about
code reviews:

* <a href="https://blog.scottnonnenberg.com/top-ten-pull-request-review-mistakes/">Top 10 Pull Request Review Mistakes</a>
* <a href="https://smartbear.com/SmartBear/media/pdfs/best-kept-secrets-of-peer-code-review.pdf">Best Kept Secrets of Peer Code Review (pdf)</a>
* <a href="http://support.smartbear.com/support/media/resources/cc/11_Best_Practices_for_Peer_Code_Review.pdf">11 Best Practices for Peer Code Review (pdf)</a>
* <a href="http://www.evoketechnologies.com/blog/code-review-checklist-perform-effective-code-reviews/">Code Review Checklist – To Perform Effective Code Reviews</a>
* <a href="https://www.codeproject.com/Articles/524235/Codeplusreviewplusguidelines">Code review guidelines</a>
* <a href="https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md">C++ Core Guidelines</a>
* <a href="https://doc.lagout.org/programmation/C/CPP101.pdf">C++ Coding Standards (Sutter & Andrescu)</a>

Beast thrives on code reviews and any sort of feedback from users and
stakeholders about its interfaces. Even if you just have questions,
asking them in the code review or in issues provides valuable information
that can be used to improve the library - do not hesitate, no question
is insignificant or unimportant!
