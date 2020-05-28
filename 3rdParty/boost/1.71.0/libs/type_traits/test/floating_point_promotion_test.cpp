
//  (C) Copyright John Maddock 2010. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/floating_point_promotion.hpp>
#endif
#include <boost/type_traits/is_same.hpp>
#include "test.hpp"
#include "check_integral_constant.hpp"

TT_TEST_BEGIN(floating_point_promotion)

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<void>::type, void>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<void const>::type, void const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<void volatile>::type, void volatile>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<void const volatile>::type, void const volatile>::value), true);

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float>::type, double>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float const>::type, double const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float volatile>::type, double volatile>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float const volatile>::type, double const volatile>::value), true);

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<double>::type, double>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<double const>::type, double const>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<double volatile>::type, double volatile>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<double const volatile>::type, double const volatile>::value), true);

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float&>::type, float&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float const&>::type, float const&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float volatile&>::type, float volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_same< ::tt::floating_point_promotion<float const volatile&>::type, float const volatile&>::value), true);

TT_TEST_END
