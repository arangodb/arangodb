# Boost.Variant2

This repository contains a never-valueless, strong guarantee, C++11/14/17
implementation of [std::variant](http://en.cppreference.com/w/cpp/utility/variant).
See [the documentation](https://www.boost.org/libs/variant2)
for more information.

The library is part of Boost, starting from release 1.71. It depends on
Boost.Mp11, Boost.Config, and Boost.Assert.

Supported compilers:

* g++ 4.8 or later with `-std=c++11` or above
* clang++ 3.9 or later with `-std=c++11` or above
* Visual Studio 2015, 2017, 2019

Tested on [Github Actions](https://github.com/boostorg/variant2/actions) and
[Appveyor](https://ci.appveyor.com/project/pdimov/variant2-fkab9).
