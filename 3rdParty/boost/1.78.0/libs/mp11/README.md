# Mp11, a C++11 metaprogramming library

Mp11 is a C++11 metaprogramming library based on template aliases and variadic templates.
It implements the approach outlined in the article
["Simple C++11 metaprogramming"](https://www.boost.org/libs/mp11/doc/html/simple_cxx11_metaprogramming.html)
and [its sequel](https://www.boost.org/libs/mp11/doc/html/simple_cxx11_metaprogramming_2.html).

Mp11 is part of [Boost](http://boost.org/libs/mp11), starting with release 1.66.0. It
however has no Boost dependencies and can be used standalone, as a Git submodule, for
instance. For CMake users, `add_subdirectory` is supported, as is installation and
`find_package(boost_mp11)`.

## Supported compilers

* g++ 4.8 or later
* clang++ 3.9 or later
* Visual Studio 2013, 2015, 2017, 2019

Tested on [Github Actions](https://github.com/boostorg/mp11/actions) and [Appveyor](https://ci.appveyor.com/project/pdimov/mp11/).

## License

Distributed under the [Boost Software License, Version 1.0](http://boost.org/LICENSE_1_0.txt).
