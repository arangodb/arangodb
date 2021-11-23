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
#include <boost/gil/extension/toolbox/color_spaces/lab.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cmath>

namespace gil = boost::gil;

// FIXME: Remove when https://github.com/boostorg/core/issues/38 happens
#define BOOST_GIL_TEST_IS_CLOSE(a, b) BOOST_TEST_LT(std::fabs((a) - (b)), (0.0005f))

void test_lab_to_xyz()
{
    {
        gil::lab32f_pixel_t lab_pixel(40.366198f, 53.354489f, 26.117702f);
        gil::xyz32f_pixel_t xyz_pixel;
        gil::color_convert(lab_pixel, xyz_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(xyz_pixel[0]), 0.197823f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(xyz_pixel[1]), 0.114731f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(xyz_pixel[2]), 0.048848f);
    }
    {
        gil::lab32f_pixel_t lab_pixel(50, 0, 0);
        gil::xyz32f_pixel_t xyz_pixel;
        gil::color_convert(lab_pixel, xyz_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(xyz_pixel[0]), 0.175064f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(xyz_pixel[1]), 0.184187f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(xyz_pixel[2]), 0.200548f);
    }
}

void test_xyz_to_lab()
{
    gil::lab32f_pixel_t lab_pixel;
    gil::xyz32f_pixel_t xyz_pixel(0.085703f, 0.064716f, 0.147082f);
    gil::color_convert(xyz_pixel, lab_pixel);
    BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[0]), 30.572438f);
    BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[1]), 23.4674f);
    BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[2]), -22.322275f);
}

void test_rgb_to_lab()
{
    {
        gil::rgb32f_pixel_t rgb_pixel(0.75f, 0.5f, 0.25f);
        gil::lab32f_pixel_t lab_pixel;
        gil::color_convert(rgb_pixel, lab_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[0]), 58.7767f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[1]), 18.5851f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[2]), 43.7975f);
    }
    {
        gil::rgb32f_pixel_t rgb_pixel(1.f, 0.f, 0.f);
        gil::lab32f_pixel_t lab_pixel;
        gil::color_convert(rgb_pixel, lab_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[0]), 53.2408f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[1]), 80.0925f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[2]), 67.2032f);
    }
    {
        gil::rgb32f_pixel_t rgb_pixel(0.f, 1.f, 0.f);
        gil::lab32f_pixel_t lab_pixel;
        gil::color_convert(rgb_pixel, lab_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[0]), 87.7347f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[1]), -86.1827f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[2]), 83.1793f);
    }
    {
        gil::rgb32f_pixel_t rgb_pixel(0.f, 0.f, 1.f);
        gil::lab32f_pixel_t lab_pixel;
        gil::color_convert(rgb_pixel, lab_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[0]), 32.2970f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[1]), 79.1875f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[2]), -107.8602f);
    }
    {
        gil::rgb32f_pixel_t rgb_pixel(1.f, 1.f, 1.f);
        gil::lab32f_pixel_t lab_pixel;
        gil::color_convert(rgb_pixel, lab_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[0]), 100.f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[1]), 0.f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(lab_pixel[2]), 0.f);
    }
}

void test_lab_to_rgb()
{
    {
        gil::lab32f_pixel_t lab_pixel(75.f, 20.f, 40.f);
        gil::rgb32f_pixel_t rgb_pixel;
        gil::color_convert(lab_pixel, rgb_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[0]), 0.943240f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[1]), 0.663990f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[2]), 0.437893f);
    }
    {
        gil::lab32f_pixel_t lab_pixel(100.f, 0.f, 0.f);
        gil::rgb32f_pixel_t rgb_pixel;
        gil::color_convert(lab_pixel, rgb_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[0]), 1.f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[1]), 1.f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[2]), 1.f);
    }
    {
        gil::lab32f_pixel_t lab_pixel(56.8140f, -42.3665f, 10.6728f);
        gil::rgb32f_pixel_t rgb_pixel;
        gil::color_convert(lab_pixel, rgb_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[0]), 0.099999f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[1]), 0.605568f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[2]), 0.456662f);
    }
    {
        gil::lab32f_pixel_t lab_pixel(50.5874f, 4.0347f, 50.5456f);
        gil::rgb32f_pixel_t rgb_pixel;
        gil::color_convert(lab_pixel, rgb_pixel);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[0]), 0.582705f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[1]), 0.454891f);
        BOOST_GIL_TEST_IS_CLOSE(static_cast<float>(rgb_pixel[2]), 0.1f);
    }
}

int main()
{
    test_lab_to_xyz();
    test_xyz_to_lab();
    test_rgb_to_lab();
    test_lab_to_rgb();

    return ::boost::report_errors();
}
