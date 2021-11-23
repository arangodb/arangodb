
// Copyright Aleksey Gurtovoy 2001-2006
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/next_prior.hpp>
#include <boost/mpl/aux_/test.hpp>
#include <boost/mpl/aux_/config/workaround.hpp>

#include <cassert>

#if !BOOST_WORKAROUND(BOOST_BORLANDC, < 0x600)
#   define INTEGRAL_WRAPPER_RUNTIME_TEST(i, T) \
    BOOST_TEST(( WRAPPER(T,i)() == i )); \
    BOOST_TEST(( WRAPPER(T,i)::value == i )); \
    /**/
#else
#   define INTEGRAL_WRAPPER_RUNTIME_TEST(i, T) \
    BOOST_TEST(( WRAPPER(T,i)::value == i )); \
    /**/
#endif

#if defined(BOOST_NO_CXX11_CONSTEXPR)
#define CONSTEXPR_INTEGRAL_TEST(T, i)
#else 
#define CONSTEXPR_INTEGRAL_TEST(T, i) { static_assert(T() == i, "Constexpr for integral constant failed"); }
#endif

#if BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x582))
// agurt 20/nov/06: see http://article.gmane.org/gmane.comp.lib.boost.devel/151065
#define INTEGRAL_WRAPPER_TEST(unused1, i, T) \
    { \
      typedef WRAPPER(T,i) borland_tested_type; \
    { typedef is_same< borland_tested_type::value_type, T > borland_test_type; \
      MPL_ASSERT(( borland_test_type )); } \
    { MPL_ASSERT(( is_same< borland_tested_type::type, WRAPPER(T,i) > )); } \
    { MPL_ASSERT(( is_same< next< borland_tested_type >::type, WRAPPER(T,i+1) > )); } \
    { MPL_ASSERT(( is_same< prior< borland_tested_type >::type, WRAPPER(T,i-1) > )); } \
    { MPL_ASSERT_RELATION( (borland_tested_type::value), ==, i ); } \
    CONSTEXPR_INTEGRAL_TEST(borland_tested_type, i) \
    INTEGRAL_WRAPPER_RUNTIME_TEST(i, T) \
    } \
/**/
#else
#define INTEGRAL_WRAPPER_TEST(unused1, i, T) \
    { MPL_ASSERT(( is_same< WRAPPER(T,i)::value_type, T > )); } \
    { MPL_ASSERT(( is_same< WRAPPER(T,i)::type, WRAPPER(T,i) > )); } \
    { MPL_ASSERT(( is_same< next< WRAPPER(T,i) >::type, WRAPPER(T,i+1) > )); } \
    { MPL_ASSERT(( is_same< prior< WRAPPER(T,i) >::type, WRAPPER(T,i-1) > )); } \
    { MPL_ASSERT_RELATION( (WRAPPER(T,i)::value), ==, i ); } \
    CONSTEXPR_INTEGRAL_TEST(WRAPPER(T,i), i) \
    INTEGRAL_WRAPPER_RUNTIME_TEST(i, T) \
/**/
#endif

