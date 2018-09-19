
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_complete.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

TT_TEST_BEGIN(is_complete)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int volatile&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const volatile&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int volatile*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const volatile*>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int[2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const[3]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int volatile[2][3]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int const volatile[4][5][6]>::value, true);
#ifdef BOOST_TT_HAS_WORKING_IS_COMPLETE
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<int[]>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<enum_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<mf8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<union_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<test_abc1>::value, true);
#ifdef BOOST_TT_HAS_WORKING_IS_COMPLETE
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<incomplete_type>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<polymorphic_base>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<virtual_inherit6>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<foo0_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_complete<foo4_t>::value, true);

TT_TEST_END

