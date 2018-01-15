
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_integral_constant.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/has_nothrow_destructor.hpp>
#endif

#ifdef BOOST_MSVC
#pragma warning(disable:4290) // exception spec ignored
#endif

struct nothrow_destruct
{
   nothrow_destruct(int);
   ~nothrow_destruct()throw();
};

#if !defined(BOOST_NO_CXX11_NOEXCEPT)

struct noexcept_destruct
{
   noexcept_destruct(int);
   ~noexcept_destruct()noexcept;
};

#endif

struct throwing_base
{
   ~throwing_base() throw(int);
};

struct throwing_derived : public throwing_base {};

struct throwing_contained{ throwing_base data; };

TT_TEST_BEGIN(has_nothrow_destructor)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<bool>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<bool const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<bool volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<bool const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<signed char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<signed char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<signed char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<signed char const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned char const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<char const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned short volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<short volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned short const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<short const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned int const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned long volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned long const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long const volatile>::value, true);

#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::long_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::ulong_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::long_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::ulong_long_type volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::long_long_type volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::ulong_long_type const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor< ::boost::long_long_type const volatile>::value, true);

#endif

#ifdef BOOST_HAS_MS_INT64

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int8 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int8 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int8 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int8 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int16 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int16 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int16 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int16 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int32 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int32 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int32 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int32 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int64 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int64 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<unsigned __int64 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<__int64 const volatile>::value, true);

#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<float>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<float const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<float volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<float const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<double const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<double volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<double const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long double const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long double volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<long double const volatile>::value, true);


BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<void*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int*const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<enum_UDT>::value, true);

//
// These are commented out for now because it's not clear what the semantics should be:
// on the one hand references always have trivial destructors (in the sense that there is
// nothing to destruct), on the other hand the thing referenced may not have a trivial
// destructor, it really depends upon the users code as to what should happen here:
//
//BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int&>::value, false);
//BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<const int&>::value, false);
//
// Destructors on UDT's are non-throwing by default, unless they are explicity marked otherwise:
//
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int[2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int[3][2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<int[2][4][5][6][3]>::value, true);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<empty_UDT>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<void>::value, false);
// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<empty_POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<empty_POD_union_UDT>::value, true, false);

BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<trivial_except_destroy>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<trivial_except_copy>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<trivial_except_construct>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<trivial_except_assign>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<wrap<trivial_except_destroy> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<wrap<trivial_except_copy> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<wrap<trivial_except_construct> >::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<wrap<trivial_except_assign> >::value, true, false);

#if !defined(BOOST_NO_CXX11_NOEXCEPT)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<noexcept_destruct>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<nothrow_destruct>::value, true);
   
#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<throwing_base>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<throwing_derived>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::has_nothrow_destructor<throwing_contained>::value, false);

TT_TEST_END



