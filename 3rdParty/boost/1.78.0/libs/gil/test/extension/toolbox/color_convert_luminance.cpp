//
// Copyright 2013 Christian Henning
// Copyright 2013 Davide Anastasia <davideanastasia@users.sourceforge.net>
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_converters/rgb_to_luminance.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

struct double_zero { static double apply() { return 0.0; } };
struct double_one  { static double apply() { return 1.0; } };

using gray64f_pixel_t = gil::pixel<double, gil::gray_layout_t>;
using rgb64f_pixel_t = gil::pixel<double, gil::rgb_layout_t>;

void test_rgb_to_luminance()
{
    rgb64f_pixel_t a(10, 20, 30);
    gray64f_pixel_t b;
    gil::color_convert(a, b);
    BOOST_TEST_EQ(b, b);
}

int main()
{
    test_rgb_to_luminance();

    return ::boost::report_errors();
}
