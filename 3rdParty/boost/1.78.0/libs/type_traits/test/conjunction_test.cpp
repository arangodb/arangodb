/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt
or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifdef TEST_STD
#include <type_traits>
#else
#include <boost/type_traits/conjunction.hpp>
#endif
#include "check_integral_constant.hpp"

template<int V>
struct Int {
    static const int value = V;
};

TT_TEST_BEGIN(conjunction)

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::conjunction<Int<2>, Int<4> >::value), 4);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::conjunction<Int<0>, Int<4> >::value), 0);

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::conjunction<Int<2>, Int<4>, Int<6>,
    Int<8>, Int<10> >::value), 10);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::conjunction<Int<2>, Int<0>, Int<4>,
    Int<6>, Int<8> >::value), 0);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::conjunction<Int<4> >::value, 4);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::conjunction<>::value, true);
#endif

TT_TEST_END
