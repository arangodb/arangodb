Boost TypeTraits Library
============================

The Boost type-traits library contains a set of very specific traits classes, each of which encapsulate a single trait 
from the C++ type system; for example, is a type a pointer or a reference type? Or does a type have a trivial constructor, or a const-qualifier?

The type-traits classes share a unified design: each class inherits from the type true_type if the type has the specified property and inherits from false_type otherwise.

The type-traits library also contains a set of classes that perform a specific transformation on a type; for example, they can remove a top-level const or 
volatile qualifier from a type. Each class that performs a transformation defines a single typedef-member type that is the result of the transformation. 

The full documentation is available on [boost.org](http://www.boost.org/doc/libs/release/libs/type_traits/index.html).

|                  |  Master  |   Develop   |
|------------------|----------|-------------|
| Travis           | [![Build Status](https://travis-ci.org/boostorg/type_traits.svg?branch=master)](https://travis-ci.org/boostorg/type_traits)  |  [![Build Status](https://travis-ci.org/boostorg/type_traits.svg)](https://travis-ci.org/boostorg/type_traits) |
| Appveyor         | [![Build status](https://ci.appveyor.com/api/projects/status/lwjqu4087qiolje8/branch/master?svg=true)](https://ci.appveyor.com/project/jzmaddock/type-traits/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/lwjqu4087qiolje8/branch/develop?svg=true)](https://ci.appveyor.com/project/jzmaddock/type-traits/branch/develop)  |


## Support, bugs and feature requests ##

Bugs and feature requests can be reported through the [Gitub issue tracker](https://github.com/boostorg/type_traits/issues)
(see [open issues](https://github.com/boostorg/type_traits/issues) and
[closed issues](https://github.com/boostorg/type_traits/issues?utf8=%E2%9C%93&q=is%3Aissue+is%3Aclosed)).

You can submit your changes through a [pull request](https://github.com/boostorg/type_traits/pulls).

There is no mailing-list specific to Boost TypeTraits, although you can use the general-purpose Boost [mailing-list](http://lists.boost.org/mailman/listinfo.cgi/boost-users) using the tag [type_traits].


## Development ##

Clone the whole boost project, which includes the individual Boost projects as submodules ([see boost+git doc](https://github.com/boostorg/boost/wiki/Getting-Started)): 

    git clone https://github.com/boostorg/boost
    cd boost
    git submodule update --init

The Boost TypeTraits Library is located in `libs/type_traits/`. 

### Running tests ###
First, make sure you are in `libs/type_traits/test`. 
You can either run all the tests listed in `Jamfile.v2` or run a single test:

    ../../../b2                        <- run all tests
    ../../../b2 config_info            <- single test

