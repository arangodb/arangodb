//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/extension/toolbox/metafunctions/channel_type.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_bit_aligned.hpp>

#include <boost/gil/channel.hpp>
#include <boost/gil/packed_pixel.hpp>
#include <boost/gil/detail/is_channel_integral.hpp>

#include <type_traits>

namespace gil = boost::gil;

void test_channel_type()
{
    static_assert(std::is_same
        <
            unsigned char,
            gil::channel_type<gil::rgb8_pixel_t>::type
        >::value, "");

    // float32_t is a scoped_channel_value object
    static_assert(std::is_same
        <
            gil::float32_t,
            gil::channel_type<gil::rgba32f_pixel_t>::type
        >::value, "");

    // channel_type for bit_aligned images doesn't work with standard gil.
    using image_t = gil::bit_aligned_image4_type<4, 4, 4, 4, gil::rgb_layout_t>::type;
    using channel_t = gil::channel_type<image_t::view_t::reference>::type;
    static_assert(gil::detail::is_channel_integral<channel_t>::value, "");
}

int main()
{
    test_channel_type();
}
