//
// Copyright 2013 Christian Henning
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/gray_alpha.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_gray_alpha_to_gray()
{
    gil::gray_alpha8_pixel_t a(10, 20);
    gil::gray8_pixel_t b;
    gil::color_convert(a, b);
    BOOST_TEST_EQ(b, gil::gray8_pixel_t(1));
}

void test_gray_alpha_to_rgb()
{
    gil::gray_alpha8_pixel_t a(10, 20);
    gil::rgb8_pixel_t b;
    gil::color_convert(a, b);
    BOOST_TEST_EQ(b, gil::rgb8_pixel_t(1, 1, 1));
}

void test_gray_alpha_to_rgba()
{
    gil::gray_alpha8_pixel_t a(10, 20);
    gil::rgba8_pixel_t b;
    gil::color_convert(a, b);
    BOOST_TEST_EQ(b, gil::rgba8_pixel_t(10, 10, 10, 20));
}

int main()
{
    test_gray_alpha_to_gray();
    test_gray_alpha_to_rgb();
    test_gray_alpha_to_rgba();

    return boost::report_errors();
}
