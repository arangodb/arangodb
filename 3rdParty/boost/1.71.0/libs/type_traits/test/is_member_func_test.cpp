
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_member_function_pointer.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

#if defined(BOOST_GCC) && (BOOST_GCC >= 70000)
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

#ifdef BOOST_TT_HAS_ASCCURATE_IS_FUNCTION
struct tricky_members
{
   void noexcept_proc()noexcept
   {}
   void const_ref_proc()const &
   {}
   void rvalue_proc()&&
   {}
};

template <class T>
void test_tricky(T)
{
   BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<T>::value, true);
}
#endif

TT_TEST_BEGIN(is_member_function_pointer)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<f1>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<f2>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<f3>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<void*>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf1>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf2>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf3>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf4>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<cmf>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mp>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<void>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<test_abc1>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<foo0_t>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<int&>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<const int&>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<const int[2] >::value, false);

#if defined(__cpp_noexcept_function_type) && !defined(BOOST_TT_NO_NOEXCEPT_SEPARATE_TYPE)
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf5>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf6>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf7>::value, true);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<mf8>::value, true);

#ifndef __IBMCPP__
// this test may not be strictly legal:
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<const int[] >::value, false);
#endif
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<void>::value, false);

#ifdef BOOST_TT_TEST_MS_FUNC_SIGS

typedef void (__stdcall test_abc1::*scall_proc)();
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<scall_proc>::value, true);
#ifndef __CLR_VER
typedef void (__fastcall test_abc1::*fcall_proc)(int);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<fcall_proc>::value, true);
#endif
typedef void (__cdecl test_abc1::*ccall_proc)(int, long, double);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<ccall_proc>::value, true);
#if (_MSC_VER >= 1800) && !defined(__CLR_VER) && (defined(_M_IX86_FP) && (_M_IX86_FP >= 2) || defined(_M_X64))
typedef void(__vectorcall test_abc1::*vcall_proc)(int, long, double, double, double, double);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<vcall_proc>::value, true);
#endif
typedef void(__thiscall test_abc1::*tcall_proc)(int, long, double, double, double, double);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_member_function_pointer<tcall_proc>::value, true);

#endif

#ifdef BOOST_TT_HAS_ASCCURATE_IS_FUNCTION
test_tricky(&tricky_members::const_ref_proc);
#ifndef BOOST_TT_NO_NOEXCEPT_SEPARATE_TYPE
test_tricky(&tricky_members::noexcept_proc);
#endif
test_tricky(&tricky_members::rvalue_proc);
#endif

TT_TEST_END


