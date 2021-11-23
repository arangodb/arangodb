//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_converters/gray_to_rgba.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_gray_to_rgba()
{
    gil::gray8_pixel_t a(45);
    gil::rgba8_pixel_t b;
    gil::color_convert(a, b);
    BOOST_TEST_EQ(b, gil::rgba8_pixel_t(45, 45, 45, 255));
}

int main()
{
    test_gray_to_rgba();

    return boost::report_errors();
}
