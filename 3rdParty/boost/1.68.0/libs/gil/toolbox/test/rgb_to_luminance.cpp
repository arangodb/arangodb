/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

#include <boost/test/unit_test.hpp>

#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_converters/rgb_to_luminance.hpp>

using namespace boost;
using namespace gil;

struct double_zero { static double apply() { return 0.0; } };
struct double_one  { static double apply() { return 1.0; } };

typedef pixel< double, gray_layout_t > gray64f_pixel_t;
typedef pixel< double, rgb_layout_t  > rgb64f_pixel_t;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( rgb_to_luminance_test )
{
    rgb64f_pixel_t a( 10, 20, 30 );
    gray64f_pixel_t b;

    color_convert( a, b );
}

BOOST_AUTO_TEST_SUITE_END()
