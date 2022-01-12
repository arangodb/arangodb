//
// Copyright 2013 Christian Henning
// Copyright 2020 Mateusz Loskot <mateusz@loskot.net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/hsv.hpp>

#include <boost/core/lightweight_test.hpp>

#include <iostream>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_rgb_to_hsv()
{
    gil::rgb8_pixel_t p{128, 0, 128};
    gil::hsv32f_pixel_t h;
    gil::color_convert(p, h);

    BOOST_TEST_GT(gil::get_color(h, gil::hsv_color_space::hue_t()), 0.80);       // 0.83333331
    BOOST_TEST_LT(gil::get_color(h, gil::hsv_color_space::hue_t()), 0.85);
    BOOST_TEST_GE(gil::get_color(h, gil::hsv_color_space::saturation_t()), 1.0); // 1.00000000
    BOOST_TEST_LT(gil::get_color(h, gil::hsv_color_space::saturation_t()), 1.1);
    BOOST_TEST_GE(gil::get_color(h, gil::hsv_color_space::value_t()), 0.50);     // 0.50196081
    BOOST_TEST_LT(gil::get_color(h, gil::hsv_color_space::value_t()), 0.51);
}

void test_hsv_to_rgb()
{
    gil::rgb8_pixel_t p(128, 0, 128);
    gil::hsv32f_pixel_t h;
    gil::color_convert(p, h);

    gil::rgb8_pixel_t b;
    gil::color_convert(h, b);
    BOOST_TEST_EQ(b, gil::rgb8_pixel_t(128, 0, 128));
}


void test_image_assign_hsv()
{
    std::ptrdiff_t const w = 320;
    std::ptrdiff_t const h = 240;
    gil::hsv32f_image_t hsv_img(w, h);

    for (std::ptrdiff_t y = 0; y < h; y++)
    {
        gil::hsv32f_view_t::x_iterator hsv_x_it = view(hsv_img).row_begin(y);
        float v = static_cast<float>(h - y) / h;
        for (std::ptrdiff_t x = 0; x < w; x++)
        {
            float const hue = (x + 1.f) / w;
            gil::hsv32f_pixel_t const p(hue, 1.0, v);
            hsv_x_it[x] = p;
            BOOST_TEST_EQ(gil::view(hsv_img)(x, y), p);
        }
    }
}

void test_copy_pixels_rgb_to_hsv()
{
    gil::rgb8_image_t rgb_img(320, 240);
    gil::rgb8_pixel_t rgb_pix(64, 32, 64);
    gil::fill_pixels(view(rgb_img), rgb_pix);
    gil::hsv32f_image_t hsv_img(view(rgb_img).dimensions());
    gil::copy_pixels(gil::color_converted_view<gil::hsv32f_pixel_t>(view(rgb_img)), view(hsv_img));

    auto view = gil::view(hsv_img);
    for (auto it = view.begin(), end = view.end(); it != end; ++it)
    {
        auto h = *it;
        BOOST_TEST_GT(gil::get_color(h, gil::hsv_color_space::hue_t()), 0.80);       // 0.8333333
        BOOST_TEST_LT(gil::get_color(h, gil::hsv_color_space::hue_t()), 0.85);
        BOOST_TEST_GE(gil::get_color(h, gil::hsv_color_space::saturation_t()), 0.5); // 0.5000000
        BOOST_TEST_LT(gil::get_color(h, gil::hsv_color_space::saturation_t()),  0.51);
        BOOST_TEST_GT(gil::get_color(h, gil::hsv_color_space::value_t()), 0.25);     // 0.25
        BOOST_TEST_LT(gil::get_color(h, gil::hsv_color_space::value_t()), 0.26);
    }
}

int main()
{
    test_rgb_to_hsv();
    test_hsv_to_rgb();
    test_image_assign_hsv();
    test_copy_pixels_rgb_to_hsv();

    return ::boost::report_errors();
}
