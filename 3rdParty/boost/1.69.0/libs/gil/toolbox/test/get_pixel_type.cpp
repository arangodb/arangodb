//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/get_pixel_type.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( get_pixel_type_test )
{
    {
        typedef bit_aligned_image3_type< 4, 15, 4, rgb_layout_t >::type image_t;
        BOOST_STATIC_ASSERT(( is_same< get_pixel_type< image_t::view_t >::type
                                     , image_t::view_t::reference
                                     >::value
                           ));
    }

    {
        typedef rgb8_image_t image_t;
        BOOST_STATIC_ASSERT(( is_same< get_pixel_type< image_t::view_t >::type
                                     , image_t::view_t::value_type
                                     >::value
                           ));
    }
}

BOOST_AUTO_TEST_SUITE_END()
