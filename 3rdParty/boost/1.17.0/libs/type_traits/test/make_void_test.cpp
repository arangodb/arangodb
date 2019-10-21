/*
Copyright 2017 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt
or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifdef TEST_STD
#include <type_traits>
#else
#include <boost/type_traits/make_void.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"

TT_TEST_BEGIN(make_void)

BOOST_CHECK_TYPE(::tt::make_void<int>::type, void);
BOOST_CHECK_TYPE(::tt::make_void<const volatile int>::type, void);
BOOST_CHECK_TYPE(::tt::make_void<int&>::type, void);
BOOST_CHECK_TYPE(::tt::make_void<void>::type, void);
BOOST_CHECK_TYPE(::tt::make_void<int(*)(int)>::type, void);
BOOST_CHECK_TYPE(::tt::make_void<int[]>::type, void);
BOOST_CHECK_TYPE(::tt::make_void<int[1]>::type, void);

BOOST_CHECK_TYPE(::tt::make_void<>::type, void);
BOOST_CHECK_TYPE3(::tt::make_void<int, int>::type, void);

#ifndef BOOST_NO_CXX11_TEMPLATE_ALIASES
BOOST_CHECK_TYPE(::tt::void_t<int>, void);
BOOST_CHECK_TYPE(::tt::void_t<const volatile int>, void);
BOOST_CHECK_TYPE(::tt::void_t<int&>, void);
BOOST_CHECK_TYPE(::tt::void_t<void>, void);
BOOST_CHECK_TYPE(::tt::void_t<int(*)(int)>, void);
BOOST_CHECK_TYPE(::tt::void_t<int[]>, void);
BOOST_CHECK_TYPE(::tt::void_t<int[1]>, void);

BOOST_CHECK_TYPE(::tt::void_t<>, void);
BOOST_CHECK_TYPE3(::tt::void_t<int, int>, void);
#endif

TT_TEST_END
