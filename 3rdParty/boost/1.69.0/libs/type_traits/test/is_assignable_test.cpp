
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_assignable.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS

struct non_assignable
{
   non_assignable();
   non_assignable& operator=(const non_assignable&) = delete;
};

#endif

#ifndef BOOST_NO_CXX11_NOEXCEPT

struct noexcept_assignable
{
   noexcept_assignable();
   noexcept_assignable& operator=(const non_assignable&)noexcept;
};

#endif

TT_TEST_BEGIN(is_assignable)

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<bool&, const bool&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<bool const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<bool volatile&, bool const volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<bool const volatile&>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<signed char&, const signed char&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<signed char const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<signed char volatile&, const signed char volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<signed char const volatile&>::value), false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned char&, const unsigned char&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<char&, const char&>::value), true);

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned short&, const unsigned short&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<short&, const short&>::value), true);

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned int&, const unsigned int&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int&, const int&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned int const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned int volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned int const volatile&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int const volatile&>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned long&, const unsigned long&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long&, const long&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned long const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned long volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<unsigned long const volatile&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long const volatile&>::value), false);
#endif

#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::ulong_long_type&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::long_long_type&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::ulong_long_type const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::long_long_type const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::ulong_long_type volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::long_long_type volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::ulong_long_type const volatile&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable< ::boost::long_long_type const volatile&>::value), false);
#endif
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<float&, const float&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<float const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<float volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<float const volatile&>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<double&, const double&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<double const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<double volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<double const volatile&>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long double&, const long double&>::value), true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long double const&>::value), false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long double volatile&>::value), true);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<long double const volatile&>::value), false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<void*&>::value), true);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int&, int&&>::value), true);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<const int&>::value), false);
//BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int(&)[2], const int(&)[2]>::value), true);
//BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int(&)[3][2]>::value), true);
//BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<int(&)[2][4][5][6][3]>::value), true);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<UDT&>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<empty_UDT&>::value), true, false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<void>::value), false);
// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<empty_POD_UDT&>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<POD_UDT&>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<POD_union_UDT&>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<empty_POD_union_UDT&>::value), true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<nothrow_assign_UDT&>::value), true, false);

BOOST_CHECK_SOFT_INTEGRAL_CONSTANT((::tt::is_assignable<test_abc1&>::value), true, false);

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<non_assignable&>::value), false);
#endif
#ifndef BOOST_NO_CXX11_NOEXCEPT
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_assignable<noexcept_assignable&>::value), true);
#endif

TT_TEST_END



