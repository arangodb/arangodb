
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/has_nothrow_copy.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

struct non_copy
{
   non_copy();
private:
   non_copy(const non_copy&);
};

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS

struct delete_move
{
   delete_move();
   delete_move(const delete_move&&) = delete;
};

struct delete_copy
{
   delete_copy();
   delete_copy(const delete_copy&) = delete;
};

#endif

#ifndef BOOST_NO_CXX11_NOEXCEPT

struct noexcept_copy
{
   noexcept_copy();
   noexcept_copy& operator=(const noexcept_copy&)noexcept;
};

#endif

TT_TEST_BEGIN(has_nothrow_copy)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<bool>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<bool const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<bool volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<bool const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<signed char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<signed char const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<signed char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<signed char const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<char const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned char const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<char const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<short const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned short const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<short const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned int const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned long const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long const volatile>::value, false);
#endif

#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::long_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::ulong_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::long_long_type const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::ulong_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::long_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::ulong_long_type const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy< ::boost::long_long_type const volatile>::value, false);
#endif
#endif

#ifdef BOOST_HAS_MS_INT64

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int8 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int8 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int8 const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int16 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int16 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int16 const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int32 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int32 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int32 const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int64 const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<unsigned __int64 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<__int64 const volatile>::value, false);
#endif
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<float>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<float const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<float volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<float const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<double const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<double const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long double const>::value, true);
#ifndef TEST_STD
// unspecified behaviour:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<long double const volatile>::value, false);
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<void*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int*const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<enum_UDT>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int&>::value, false);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int&&>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<const int&>::value, false);
// These used to be true, but are now false to match std conforming behavior:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int[2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int[3][2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<int[2][4][5][6][3]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<UDT>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<void>::value, false);
// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<empty_POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<empty_POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<nothrow_copy_UDT>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<nothrow_assign_UDT>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<nothrow_construct_UDT>::value, false);

#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && BOOST_WORKAROUND(BOOST_GCC, < 40700))
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<test_abc1>::value, false);
#endif

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<delete_copy>::value, false);
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && BOOST_WORKAROUND(BOOST_GCC, < 40600)) && !(defined(CI_SUPPRESS_KNOWN_ISSUES) && BOOST_WORKAROUND(BOOST_MSVC, == 1800))\
  && !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(_MANAGED))
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<delete_move>::value, false);
#endif
#endif

#ifndef BOOST_NO_CXX11_NOEXCEPT
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<noexcept_copy>::value, true);
#endif

#if !defined(BOOST_NO_CXX11_DECLTYPE) && !BOOST_WORKAROUND(BOOST_MSVC, < 1800)
#if !defined(BOOST_GCC) || (BOOST_GCC >= 40800)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_copy<non_copy>::value, false);
#endif
#endif

TT_TEST_END



