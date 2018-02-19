
// Copyright Aleksey Gurtovoy 2000-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/distance.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/range_c.hpp>

#include <boost/mpl/aux_/test.hpp>


MPL_TEST_CASE()
{
    typedef list<char,short,int,long>::type list;
    typedef begin<list>::type first;
    typedef end<list>::type last;
    
    MPL_ASSERT_RELATION( (mpl::distance<first,last>::value), ==, 4 );
}

MPL_TEST_CASE()
{
    typedef range_c<int,0,10>::type range;
    typedef begin<range>::type first;
    typedef end<range>::type last;
    
    MPL_ASSERT_RELATION( (mpl::distance<first,last>::value), ==, 10 );
}
