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
#include <boost/type_traits/is_unscoped_enum.hpp>
#endif
#include "check_integral_constant.hpp"

enum Unscoped {
    Constant = 1
};

#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)
enum class Scoped {
    Constant = 2
};
#endif

TT_TEST_BEGIN(is_unscoped_enum)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_unscoped_enum<void>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_unscoped_enum<int>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_unscoped_enum<Unscoped>::value, true);

#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_unscoped_enum<Scoped>::value, false);
#endif

TT_TEST_END
