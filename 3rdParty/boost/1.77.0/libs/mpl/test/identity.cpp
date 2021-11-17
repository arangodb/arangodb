
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

#include <boost/mpl/apply.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/mpl/aux_/test.hpp>

MPL_TEST_CASE()
{
    typedef apply1< identity<>, char >::type t1;
    typedef apply1< identity<_1>, int >::type t2;
    MPL_ASSERT(( is_same< t1, char > ));
    MPL_ASSERT(( is_same< t2, int > ));
}

MPL_TEST_CASE()
{
    typedef apply1< make_identity<>, char >::type t1;
    typedef apply1< make_identity<_1>, int >::type t2;
    MPL_ASSERT(( is_same< t1, identity<char> > ));
    MPL_ASSERT(( is_same< t2, identity<int> > ));
}
