Boost Math Library [![Build Status](https://travis-ci.org/boostorg/math.svg?branch=develop)](https://travis-ci.org/boostorg/math)
==================

This library is divided into several interconnected parts:

### Floating Point Utilities

Utility functions for dealing with floating point arithmetic, includes functions for floating point classification (fpclassify, isnan, isinf etc), sign manipulation, rounding, comparison, and computing the distance between floating point numbers.

### Specific Width Floating Point Types

A set of typedefs similar to those provided by <cstdint> but for floating point types.

### Mathematical Constants

A wide range of constants ranging from various multiples of π, fractions, Euler's constant, etc.

These are of course usable from template code, or as non-templates with a simplified interface if that is more appropriate.

### Statistical Distributions

Provides a reasonably comprehensive set of statistical distributions, upon which higher level statistical tests can be built.

The initial focus is on the central univariate distributions. Both continuous (like normal & Fisher) and discrete (like binomial & Poisson) distributions are provided.

A comprehensive tutorial is provided, along with a series of worked examples illustrating how the library is used to conduct statistical tests.

### Special Functions

Provides a small number of high quality special functions; initially these were concentrated on functions used in statistical applications along with those in the Technical Report on C++ Library Extensions.

The function families currently implemented are the gamma, beta & error functions along with the incomplete gamma and beta functions (four variants of each) and all the possible inverses of these, plus the digamma, various factorial functions, Bessel functions, elliptic integrals, sinus cardinals (along with their hyperbolic variants), inverse hyperbolic functions, Legrendre/Laguerre/Hermite/Chebyshev polynomials and various special power and logarithmic functions.

All the implementations are fully generic and support the use of arbitrary "real-number" types, including Boost.Multiprecision, although they are optimised for use with types with known significand (or mantissa) sizes: typically float, double or long double.

These functions also provide the basis of support for the TR1 special functions.

### Root Finding and Function Minimisation

A comprehensive set of root-finding algorithms over the real line, both with derivatives and derivative free.

Also function minimisation via Brent's Method.

### Polynomials and Rational Functions

Tools for manipulating polynomials and for efficient evaluation of rationals or polynomials.

### Interpolation

Function interpolation via barycentric rational interpolation, compactly supported cubic B-splines, and the Chebyshev transform.

### Numerical Integration and Differentiation

A reasonably comprehensive set of routines for integration (trapezoidal, Gauss-Legendre, Gauss-Kronrod, Gauss-Chebyshev, double-exponential, and Monte-Carlo) and differentiation (Chebyshev transform, finite difference, and the complex step derivative).

The integration routines are usable for functions returning complex results - and hence can be used for computation of  contour integrals.

### Quaternions and Octonions

Quaternion and Octonians as class templates similar to std::complex.

The full documentation is available on [boost.org](http://www.boost.org/doc/libs/release/libs/math).

|                  |  Master  |   Develop   |
|------------------|----------|-------------|
| Travis           | [![Build Status](https://travis-ci.org/boostorg/math.svg?branch=master)](https://travis-ci.org/boostorg/math)  |  [![Build Status](https://travis-ci.org/boostorg/math.svg)](https://travis-ci.org/boostorg/math) |
| Appveyor         | [![Build status](https://ci.appveyor.com/api/projects/status/cnugjx9dt7cou7nj/branch/master?svg=true)](https://ci.appveyor.com/project/jzmaddock/math/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/cnugjx9dt7cou7nj/branch/develop?svg=true)](https://ci.appveyor.com/project/jzmaddock/math/branch/develop)  |



## Support, bugs and feature requests ##

Bugs and feature requests can be reported through the [Gitub issue tracker](https://github.com/boostorg/math/issues)
(see [open issues](https://github.com/boostorg/math/issues) and
[closed issues](https://github.com/boostorg/math/issues?utf8=%E2%9C%93&q=is%3Aissue+is%3Aclosed)).

You can submit your changes through a [pull request](https://github.com/boostorg/math/pulls).

There is no mailing-list specific to Boost Math, although you can use the general-purpose Boost [mailing-list](http://lists.boost.org/mailman/listinfo.cgi/boost-users) using the tag [math].


## Development ##

Clone the whole boost project, which includes the individual Boost projects as submodules ([see boost+git doc](https://github.com/boostorg/boost/wiki/Getting-Started)):

    $ git clone https://github.com/boostorg/boost
    $ cd boost
    $ git submodule update --init

The Boost Math Library is located in `libs/math/`.

### Running tests ###
First, make sure you are in `libs/math/test`.
You can either run all the tests listed in `Jamfile.v2` or run a single test:

    test$ ../../../b2                        <- run all tests
    test$ ../../../b2 static_assert_test     <- single test
    test$ # A more advanced syntax, demoing various options for building the tests:
    test$ ../../../b2 -a -j2 -q --reconfigure toolset=clang cxxflags="--std=c++14 -fsanitize=address -fsanitize=undefined" linkflags="-fsanitize=undefined -fsanitize=address"
    
### Building documentation ###

Full instructions can be found [here](https://svn.boost.org/trac10/wiki/BoostDocs/GettingStarted), but to reiterate slightly:

```bash
libs/math/doc$ brew install docbook-xsl # on mac
libs/math/doc$ touch ~/user-config.jam
libs/math/doc$ # now edit so that:
libs/math/doc$ cat ~/user-config.jam
using darwin ;

using xsltproc ;

using boostbook
    : /usr/local/opt/docbook-xsl/docbook-xsl
    ;

using doxygen ;
using quickbook ;
libs/math/doc$ ../../../b2
```
