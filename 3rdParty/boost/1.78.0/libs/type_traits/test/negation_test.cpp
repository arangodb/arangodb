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
#include <boost/type_traits/negation.hpp>
#endif
#include "check_integral_constant.hpp"

template<int V>
struct Int {
    static const int value = V;
};

TT_TEST_BEGIN(negation)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::negation<Int<5> >::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::negation<Int<0> >::value, true);

TT_TEST_END
