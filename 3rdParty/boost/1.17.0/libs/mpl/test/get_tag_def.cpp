
// Copyright Sergey Krivonos 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$


#include <boost/mpl/get_tag.hpp>
#include <boost/mpl/aux_/test.hpp>
#include <boost/mpl/aux_/test/assert.hpp>
#include <boost/type_traits/is_same.hpp>


struct test_type_get_tag_def
{
    typedef int a_tag;
};

BOOST_MPL_GET_TAG_DEF(a_tag);

MPL_TEST_CASE()
{
    MPL_ASSERT(( is_same<int, boost::mpl::get_a_tag<test_type_get_tag_def>::type> ));
    MPL_ASSERT(( is_same<test_type_get_tag_def::a_tag, boost::mpl::get_a_tag<test_type_get_tag_def>::type> ));
}
