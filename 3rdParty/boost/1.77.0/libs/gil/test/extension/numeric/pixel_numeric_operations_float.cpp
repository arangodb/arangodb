//
// Copyright 2013 Krzysztof Czainski
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cmath>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

// FIXME: Remove when https://github.com/boostorg/core/issues/38 happens
#define BOOST_GIL_TEST_IS_CLOSE(a, b, epsilon) BOOST_TEST_LT(std::fabs((a) - (b)), (epsilon))

void test_plus()
{
    gil::rgb8_pixel_t a(10, 20, 30);
    gil::bgr8_pixel_t b(30, 20, 10);

    gil::pixel_plus_t<gil::rgb8_pixel_t, gil::bgr8_pixel_t, gil::rgb8_pixel_t> op;
    gil::rgb8_pixel_t c = op(a, b);

    BOOST_TEST_EQ(get_color(c, gil::red_t()), 20);
    BOOST_TEST_EQ(get_color(c, gil::green_t()), 40);
    BOOST_TEST_EQ(get_color(c, gil::blue_t()), 60);

    gil::pixel_plus_t<gil::rgb8_pixel_t, gil::bgr8_pixel_t, gil::bgr8_pixel_t> op2;
    gil::bgr8_pixel_t d = op2(a, b);

    BOOST_TEST_EQ(get_color(d, gil::red_t()), 20);
    BOOST_TEST_EQ(get_color(d, gil::green_t()), 40);
    BOOST_TEST_EQ(get_color(d, gil::blue_t()), 60);
}

void test_multiply()
{
    gil::rgb32f_pixel_t a(1.f, 2.f, 3.f);
    gil::bgr32f_pixel_t b(2.f, 2.f, 2.f);

    gil::pixel_multiply_t<
        gil::rgb32f_pixel_t, gil::bgr32f_pixel_t, gil::rgb32f_pixel_t>
        op;
    gil::rgb32f_pixel_t c = op(a, b);

    float epsilon = 1e-6f;
    BOOST_GIL_TEST_IS_CLOSE(get_color(c, gil::red_t()), 2.f, epsilon);
    BOOST_GIL_TEST_IS_CLOSE(get_color(c, gil::green_t()), 4.f, epsilon);
    BOOST_GIL_TEST_IS_CLOSE(get_color(c, gil::blue_t()), 6.f, epsilon);
}

void test_divide()
{
    // integer
    {
        gil::rgb8_pixel_t a(10, 20, 30);
        gil::bgr8_pixel_t b(2, 2, 2);

        gil::pixel_divide_t<gil::rgb8_pixel_t, gil::bgr8_pixel_t, gil::rgb8_pixel_t> op;
        gil::rgb32f_pixel_t c = op(a, b);

        BOOST_TEST_EQ(get_color(c, gil::red_t()), 5);
        BOOST_TEST_EQ(get_color(c, gil::green_t()), 10);
        BOOST_TEST_EQ(get_color(c, gil::blue_t()), 15);
    }

    // float
    {
        gil::rgb32f_pixel_t a(1.f, 2.f, 3.f);
        gil::bgr32f_pixel_t b(2.f, 2.f, 2.f);

        gil::pixel_divide_t<gil::rgb32f_pixel_t, gil::bgr32f_pixel_t, gil::rgb32f_pixel_t> op;
        gil::rgb32f_pixel_t c = op(a, b);

        float epsilon = 1e-6f;
        BOOST_GIL_TEST_IS_CLOSE(get_color(c, gil::red_t()), 0.5f, epsilon);
        BOOST_GIL_TEST_IS_CLOSE(get_color(c, gil::green_t()), 1.f, epsilon);
        BOOST_GIL_TEST_IS_CLOSE(get_color(c, gil::blue_t()), 1.5f, epsilon);
    }
}

int main()
{
    test_plus();
    test_multiply();
    test_divide();

    return ::boost::report_errors();
}
