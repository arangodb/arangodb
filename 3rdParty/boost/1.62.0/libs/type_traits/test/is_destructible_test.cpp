
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_integral_constant.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_destructible.hpp>
#endif

#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

struct deleted_destruct
{
   deleted_destruct();
   ~deleted_destruct() = delete;
};

#endif


TT_TEST_BEGIN(is_destructible)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<bool>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<bool const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<bool volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<bool const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<signed char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<signed char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<signed char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<signed char const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned char const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<char const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned short volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<short volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned short const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<short const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned int const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned long volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned long const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long const volatile>::value, true);

#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::long_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::ulong_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::long_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::ulong_long_type volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::long_long_type volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::ulong_long_type const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible< ::boost::long_long_type const volatile>::value, true);

#endif

#ifdef BOOST_HAS_MS_INT64

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int8 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int8 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int8 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int8 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int16 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int16 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int16 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int16 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int32 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int32 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int32 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int32 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int64 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int64 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<unsigned __int64 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<__int64 const volatile>::value, true);

#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<float>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<float const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<float volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<float const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<double const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<double volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<double const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long double const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long double volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<long double const volatile>::value, true);


BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<void*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int*const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT const&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT const volatile&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT volatile&>::value, true);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT&&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT const&&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT const volatile&&>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<enum_UDT volatile&&>::value, true);
#endif

//
// These are commented out for now because it's not clear what the semantics should be:
// on the one hand references always have trivial destructors (in the sense that there is
// nothing to destruct), on the other hand the thing referenced may not have a trivial
// destructor, it really depends upon the users code as to what should happen here:
//
//BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int&>::value, false);
//BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<const int&>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int[2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int[3][2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<int[2][4][5][6][3]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<empty_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<void>::value, false);

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_destructible<deleted_destruct>::value, false);
#endif

TT_TEST_END



