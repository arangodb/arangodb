//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/extension/toolbox/metafunctions/channel_type.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( channel_type_test )
{
    BOOST_STATIC_ASSERT(( is_same< unsigned char, channel_type< rgb8_pixel_t >::type >::value ));

    // float32_t is a scoped_channel_value object
    BOOST_STATIC_ASSERT((
        is_same<float32_t, channel_type<rgba32f_pixel_t>::type>::value));

    // channel_type for bit_aligned images doesn't work with standard gil.
    typedef bit_aligned_image4_type<4, 4, 4, 4, rgb_layout_t>::type image_t;
    typedef channel_type< image_t::view_t::reference >::type channel_t;
}

BOOST_AUTO_TEST_SUITE_END()
