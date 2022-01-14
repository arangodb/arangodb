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
#include <boost/type_traits/disjunction.hpp>
#endif
#include "check_integral_constant.hpp"

template<int V>
struct Int {
    static const int value = V;
};

TT_TEST_BEGIN(disjunction)

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::disjunction<Int<2>, Int<4> >::value), 2);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::disjunction<Int<0>, Int<4> >::value), 4);

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::disjunction<Int<2>, Int<4>, Int<6>,
    Int<8>, Int<10> >::value), 2);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::disjunction<Int<0>, Int<0>, Int<6>,
    Int<0>, Int<0> >::value), 6);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::disjunction<Int<4> >::value, 4);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::disjunction<>::value, false);
#endif

TT_TEST_END
