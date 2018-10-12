/*
Copyright 2017 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

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
#include <boost/type_traits/detected_or.hpp>
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

TT_TEST_BEGIN(detected_or)

CHECK_FALSE((::tt::detected_or<bool, type_t, int>::value_t::value));
CHECK_TRUE((::tt::detected_or<bool, type_t, has_type>::value_t::value));
CHECK_FALSE((::tt::detected_or<bool, type_t, no_type>::value_t::value));
BOOST_CHECK_TYPE4(::tt::detected_or<bool, type_t, int>::type, bool);
BOOST_CHECK_TYPE4(::tt::detected_or<bool, type_t, has_type>::type, char);
BOOST_CHECK_TYPE4(::tt::detected_or<bool, type_t, no_type>::type, bool);
BOOST_CHECK_TYPE4(::tt::detected_or_t<bool, type_t, int>, bool);
BOOST_CHECK_TYPE4(::tt::detected_or_t<bool, type_t, has_type>, char);
BOOST_CHECK_TYPE4(::tt::detected_or_t<bool, type_t, no_type>, bool);

TT_TEST_END
#else
int main()
{
    return 0;
}
#endif
