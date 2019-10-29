
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_default_constructible.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

class bug11324_base
{
public:
   bug11324_base & operator=(const bug11324_base&){ throw int(); }
   virtual ~bug11324_base() {}
};

class bug11324_derived : public bug11324_base
{
public:
   char data;
   explicit bug11324_derived(char arg) : data(arg) {}
};

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS

struct deleted_default_construct
{
   deleted_default_construct() = delete;
   deleted_default_construct(char val) : member(val) {}
   char member;
};

#endif

struct private_default_construct
{
private:
   private_default_construct();
public:
   private_default_construct(char val) : member(val) {}
   char member;
};

TT_TEST_BEGIN(is_default_constructible)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<bool>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<bool const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<bool volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<bool const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<signed char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<signed char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<signed char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<signed char const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<char const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<char volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned char const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<char const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<short const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned short volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<short volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned short const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<short const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned int const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned long volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned long const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long const volatile>::value, true);

#ifdef BOOST_HAS_LONG_LONG

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::long_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::ulong_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::long_long_type const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::ulong_long_type volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::long_long_type volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::ulong_long_type const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible< ::boost::long_long_type const volatile>::value, true);

#endif

#ifdef BOOST_HAS_MS_INT64

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int8 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int8 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int8 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int8 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int8 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int16 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int16 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int16 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int16 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int16 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int32 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int32 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int32 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int32 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int32 const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int64 const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int64 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int64 volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<unsigned __int64 const volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<__int64 const volatile>::value, true);

#endif

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<float>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<float const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<float volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<float const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<double const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<double volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<double const volatile>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long double const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long double volatile>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<long double const volatile>::value, true);


BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<void*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int*const>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<enum_UDT>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int&>::value, false);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int&&>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<const int&>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int[2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int[3][2]>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<int[2][4][5][6][3]>::value, true);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<empty_UDT>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<void>::value, false);
// cases we would like to succeed but can't implement in the language:
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<empty_POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<empty_POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_default_constructible<nothrow_construct_UDT>::value, true, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<test_abc1>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<bug11324_derived>::value, false);
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<deleted_default_construct>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_default_constructible<std::pair<deleted_default_construct, int> >::value), false);
#endif
#if !BOOST_WORKAROUND(BOOST_GCC_VERSION, < 40800)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_default_constructible<private_default_construct>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT((::tt::is_default_constructible<std::pair<private_default_construct, int> >::value), false);
#endif

TT_TEST_END

