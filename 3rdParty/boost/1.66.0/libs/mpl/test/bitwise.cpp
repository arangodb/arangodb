
// Copyright Aleksey Gurtovoy 2003-2004
// Copyright Jaap Suter 2003
//
// Distributed under the Boost Software License,Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/bitwise.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/aux_/test.hpp>

typedef integral_c<unsigned int, 0> _0;
typedef integral_c<unsigned int, 1> _1;
typedef integral_c<unsigned int, 2> _2;
typedef integral_c<unsigned int, 8> _8;
typedef integral_c<unsigned int, 0xffffffff> _ffffffff;

MPL_TEST_CASE()
{
    MPL_ASSERT_RELATION( (bitand_<_0,_0>::value), ==, 0 );
    MPL_ASSERT_RELATION( (bitand_<_1,_0>::value), ==, 0 );
    MPL_ASSERT_RELATION( (bitand_<_0,_1>::value), ==, 0 );
    MPL_ASSERT_RELATION( (bitand_<_0,_ffffffff>::value), ==, 0 );
    MPL_ASSERT_RELATION( (bitand_<_1,_ffffffff>::value), ==, 1 );
    MPL_ASSERT_RELATION( (bitand_<_8,_ffffffff>::value), ==, 8 );
}

MPL_TEST_CASE()
{
    MPL_ASSERT_RELATION( (bitor_<_0,_0>::value), ==, 0 );
    MPL_ASSERT_RELATION( (bitor_<_1,_0>::value), ==, 1 );
    MPL_ASSERT_RELATION( (bitor_<_0,_1>::value), ==, 1 );
    MPL_ASSERT_RELATION( static_cast<long>(bitor_<_0,_ffffffff>::value), ==, static_cast<long>(0xffffffff) );
    MPL_ASSERT_RELATION( static_cast<long>(bitor_<_1,_ffffffff>::value), ==, static_cast<long>(0xffffffff) );
    MPL_ASSERT_RELATION( static_cast<long>(bitor_<_8,_ffffffff>::value), ==, static_cast<long>(0xffffffff) );
}

MPL_TEST_CASE()
{
    MPL_ASSERT_RELATION( (bitxor_<_0,_0>::value), ==, 0 );
    MPL_ASSERT_RELATION( (bitxor_<_1,_0>::value), ==, 1 );
    MPL_ASSERT_RELATION( (bitxor_<_0,_1>::value), ==, 1 );
    MPL_ASSERT_RELATION( static_cast<long>(bitxor_<_0,_ffffffff>::value), ==, static_cast<long>(0xffffffff ^ 0) );
    MPL_ASSERT_RELATION( static_cast<long>(bitxor_<_1,_ffffffff>::value), ==, static_cast<long>(0xffffffff ^ 1) );
    MPL_ASSERT_RELATION( static_cast<long>(bitxor_<_8,_ffffffff>::value), ==, static_cast<long>(0xffffffff ^ 8) );
}

MPL_TEST_CASE()
{
    MPL_ASSERT_RELATION( (shift_right<_0,_0>::value), ==, 0 );
    MPL_ASSERT_RELATION( (shift_right<_1,_0>::value), ==, 1 );
    MPL_ASSERT_RELATION( (shift_right<_1,_1>::value), ==, 0 );
    MPL_ASSERT_RELATION( (shift_right<_2,_1>::value), ==, 1 );
    MPL_ASSERT_RELATION( (shift_right<_8,_1>::value), ==, 4 );
}

MPL_TEST_CASE()
{
    MPL_ASSERT_RELATION( (shift_left<_0,_0>::value), ==, 0 );
    MPL_ASSERT_RELATION( (shift_left<_1,_0>::value), ==, 1 );
    MPL_ASSERT_RELATION( (shift_left<_1,_1>::value), ==, 2 );
    MPL_ASSERT_RELATION( (shift_left<_2,_1>::value), ==, 4 );
    MPL_ASSERT_RELATION( (shift_left<_8,_1>::value), ==, 16 );
}
