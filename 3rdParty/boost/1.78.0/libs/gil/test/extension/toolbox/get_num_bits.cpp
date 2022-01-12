//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/channel_type.hpp>
#include <boost/gil/extension/toolbox/metafunctions/get_num_bits.hpp>

namespace gil = boost::gil;

void test_get_num_bits()
{
    using image_t = gil::bit_aligned_image4_type<4, 4, 4, 4, gil::rgb_layout_t>::type;

    using channel_t = gil::channel_type<image_t::view_t::reference>::type;
    static_assert(gil::get_num_bits<channel_t>::value == 4, "");

    using const_channel_t = gil::channel_type<image_t::const_view_t::reference>::type;
    static_assert(gil::get_num_bits<const_channel_t>::value == 4, "");

    using bits_t = gil::packed_channel_value<23>;
    static_assert(gil::get_num_bits<bits_t>::value == 23, "");
    static_assert(gil::get_num_bits<bits_t const>::value == 23, "");

    static_assert(gil::get_num_bits<unsigned char >::value == 8, "");
    static_assert(gil::get_num_bits<unsigned char const>::value == 8, "");

    using gray8_channel_t = gil::channel_type<gil::gray8_image_t::view_t::value_type>::type;
    static_assert(gil::get_num_bits<gray8_channel_t>::value == 8, "");

    using rgba32_channel_t = gil::channel_type<gil::rgba32_image_t::view_t::value_type>::type;
    static_assert(gil::get_num_bits<rgba32_channel_t>::value == 32, "");
}

int main()
{
    test_get_num_bits();
}
