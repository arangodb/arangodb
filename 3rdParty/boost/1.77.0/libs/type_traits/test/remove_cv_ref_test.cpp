
//  (C) Copyright John Maddock 2000. 
//  (C) Copyright Peter Dimov 2017. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/remove_cv_ref.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"

BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_1, ::tt::remove_cv_ref, const)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_2, ::tt::remove_cv_ref, volatile)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_3, ::tt::remove_cv_ref, const volatile)
BOOST_DECL_TRANSFORM_TEST0(remove_cv_ref_test_4, ::tt::remove_cv_ref)

BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_5, ::tt::remove_cv_ref, const &)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_6, ::tt::remove_cv_ref, volatile &)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_7, ::tt::remove_cv_ref, const volatile &)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_8, ::tt::remove_cv_ref, &)

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_9,  ::tt::remove_cv_ref, const &&)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_10, ::tt::remove_cv_ref, volatile &&)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_11, ::tt::remove_cv_ref, const volatile &&)
BOOST_DECL_TRANSFORM_TEST3(remove_cv_ref_test_12, ::tt::remove_cv_ref, &&)
#endif

BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_13, ::tt::remove_cv_ref, *const, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_14, ::tt::remove_cv_ref, *volatile, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_15, ::tt::remove_cv_ref, *const volatile, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_16, ::tt::remove_cv_ref, *, *)

BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_17, ::tt::remove_cv_ref, *const &, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_18, ::tt::remove_cv_ref, *volatile &, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_19, ::tt::remove_cv_ref, *const volatile &, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_20, ::tt::remove_cv_ref, * &, *)

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_21, ::tt::remove_cv_ref, *const &&, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_22, ::tt::remove_cv_ref, *volatile &&, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_23, ::tt::remove_cv_ref, *const volatile &&, *)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_24, ::tt::remove_cv_ref, * &&, *)
#endif

BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_25, ::tt::remove_cv_ref, const*, const*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_26, ::tt::remove_cv_ref, volatile*, volatile*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_27, ::tt::remove_cv_ref, const*const, const*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_28, ::tt::remove_cv_ref, const*volatile, const*)

BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_29, ::tt::remove_cv_ref, const* &, const*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_30, ::tt::remove_cv_ref, volatile* &, volatile*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_31, ::tt::remove_cv_ref, const*const &, const*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_32, ::tt::remove_cv_ref, const*volatile &, const*)

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_33, ::tt::remove_cv_ref, const* &&, const*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_34, ::tt::remove_cv_ref, volatile* &&, volatile*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_35, ::tt::remove_cv_ref, const*const &&, const*)
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_36, ::tt::remove_cv_ref, const*volatile &&, const*)
#endif

BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_37, ::tt::remove_cv_ref, const[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_38, ::tt::remove_cv_ref, volatile[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_39, ::tt::remove_cv_ref, const volatile[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_40, ::tt::remove_cv_ref, [2], [2])

BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_41, ::tt::remove_cv_ref, const(&)[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_42, ::tt::remove_cv_ref, volatile(&)[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_43, ::tt::remove_cv_ref, const volatile(&)[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_44, ::tt::remove_cv_ref, (&)[2], [2])

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_45, ::tt::remove_cv_ref, const(&&)[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_46, ::tt::remove_cv_ref, volatile(&&)[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_47, ::tt::remove_cv_ref, const volatile(&&)[2], [2])
BOOST_DECL_TRANSFORM_TEST(remove_cv_ref_test_48, ::tt::remove_cv_ref, (&&)[2], [2])
#endif

TT_TEST_BEGIN(remove_cv_ref)

   remove_cv_ref_test_1();
   remove_cv_ref_test_2();
   remove_cv_ref_test_3();
   remove_cv_ref_test_4();
   remove_cv_ref_test_5();
   remove_cv_ref_test_6();
   remove_cv_ref_test_7();
   remove_cv_ref_test_8();
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   remove_cv_ref_test_9();
   remove_cv_ref_test_10();
   remove_cv_ref_test_11();
   remove_cv_ref_test_12();
#endif
   remove_cv_ref_test_13();
   remove_cv_ref_test_14();
   remove_cv_ref_test_15();
   remove_cv_ref_test_16();
   remove_cv_ref_test_17();
   remove_cv_ref_test_18();
   remove_cv_ref_test_19();
   remove_cv_ref_test_20();
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   remove_cv_ref_test_21();
   remove_cv_ref_test_22();
   remove_cv_ref_test_23();
   remove_cv_ref_test_24();
#endif
   remove_cv_ref_test_25();
   remove_cv_ref_test_26();
   remove_cv_ref_test_27();
   remove_cv_ref_test_28();
   remove_cv_ref_test_29();
   remove_cv_ref_test_30();
   remove_cv_ref_test_31();
   remove_cv_ref_test_32();
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   remove_cv_ref_test_33();
   remove_cv_ref_test_34();
   remove_cv_ref_test_35();
   remove_cv_ref_test_36();
#endif
   remove_cv_ref_test_37();
   remove_cv_ref_test_38();
   remove_cv_ref_test_39();
   remove_cv_ref_test_40();
   remove_cv_ref_test_41();
   remove_cv_ref_test_42();
   remove_cv_ref_test_43();
   remove_cv_ref_test_44();
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   remove_cv_ref_test_45();
   remove_cv_ref_test_46();
   remove_cv_ref_test_47();
   remove_cv_ref_test_48();
#endif

TT_TEST_END

