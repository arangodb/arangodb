# Boost PolyCollection library

Branch   | Travis | AppVeyor | Regression tests
---------|--------|----------|-----------------
develop  | [![Build Status](https://travis-ci.org/boostorg/poly_collection.svg?branch=develop)](https://travis-ci.org/boostorg/poly_collection) | [![Build Status](https://ci.appveyor.com/api/projects/status/github/boostorg/poly_collection?branch=develop&svg=true)](https://ci.appveyor.com/project/joaquintides/poly-collection) | [![Test Results](./test_results.svg)](https://www.boost.org/development/tests/develop/developer/poly_collection.html)
master   | [![Build Status](https://travis-ci.org/boostorg/poly_collection.svg?branch=master)](https://travis-ci.org/boostorg/poly_collection) | [![Build Status](https://ci.appveyor.com/api/projects/status/github/boostorg/poly_collection?branch=master&svg=true)](https://ci.appveyor.com/project/joaquintides/poly-collection) | [![Test Results](./test_results.svg)](https://www.boost.org/development/tests/master/developer/poly_collection.html)

**Boost.PolyCollection**: fast containers of polymorphic objects.

[Online docs](http://boost.org/libs/poly_collection)  
[Seminal article at bannalia.blogspot.com](http://bannalia.blogspot.com/2014/05/fast-polymorphic-collections.html)

Typically, polymorphic objects cannot be stored *directly* in regular containers
and need be accessed through an indirection pointer, which introduces performance
problems related to CPU caching and branch prediction. Boost.PolyCollection
implements a
[novel data structure](http://www.boost.org/doc/html/poly_collection/an_efficient_polymorphic_data_st.html)
that is able to contiguously store polymorphic objects without such indirection,
thus providing a value-semantics user interface and better performance.
Three *polymorphic collections* are provided:

* [`boost::base_collection`](http://www.boost.org/doc/html/poly_collection/tutorial.html#poly_collection.tutorial.basics.boost_base_collection) 
* [`boost::function_collection`](http://www.boost.org/doc/html/poly_collection/tutorial.html#poly_collection.tutorial.basics.boost_function_collection)
* [`boost::any_collection`](http://www.boost.org/doc/html/poly_collection/tutorial.html#poly_collection.tutorial.basics.boost_any_collection)

dealing respectively with classic base/derived or OOP polymorphism, function wrapping
in the spirit of `std::function` and so-called
[*duck typing*](https://en.wikipedia.org/wiki/Duck_typing) as implemented by
[Boost.TypeErasure](http://www.boost.org/libs/type_erasure).

## Requirements

Boost.PolyCollection is a header-only library. C++11 support is required. The library has been verified to work with Visual Studio 2015, GCC 4.8 and Clang 3.3.
