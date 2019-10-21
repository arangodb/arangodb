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

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( get_num_bits_test )
{
    using image_t = bit_aligned_image4_type<4, 4, 4, 4, rgb_layout_t>::type;

    using channel_t = channel_type<image_t::view_t::reference>::type;
    static_assert(get_num_bits<channel_t>::value == 4, "");

    using const_channel_t = channel_type<image_t::const_view_t::reference>::type;
    static_assert(get_num_bits<const_channel_t>::value == 4, "");

    using bits_t = packed_channel_value<23>;
    static_assert(get_num_bits<bits_t>::value == 23, "");
    static_assert(get_num_bits<bits_t const>::value == 23, "");

    static_assert(get_num_bits<unsigned char >::value == 8, "");
    static_assert(get_num_bits<unsigned char const>::value == 8, "");

    static_assert(get_num_bits<channel_type<gray8_image_t::view_t::value_type>::type>::value == 8, "");
    static_assert(get_num_bits<channel_type<rgba32_image_t::view_t::value_type>::type>::value == 32, "");
}

BOOST_AUTO_TEST_SUITE_END()

