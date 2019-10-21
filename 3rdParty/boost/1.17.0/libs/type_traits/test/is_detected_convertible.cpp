/*
Copyright 2017-2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt
or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && \
    !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
#ifdef TEST_STD
#include <type_traits>
#else
#include <boost/type_traits/is_detected_convertible.hpp>
#endif
#include "check_integral_constant.hpp"
#include "check_type.hpp"

#define CHECK_FALSE(e) BOOST_CHECK_INTEGRAL_CONSTANT(e, false)
#define CHECK_TRUE(e) BOOST_CHECK_INTEGRAL_CONSTANT(e, true)

template<class T>
using type_t = typename T::type;

struct has_type {
    using type = char;
};

struct no_type { };

TT_TEST_BEGIN(is_detected_convertible)

CHECK_FALSE((::tt::is_detected_convertible<long, type_t, int>::value));
CHECK_TRUE((::tt::is_detected_convertible<long, type_t, has_type>::value));
CHECK_FALSE((::tt::is_detected_convertible<long, type_t, no_type>::value));
#ifndef BOOST_NO_CXX14_VARIABLE_TEMPLATES
CHECK_FALSE((::tt::is_detected_convertible_v<long, type_t, int>));
CHECK_TRUE((::tt::is_detected_convertible_v<long, type_t, has_type>));
CHECK_FALSE((::tt::is_detected_convertible_v<long, type_t, no_type>));
#endif

TT_TEST_END
#else
int main()
{
    return 0;
}
#endif
