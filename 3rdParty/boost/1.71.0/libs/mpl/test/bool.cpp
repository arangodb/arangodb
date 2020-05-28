
// Copyright Aleksey Gurtovoy 2001-2004
//
// Distributed under the Boost Software License,Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/bool.hpp>
#include <boost/mpl/aux_/test.hpp>

#include <cassert>

#if defined(BOOST_NO_CXX11_CONSTEXPR)
#define CONSTEXPR_BOOL_TEST(c)
#else 
#define CONSTEXPR_BOOL_TEST(c) { static_assert(bool_<c>() == c, "Constexpr for bool_ failed"); }
#endif

#define BOOL_TEST(c) \
    { MPL_ASSERT(( is_same< bool_<c>::value_type, bool > )); } \
    { MPL_ASSERT(( is_same< bool_<c>, c##_ > )); } \
    { MPL_ASSERT(( is_same< bool_<c>::type, bool_<c> > )); } \
    { MPL_ASSERT_RELATION( bool_<c>::value, ==, c ); } \
    CONSTEXPR_BOOL_TEST(c) \
    BOOST_TEST( bool_<c>() == c ); \
/**/

MPL_TEST_CASE()
{
    BOOL_TEST(true)
    BOOL_TEST(false)
}
