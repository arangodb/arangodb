
//  (C) Copyright John Maddock 2000. 
//  (C) Copyright Peter Dimov 2015.
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_type.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/type_identity.hpp>
#endif

BOOST_DECL_TRANSFORM_TEST(type_identity_test_1, ::tt::type_identity, const, const)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_2, ::tt::type_identity, volatile, volatile)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_3, ::tt::type_identity, const volatile, const volatile)
BOOST_DECL_TRANSFORM_TEST0(type_identity_test_4, ::tt::type_identity)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_5, ::tt::type_identity, [], [])
BOOST_DECL_TRANSFORM_TEST(type_identity_test_6, ::tt::type_identity, *const, *const)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_7, ::tt::type_identity, *volatile, *volatile)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_8, ::tt::type_identity, *const volatile, *const volatile)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_9, ::tt::type_identity, *, *)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_10, ::tt::type_identity, *, *)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_11, ::tt::type_identity, volatile*, volatile*)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_12, ::tt::type_identity, const[2], const[2])
BOOST_DECL_TRANSFORM_TEST(type_identity_test_13, ::tt::type_identity, volatile[2], volatile[2])
BOOST_DECL_TRANSFORM_TEST(type_identity_test_14, ::tt::type_identity, const volatile[2], const volatile[2])
BOOST_DECL_TRANSFORM_TEST(type_identity_test_15, ::tt::type_identity, [2], [2])
BOOST_DECL_TRANSFORM_TEST(type_identity_test_16, ::tt::type_identity, const*, const*)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_17, ::tt::type_identity, const*volatile, const*volatile)
BOOST_DECL_TRANSFORM_TEST(type_identity_test_18, ::tt::type_identity, (), ())
BOOST_DECL_TRANSFORM_TEST(type_identity_test_19, ::tt::type_identity, (int), (int))
BOOST_DECL_TRANSFORM_TEST(type_identity_test_20, ::tt::type_identity, (*const)(), (*const)())

TT_TEST_BEGIN(type_identity)

   type_identity_test_1();
   type_identity_test_2();
   type_identity_test_3();
   type_identity_test_4();
   type_identity_test_5();
   type_identity_test_6();
   type_identity_test_7();
   type_identity_test_8();
   type_identity_test_9();
   type_identity_test_10();
   type_identity_test_11();
   type_identity_test_12();
   type_identity_test_13();
   type_identity_test_14();
   type_identity_test_15();
   type_identity_test_16();
   type_identity_test_17();
   type_identity_test_18();
   type_identity_test_19();
   type_identity_test_20();

TT_TEST_END
