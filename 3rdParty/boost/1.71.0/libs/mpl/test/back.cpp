
// Copyright Aleksey Gurtovoy 2000-2004
//
// Distributed under the Boost Software License,Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/back.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/aux_/test.hpp>

template< typename Seq, int value > struct back_test
{
    typedef typename back<Seq>::type t;
    MPL_ASSERT_RELATION( t::value, ==, value );
};

MPL_TEST_CASE()
{
    back_test< range_c<int,0,1>, 0 >();
    back_test< range_c<int,0,10>, 9 >();
    back_test< range_c<int,-10,0>, -1 >();
}
