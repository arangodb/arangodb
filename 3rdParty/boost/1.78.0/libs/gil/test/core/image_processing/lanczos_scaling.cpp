//
// Copyright 2019 Olzhas Zhumabek <anonymous.from.applecity@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/image.hpp>
#include <boost/gil/image_processing/scaling.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

bool are_equal(gil::rgb8_view_t expected, gil::rgb8_view_t actual)
{
    if (expected.dimensions() != actual.dimensions())
        return false;

    for (long int y = 0; y < expected.height(); ++y)
    {
        for (long int x = 0; x < expected.width(); ++x)
        {
            if (expected(x, y) != actual(x, y))
                return false;
        }
    }

    return true;
}

void test_lanczos_black_image()
{
    const gil::point_t input_dimensions(20, 20);
    const gil::point_t output_dimensions(input_dimensions.x / 2, input_dimensions.y / 2);
    gil::rgb8_image_t image(input_dimensions, gil::rgb8_pixel_t(0, 0, 0), 0);
    // fill with values other than 0
    gil::rgb8_image_t output_image(
        output_dimensions,
        gil::rgb8_pixel_t(100, 100, 100),
        0
    );
    gil::rgb8_image_t expected(
        output_dimensions,
        gil::rgb8_pixel_t(0, 0, 0),
        0
    );

    auto view = gil::view(image);
    auto output_view = gil::view(output_image);
    auto expected_view = gil::view(expected);
    gil::scale_lanczos(view, output_view, 5);
    BOOST_TEST(are_equal(expected_view,output_view));
}

void test_lanczos_response_on_zero()
{
    //random value for a
    BOOST_TEST_EQ(gil::lanczos(0, 2), 1);
}

int main()
{
    test_lanczos_black_image();
    test_lanczos_response_on_zero();
    return boost::report_errors();
}
