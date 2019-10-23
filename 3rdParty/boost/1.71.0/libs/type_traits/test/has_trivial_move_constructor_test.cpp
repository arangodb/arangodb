
//  (C) Copyright John Maddock 2000. 
//  (C) Copyright Antony Polukhin 2013.
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/has_trivial_move_constructor.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS

struct non_copyable_movable
{
   int val;
   non_copyable_movable(int);
   non_copyable_movable(const non_copyable_movable&) = delete;
   non_copyable_movable& operator=(const non_copyable_movable&) = delete;
#if BOOST_WORKAROUND(BOOST_MSVC, <= 1800) || BOOST_WORKAROUND(BOOST_GCC_VERSION, < 40500)
   non_copyable_movable(non_copyable_movable&& o) : val(o.val){}
   non_copyable_movable& operator=(non_copyable_movable&& o)
   {
      val = std::move(o.val);
      return *this;
   }
#else
   non_copyable_movable(non_copyable_movable&&) = default;
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

TT_TEST_BEGIN(has_trivial_move_constructor)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<bool>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<bool const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<bool volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<bool const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<signed char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<signed char const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<signed char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<signed char const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<char const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned char const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<char const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<short const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned short const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<short const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned int const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned long const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long const volatile>::value, false);
#endif
#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::long_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::ulong_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::long_long_type const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::ulong_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::long_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::ulong_long_type const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor< ::boost::long_long_type const volatile>::value, false);
#endif
#endif

#ifdef BOOST_HAS_MS_INT64

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int8 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int8 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int8 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int16 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int16 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int16 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int32 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int32 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int32 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int64 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<unsigned __int64 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<__int64 const volatile>::value, false);
#endif
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<float>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<float const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<float volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<float const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<double const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<double const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long double const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<long double const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<void*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int*const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<enum_UDT>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int&>::value, true);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int&&>::value, true);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<const int&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int[2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int[3][2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<int[2][4][5][6][3]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<UDT>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<void>::value, false);

#ifdef BOOST_HAS_TRIVIAL_MOVE_CONSTRUCTOR

// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<empty_POD_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<POD_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<POD_union_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<empty_POD_union_UDT>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_copy>::value, false);
// Why does this fail on multiple compilers??
//BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_destroy>::value, true);
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1800)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_construct>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_assign>::value, true);
#endif
/*
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_copy> >::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_destroy> >::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_construct> >::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_assign> >::value, true);
*/
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<test_abc1>::value, false);

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1900)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<non_copyable_movable>::value, true);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<copyable_non_moveable>::value, false);
#endif

#else

// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<empty_POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<empty_POD_union_UDT>::value, true, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_copy>::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_destroy>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_construct>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<trivial_except_assign>::value, true, false);
/*
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_copy> >::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_destroy> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_construct> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<wrap<trivial_except_assign> >::value, true, false);
*/
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<test_abc1>::value, false);

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<non_copyable_movable>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_trivial_move_constructor<copyable_non_moveable>::value, false);
#endif

#endif

TT_TEST_END








