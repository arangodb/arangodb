//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/get_pixel_type.hpp>

#include <type_traits>

namespace gil = boost::gil;

void test_bit_aligned_image()
{
    using image_t = gil::bit_aligned_image3_type<4, 15, 4, gil::rgb_layout_t>::type;
    static_assert(std::is_same
        <
            gil::get_pixel_type<image_t::view_t>::type,
            image_t::view_t::reference
        >::value, "");
}

void test_rgb8_image()
{
    using image_t = gil::rgb8_image_t;
    static_assert(std::is_same
        <
            gil::get_pixel_type<image_t::view_t>::type,
            image_t::view_t::value_type
        >::value, "");
}

int main()
{
    test_bit_aligned_image();
    test_rgb8_image();
}
