
// Copyright Aleksey Gurtovoy 2001-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/inherit.hpp>
#include <boost/mpl/aux_/test.hpp>

struct her { typedef her herself; };
struct my { typedef my myself; };

MPL_TEST_CASE()
{
    MPL_ASSERT(( is_same<inherit<her>::type, her> ));

    typedef inherit<her,my>::type her_my1;
    MPL_ASSERT(( is_same<her_my1::herself, her> ));
    MPL_ASSERT(( is_same<her_my1::myself, my> ));
    
    typedef inherit<empty_base,her>::type her1;
    MPL_ASSERT(( is_same<her1, her> ));
    
    typedef inherit<empty_base,her,empty_base,empty_base>::type her2;
    MPL_ASSERT(( is_same<her2, her> ));

    typedef inherit<her,empty_base,my>::type her_my2;
    MPL_ASSERT(( is_same<her_my2::herself, her> ));
    MPL_ASSERT(( is_same<her_my2::myself, my> ));

    typedef inherit<empty_base,empty_base>::type empty;
    MPL_ASSERT(( is_same<empty, empty_base> ));
}
