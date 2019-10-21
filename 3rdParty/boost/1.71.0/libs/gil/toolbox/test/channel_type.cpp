//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/extension/toolbox/metafunctions/channel_type.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_bit_aligned.hpp>

#include <boost/gil/channel.hpp> // boost::is_integral<packed_dynamic_channel_reference<...>>

#include <boost/test/unit_test.hpp>

#include <type_traits>

namespace bg = boost::gil;

BOOST_AUTO_TEST_SUITE(toolbox_tests)

BOOST_AUTO_TEST_CASE(channel_type_test)
{
    static_assert(std::is_same
        <
            unsigned char,
            bg::channel_type<bg::rgb8_pixel_t>::type
        >::value, "");

    // float32_t is a scoped_channel_value object
    static_assert(std::is_same
        <
            bg::float32_t,
            bg::channel_type<bg::rgba32f_pixel_t>::type
        >::value, "");

    // channel_type for bit_aligned images doesn't work with standard gil.
    using image_t = bg::bit_aligned_image4_type<4, 4, 4, 4, bg::rgb_layout_t>::type;
    using channel_t = bg::channel_type<image_t::view_t::reference>::type;
    static_assert(boost::is_integral<channel_t>::value, "");
}

BOOST_AUTO_TEST_SUITE_END()
