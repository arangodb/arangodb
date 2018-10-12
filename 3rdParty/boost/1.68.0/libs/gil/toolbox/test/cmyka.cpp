/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

#include <boost/test/unit_test.hpp>

#include <boost/type_traits/is_same.hpp>

#include <boost/gil/extension/toolbox/color_spaces/cmyka.hpp>

using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( cmyka_test )
{
    cmyka8_pixel_t a( 10, 20, 30, 40, 50 );
    rgba8_pixel_t  b;
    cmyka8_pixel_t c;

    color_convert( a, b );

    // no rgba to cmyka conversion implemented
    //color_convert( b, c );
    //BOOST_ASSERT( at_c<0>(a) == at_c<0>(c) );
}

BOOST_AUTO_TEST_SUITE_END()
