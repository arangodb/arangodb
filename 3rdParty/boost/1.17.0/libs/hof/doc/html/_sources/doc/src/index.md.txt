<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

About
=====

HigherOrderFunctions is a header-only C++11/C++14 library that provides utilities for functions and function objects, which can solve many problems with much simpler constructs than whats traditionally been done with metaprogramming.

HigherOrderFunctions is:

- Modern: HigherOrderFunctions takes advantages of modern C++11/C++14 features. It support both `constexpr` initialization and `constexpr` evaluation of functions. It takes advantage of type deduction, varidiac templates, and perfect forwarding to provide a simple and modern interface. 
- Relevant: HigherOrderFunctions provides utilities for functions and does not try to implement a functional language in C++. As such, HigherOrderFunctions solves many problems relevant to C++ programmers, including initialization of function objects and lambdas, overloading with ordering, improved return type deduction, and much more.
- Lightweight: HigherOrderFunctions builds simple lightweight abstraction on top of function objects. It does not require subscribing to an entire framework. Just use the parts you need.

HigherOrderFunctions is divided into three components:

* Function Adaptors and Decorators: These enhance functions with additional capability.
* Functions: These return functions that achieve a specific purpose.
* Utilities: These are general utilities that are useful when defining or using functions

Github: [https://github.com/pfultz2/higherorderfunctions/](https://github.com/pfultz2/higherorderfunctions/)

Documentation: [http://pfultz2.github.io/higherorderfunctions/doc/html/](http://pfultz2.github.io/higherorderfunctions/doc/html/)

Motivation
==========

- Improve the expressiveness and capabilities of functions, including first-class citzens for function overload set, extension methods, infix operators and much more.
- Simplify constructs in C++ that have generally required metaprogramming
- Enable point-free style programming
- Workaround the limitations of lambdas in C++14

Requirements
============

This requires a C++11 compiler. There are no third-party dependencies. This has been tested on clang 3.5-3.8, gcc 4.6-7, and Visual Studio 2015 and 2017.

Contexpr support
----------------

Both MSVC and gcc 4.6 have limited constexpr support due to many bugs in the implementation of constexpr. However, constexpr initialization of functions is supported when using the [`BOOST_HOF_STATIC_FUNCTION`](BOOST_HOF_STATIC_FUNCTION) and [`BOOST_HOF_STATIC_LAMBDA_FUNCTION`](BOOST_HOF_STATIC_LAMBDA_FUNCTION) constructs.

Noexcept support
----------------

On older compilers such as gcc 4.6 and gcc 4.7, `noexcept` is not used due to many bugs in the implementation. Also, most compilers don't support deducing `noexcept` with member function pointers. Only newer versions of gcc(4.9 and later) support this.
