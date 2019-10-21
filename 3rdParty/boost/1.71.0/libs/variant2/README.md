# variant2

This repository contains a never-valueless, strong guarantee, C++11/14/17
implementation of [std::variant](http://en.cppreference.com/w/cpp/utility/variant).
See [the documentation](https://www.boost.org/doc/libs/master/libs/variant2/)
for more information.

The code requires [Boost.Mp11](https://github.com/boostorg/mp11) and
Boost.Config.

The repository is intended to be placed into the `libs/variant2` directory of
a Boost clone or release, but the header `variant.hpp` will also work
[standalone](https://godbolt.org/z/nVUNKX).

Supported compilers:

* g++ 4.8 or later with `-std=c++11` or above
* clang++ 3.5 or later with `-std=c++11` or above
* Visual Studio 2015, 2017, 2019

Tested on [Travis](https://travis-ci.org/boostorg/variant2/) and
[Appveyor](https://ci.appveyor.com/project/pdimov/variant2/).
