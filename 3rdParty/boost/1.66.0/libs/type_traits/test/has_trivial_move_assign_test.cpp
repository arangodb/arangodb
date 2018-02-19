
//  (C) Copyright John Maddock 2000. 
//  (C) Copyright Antony Polukhin 2013.
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_integral_constant.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/has_trivial_move_assign.hpp>
#endif

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS

struct non_copyable_movable
{
   int val;
   non_copyable_movable(int);
   non_copyable_movable(const non_copyable_movable&) = delete;
   non_copyable_movable& operator=(const non_copyable_movable&) = delete;
   //non_copyable_movable(non_copyable_movable&&) = default;
#if BOOST_WORKAROUND(BOOST_MSVC, <= 1800) || BOOST_WORKAROUND(BOOST_GCC_VERSION, < 40500)
   non_copyable_movable& operator=(non_copyable_movable&& o) 
   {
      val = std::move(o.val);
      return *this;
   }
#else
   non_copyable_movable& operator=(non_copyable_movable&&) = default;
#endif
};

struct copyable_non_moveable
{
   int val;
   copyable_non_moveable(int);
   copyable_non_moveable(const copyable_non_moveable&) = default;
   copyable_non_moveable& operator=(const copyable_non_moveable&) = default;
   copyable_non_moveable(copyable_non_moveable&&) = delete;
   copyable_non_moveable& operator=(copyable_non_moveable&&) = delete;
};

#endif

TT_TEST_BEGIN(has_trivial_move_assign)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<bool>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<bool const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<bool volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<bool const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<signed char>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<signed char const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<signed char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<signed char const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<char>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned char const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<char const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned char const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<char const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<short>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned short const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<short const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned short const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<short const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned int const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned int const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned long const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned long const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long const volatile>::value, false);
#endif
#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::long_long_type>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::ulong_long_type const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::long_long_type const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::ulong_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::long_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::ulong_long_type const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign< ::boost::long_long_type const volatile>::value, false);
#endif
#endif

#ifdef BOOST_HAS_MS_INT64

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int8>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int8 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int8 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int8 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int8 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int16>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int16 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int16 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int16 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int16 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int32>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int32 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int32 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int32 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int32 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int64>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int64 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int64 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<unsigned __int64 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<__int64 const volatile>::value, false);
#endif
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<float>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<float const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<float volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<float const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<double>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<double const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<double const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long double>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long double const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<long double const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<void*>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int*const>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<enum_UDT>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int&>::value, false);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int&&>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<const int&>::value, false);
// array types are not assignable:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int[2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int[3][2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<int[2][4][5][6][3]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<UDT>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<empty_UDT>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<void>::value, false);
// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<empty_POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<empty_POD_union_UDT>::value, true, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<trivial_except_assign>::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<trivial_except_destroy>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<trivial_except_construct>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<trivial_except_copy>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<wrap<trivial_except_assign> >::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<wrap<trivial_except_destroy> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<wrap<trivial_except_construct> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<wrap<trivial_except_copy> >::value, true, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<test_abc1>::value, false);

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<non_copyable_movable>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_assign<copyable_non_moveable>::value, false);
#endif
   
TT_TEST_END



