//
// Copyright 2013 Christian Henning
// Copyright 2013 Davide Anastasia <davideanastasia@users.sourceforge.net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/lab.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/test/unit_test.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

#include <iostream>

using namespace boost;

#define TEST_CHECK_CLOSE(a, b) \
    BOOST_CHECK_CLOSE(a, b, 0.0005f)

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( Lab_to_XYZ_Test1 )
{
    gil::lab32f_pixel_t lab_pixel(40.366198f, 53.354489f, 26.117702f);
    gil::xyz32f_pixel_t xyz_pixel;

    gil::color_convert(lab_pixel, xyz_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(xyz_pixel[0]), 0.197823f);
    TEST_CHECK_CLOSE(static_cast<float>(xyz_pixel[1]), 0.114731f);
    TEST_CHECK_CLOSE(static_cast<float>(xyz_pixel[2]), 0.048848f);
}

BOOST_AUTO_TEST_CASE(Lab_to_XYZ_Test2)
{
    gil::lab32f_pixel_t lab_pixel(50, 0, 0);
    gil::xyz32f_pixel_t xyz_pixel;

    gil::color_convert(lab_pixel, xyz_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(xyz_pixel[0]), 0.175064f);
    TEST_CHECK_CLOSE(static_cast<float>(xyz_pixel[1]), 0.184187f);
    TEST_CHECK_CLOSE(static_cast<float>(xyz_pixel[2]), 0.200548f);
}

BOOST_AUTO_TEST_CASE(XYZ_to_Lab_Test1)
{
    gil::lab32f_pixel_t lab_pixel;
    gil::xyz32f_pixel_t xyz_pixel(0.085703f, 0.064716f, 0.147082f);

    gil::color_convert(xyz_pixel, lab_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[0]), 30.572438f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[1]), 23.4674f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[2]), -22.322275f);
}

BOOST_AUTO_TEST_CASE(RGB_to_Lab_Test1)
{
    gil::rgb32f_pixel_t rgb_pixel(0.75f, 0.5f, 0.25f);
    gil::lab32f_pixel_t lab_pixel;

    gil::color_convert(rgb_pixel, lab_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[0]), 58.7767f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[1]), 18.5851f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[2]), 43.7975f);
}

BOOST_AUTO_TEST_CASE(RGB_to_Lab_Test2)
{
    gil::rgb32f_pixel_t rgb_pixel(1.f, 0.f, 0.f);
    gil::lab32f_pixel_t lab_pixel;

    gil::color_convert(rgb_pixel, lab_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[0]), 53.2408f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[1]), 80.0925f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[2]), 67.2032f);
}

BOOST_AUTO_TEST_CASE(RGB_to_Lab_Test3)
{
    gil::rgb32f_pixel_t rgb_pixel(0.f, 1.f, 0.f);
    gil::lab32f_pixel_t lab_pixel;

    gil::color_convert(rgb_pixel, lab_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[0]), 87.7347f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[1]), -86.1827f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[2]), 83.1793f);
}

BOOST_AUTO_TEST_CASE(RGB_to_Lab_Test4)
{
    gil::rgb32f_pixel_t rgb_pixel(0.f, 0.f, 1.f);
    gil::lab32f_pixel_t lab_pixel;

    gil::color_convert(rgb_pixel, lab_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[0]), 32.2970f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[1]), 79.1875f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[2]), -107.8602f);
}

BOOST_AUTO_TEST_CASE(RGB_to_Lab_Test5)
{
    gil::rgb32f_pixel_t rgb_pixel(1.f, 1.f, 1.f);
    gil::lab32f_pixel_t lab_pixel;

    gil::color_convert(rgb_pixel, lab_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[0]), 100.f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[1]), 0.f);
    TEST_CHECK_CLOSE(static_cast<float>(lab_pixel[2]), 0.f);
}

BOOST_AUTO_TEST_CASE(Lab_to_RGB_Test1)
{
    gil::lab32f_pixel_t lab_pixel(75.f, 20.f, 40.f);
    gil::rgb32f_pixel_t rgb_pixel;

    gil::color_convert(lab_pixel, rgb_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[0]), 0.943240f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[1]), 0.663990f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[2]), 0.437893f);
}

BOOST_AUTO_TEST_CASE(Lab_to_RGB_Test2)
{
    gil::lab32f_pixel_t lab_pixel(100.f, 0.f, 0.f);
    gil::rgb32f_pixel_t rgb_pixel;

    gil::color_convert(lab_pixel, rgb_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[0]), 1.f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[1]), 1.f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[2]), 1.f);
}

BOOST_AUTO_TEST_CASE(Lab_to_RGB_Test3)
{
    gil::lab32f_pixel_t lab_pixel(56.8140f, -42.3665f, 10.6728f);
    gil::rgb32f_pixel_t rgb_pixel;

    gil::color_convert(lab_pixel, rgb_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[0]), 0.099999f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[1]), 0.605568f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[2]), 0.456662f);
}

BOOST_AUTO_TEST_CASE(Lab_to_RGB_Test4)
{
    gil::lab32f_pixel_t lab_pixel(50.5874f, 4.0347f, 50.5456f);
    gil::rgb32f_pixel_t rgb_pixel;

    gil::color_convert(lab_pixel, rgb_pixel);

    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[0]), 0.582705f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[1]), 0.454891f);
    TEST_CHECK_CLOSE(static_cast<float>(rgb_pixel[2]), 0.1f);
}

BOOST_AUTO_TEST_SUITE_END()
