Boost StaticAssert Library
============================

The Boost StaticAssert library provides static assertions for C++, this library is the ancestor to C++ native static_assert's and 
can be used on older compilers which don't have that feature.

The full documentation is available on [boost.org](http://www.boost.org/doc/libs/release/libs/static_assert).

## Support, bugs and feature requests ##

Bugs and feature requests can be reported through the [Gitub issue tracker](https://github.com/boostorg/static_assert/issues)
(see [open issues](https://github.com/boostorg/static_assert/issues) and
[closed issues](https://github.com/boostorg/static_assert/issues?utf8=%E2%9C%93&q=is%3Aissue+is%3Aclosed)).

You can submit your changes through a [pull request](https://github.com/boostorg/static_assert/pulls).

There is no mailing-list specific to Boost StaticAssert, although you can use the general-purpose Boost [mailing-list](http://lists.boost.org/mailman/listinfo.cgi/boost-users) using the tag [static_assert].


## Development ##

Clone the whole boost project, which includes the individual Boost projects as submodules ([see boost+git doc](https://github.com/boostorg/boost/wiki/Getting-Started)): 

    git clone https://github.com/boostorg/boost
    cd boost
    git submodule update --init

The Boost StaticAssert Library is located in `libs/static_assert/`. 

### Running tests ###
First, make sure you are in `libs/static_assert/test`. 
You can either run all the tests listed in `Jamfile.v2` or run a single test:

    ../../../b2                        <- run all tests
    ../../../b2 static_assert_test     <- single test

