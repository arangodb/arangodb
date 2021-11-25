Boost Multiprecision Library
============================

>ANNOUNCEMENT: Support for C++03 is now removed from this library.  Any attempt to build with a non C++11 conforming compiler is doomed to failure.

|                  |  Master  |   Develop   |
|------------------|----------|-------------|
| Drone            |  [![Build Status](https://drone.cpp.al/api/badges/boostorg/multiprecision/status.svg?ref=refs/heads/master)](https://drone.cpp.al/boostorg/multiprecision) | [![Build Status](https://drone.cpp.al/api/badges/boostorg/multiprecision/status.svg)](https://drone.cpp.al/boostorg/multiprecision) |
| Github Actions | [![Build Status](https://github.com/boostorg/multiprecision/workflows/multiprecision/badge.svg?branch=master)](https://github.com/boostorg/multiprecision/actions) | [![Build Status](https://github.com/boostorg/multiprecision/workflows/multiprecision/badge.svg?branch=develop)](https://github.com/boostorg/multiprecision/actions) |

 The Multiprecision Library provides integer, rational, floating-point, complex and interval number types in C++ that have more range and 
 precision than C++'s ordinary built-in types. The big number types in Multiprecision can be used with a wide selection of basic 
 mathematical operations, elementary transcendental functions as well as the functions in Boost.Math. The Multiprecision types can 
 also interoperate with the built-in types in C++ using clearly defined conversion rules. This allows Boost.Multiprecision to be 
 used for all kinds of mathematical calculations involving integer, rational and floating-point types requiring extended range and precision.

Multiprecision consists of a generic interface to the mathematics of large numbers as well as a selection of big number back ends, with 
support for integer, rational and floating-point types. Boost.Multiprecision provides a selection of back ends provided off-the-rack in 
including interfaces to GMP, MPFR, MPIR, TomMath as well as its own collection of Boost-licensed, header-only back ends for integers, 
rationals, floats and complex. In addition, user-defined back ends can be created and used with the interface of Multiprecision, provided the class implementation adheres to the necessary concepts.

Depending upon the number type, precision may be arbitrarily large (limited only by available memory), fixed at compile time 
(for example 50 or 100 decimal digits), or a variable controlled at run-time by member functions. The types are expression-template-enabled 
for better performance than naive user-defined types. 

The full documentation is available on [boost.org](http://www.boost.org/doc/libs/release/libs/multiprecision/index.html).

## Support, bugs and feature requests ##

Bugs and feature requests can be reported through the [Gitub issue tracker](https://github.com/boostorg/multiprecision/issues)
(see [open issues](https://github.com/boostorg/multiprecision/issues) and
[closed issues](https://github.com/boostorg/multiprecision/issues?utf8=%E2%9C%93&q=is%3Aissue+is%3Aclosed)).

You can submit your changes through a [pull request](https://github.com/boostorg/multiprecision/pulls).

There is no mailing-list specific to Boost Multiprecision, although you can use the general-purpose Boost [mailing-list](http://lists.boost.org/mailman/listinfo.cgi/boost-users) using the tag [multiprecision].


## Development ##

Clone the whole boost project, which includes the individual Boost projects as submodules ([see boost+git doc](https://github.com/boostorg/boost/wiki/Getting-Started)): 

    git clone https://github.com/boostorg/boost
    cd boost
    git submodule update --init

The Boost Multiprecision Library is located in `libs/multiprecision/`. 

### Running tests ###
First, make sure you are in `libs/multiprecision/test`. 
You can either run all the tests listed in `Jamfile.v2` or run a single test:

    ../../../b2                        <- run all tests
    ../../../b2 test_complex           <- single test

