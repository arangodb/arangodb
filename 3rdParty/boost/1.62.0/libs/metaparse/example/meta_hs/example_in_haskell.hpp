#ifndef EXAMPLE_IN_HASKELL_HPP
#define EXAMPLE_IN_HASKELL_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#ifdef BOOST_NO_CXX11_CONSTEXPR

// We have to fall back on the handcrafted one
#include <example_handcrafted.hpp>

#else

#define BOOST_MPL_LIMIT_STRING_SIZE 50
#define BOOST_METAPARSE_LIMIT_STRING_SIZE BOOST_MPL_LIMIT_STRING_SIZE

#include <meta_hs.hpp>
#include <double_number.hpp>

#include <boost/metaparse/string.hpp>

#include <boost/mpl/int.hpp>

#ifdef _STR
#  error _STR already defined
#endif
#define _STR BOOST_METAPARSE_STRING

typedef
  meta_hs
    ::import1<_STR("f"), double_number>::type
    ::import<_STR("val"), boost::mpl::int_<11> >::type

    ::define<_STR("fib n = if n<2 then 1 else fib(n-2) + fib(n-1)")>::type
    ::define<_STR("fact n = if n<1 then 1 else n * fact(n-1)")>::type
    ::define<_STR("times4 n = f (f n)")>::type
    ::define<_STR("times11 n = n * val")>::type
  metafunctions;

typedef metafunctions::get<_STR("fib")> fib;
typedef metafunctions::get<_STR("fact")> fact;
typedef metafunctions::get<_STR("times4")> times4;
typedef metafunctions::get<_STR("times11")> times11;

#endif

#endif


