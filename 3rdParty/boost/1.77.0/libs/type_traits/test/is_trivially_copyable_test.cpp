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
#include <boost/type_traits/is_trivially_copyable.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

class private_copy {
public:
   private_copy();
private:
   private_copy(const private_copy&);
};

class private_assign {
public:
   private_assign();
private:
   private_assign& operator=(const private_assign&);
};

class private_destruct {
public:
   private_destruct();
private:
   ~private_destruct();
};

#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
struct deleted_assign {
   deleted_assign();
   deleted_assign& operator=(const deleted_assign&) = delete;
};

struct deleted_copy {
    deleted_copy() { }
    deleted_copy(const deleted_copy&) = delete;
    deleted_copy(deleted_copy&&) { }
};

struct deleted_destruct {
   deleted_destruct();
   ~deleted_destruct() = delete;
};
#endif

#ifndef BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
struct default_copy {
    default_copy(const default_copy&) = default;
    default_copy(default_copy&&) { }
    default_copy& operator=(const default_copy&) = default;
    default_copy& operator=(default_copy&&) {
        return *this;
    }
};
#endif

TT_TEST_BEGIN(is_trivially_copyable)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<bool>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<bool const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<bool volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<bool const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<signed char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<signed char const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<signed char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<signed char const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<char>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned char const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<char const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<char volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned char const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<char const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<short>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned short const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<short const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<short volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned short const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<short const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned int const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned int const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned long const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned long const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long const volatile>::value, false);
#endif
#ifdef BOOST_HAS_LONG_LONG
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::ulong_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::long_long_type>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::ulong_long_type const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::long_long_type const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::ulong_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::long_long_type volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::ulong_long_type const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable< ::boost::long_long_type const volatile>::value, false);
#endif
#endif
#ifdef BOOST_HAS_MS_INT64
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int8>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int8 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int8 const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int8 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int8 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int8 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int16>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int16 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int16 const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int16 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int16 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int16 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int32>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int32 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int32 const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int32 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int32 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int32 const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int64>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int64 const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int64 const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int64 volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<unsigned __int64 const volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<__int64 const volatile>::value, false);
#endif
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<float>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<float const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<float volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<float const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<double const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<double const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long double>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long double const>::value, false);
#ifndef TEST_STD
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long double volatile>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<long double const volatile>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<void*>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int*const>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<f1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<f2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<f3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<mp>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<enum_UDT>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int&>::value, false);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int&&>::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<const int&>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int[2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int[3][2]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<int[2][4][5][6][3]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<UDT>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<void>::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<empty_POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<POD_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<POD_union_UDT>::value, true, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<empty_POD_union_UDT>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<trivial_except_copy>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<trivial_except_destroy>::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<trivial_except_construct>::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<trivial_except_assign>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<wrap<trivial_except_copy> >::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<wrap<trivial_except_destroy> >::value, false);
BOOST_CHECK_SOFT_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<wrap<trivial_except_construct> >::value, true, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<wrap<trivial_except_assign> >::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<test_abc1>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<private_copy>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<private_assign>::value, false);
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<deleted_copy>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<deleted_assign>::value, false);
#endif
#ifndef BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_trivially_copyable<default_copy>::value, false);
#endif

TT_TEST_END
