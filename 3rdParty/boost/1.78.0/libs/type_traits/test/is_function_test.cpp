
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_function.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

#if defined(BOOST_GCC) && (BOOST_GCC >= 70000)
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

#ifdef BOOST_TT_HAS_ASCCURATE_IS_FUNCTION

struct X
{
   void f() {}
   void fc() const {}
   void fv() volatile {}
   void fcv() const volatile {}
   void noexcept_f()noexcept {}
   void ref_f()const& {}
   void rvalue_f() && {}
};

template< class C, class F > void test_cv_qual(F C::*)
{
   BOOST_CHECK_INTEGRAL_CONSTANT(boost::is_function< F >::value, true);
}

#endif

TT_TEST_BEGIN(is_function)

typedef void foo0_t();
typedef void foo1_t(int);
typedef void foo2_t(int&, double);
typedef void foo3_t(int&, bool, int, int);
typedef void foo4_t(int, bool, int*, int[], int, int, int, int, int);
#if __cpp_noexcept_function_type
typedef int foo5_t(void)noexcept;
typedef int foo6_t(double)noexcept(false);
typedef int foo7_t(int, double)noexcept(true);
#endif
typedef double foo8_t(double...);


BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo0_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo1_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo2_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo3_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo4_t>::value, true);
#if defined(__cpp_noexcept_function_type) && !defined(BOOST_TT_NO_NOEXCEPT_SEPARATE_TYPE)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo5_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo6_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo7_t>::value, true);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<foo8_t>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<void>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<int>::value, false);
#if !defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<int&>::value, false);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<int&&>::value, false);
#endif
#else
std::cout << 
"<note>is_function will fail with some types (references for example)"
"if the compiler doesn't support partial specialisation of class templates."
"These are *not* tested here</note>" << std::endl;
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<int*>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<int[]>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<test_abc1>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<int (*)(int)>::value, false);

#ifdef BOOST_TT_TEST_MS_FUNC_SIGS
typedef void __stdcall sfoo0_t();
typedef void __stdcall sfoo1_t(int);
typedef void __stdcall sfoo2_t(int&, double);
typedef void __stdcall sfoo3_t(int&, bool, int, int);
typedef void __stdcall sfoo4_t(int, bool, int*, int[], int, int, int, int, int);

typedef void __cdecl cfoo0_t();
typedef void __cdecl cfoo1_t(int);
typedef void __cdecl cfoo2_t(int&, double);
typedef void __cdecl cfoo3_t(int&, bool, int, int);
typedef void __cdecl cfoo4_t(int, bool, int*, int[], int, int, int, int, int);

#ifndef _MANAGED
typedef void __fastcall ffoo0_t();
typedef void __fastcall ffoo1_t(int);
typedef void __fastcall ffoo2_t(int&, double);
typedef void __fastcall ffoo3_t(int&, bool, int, int);
typedef void __fastcall ffoo4_t(int, bool, int*, int[], int, int, int, int, int);
#endif
#if (_MSC_VER >= 1800) && !defined(_MANAGED) && (defined(_M_IX86_FP) && (_M_IX86_FP >= 2) || defined(_M_X64))
typedef void __vectorcall vfoo0_t();
typedef void __vectorcall vfoo1_t(int);
typedef void __vectorcall vfoo2_t(int&, double);
typedef void __vectorcall vfoo3_t(int&, bool, int, int);
typedef void __vectorcall vfoo4_t(int, bool, int*, int[], int, int, int, int, int);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<sfoo0_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<sfoo1_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<sfoo2_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<sfoo3_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<sfoo4_t>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<cfoo0_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<cfoo1_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<cfoo2_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<cfoo3_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<cfoo4_t>::value, true);

#ifndef _MANAGED
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<ffoo0_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<ffoo1_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<ffoo2_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<ffoo3_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<ffoo4_t>::value, true);
#endif

#if (_MSC_VER >= 1800) && !defined(_MANAGED) && (defined(_M_IX86_FP) && (_M_IX86_FP >= 2) || defined(_M_X64))
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<vfoo0_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<vfoo1_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<vfoo2_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<vfoo3_t>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_function<vfoo4_t>::value, true);
#endif

#endif

#ifdef BOOST_TT_HAS_ASCCURATE_IS_FUNCTION

test_cv_qual(&X::f);
test_cv_qual(&X::fc);
test_cv_qual(&X::fv);
test_cv_qual(&X::fcv);
#ifndef BOOST_TT_NO_NOEXCEPT_SEPARATE_TYPE
test_cv_qual(&X::noexcept_f);
#endif
test_cv_qual(&X::ref_f);
test_cv_qual(&X::rvalue_f);

#endif

TT_TEST_END








