//
// Copyright 2019 Miral Shah <miralshah2211@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/algorithm.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/image_processing/threshold.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

int height = 2;
int width = 2;

gil::gray8_image_t original_gray(width, height), otsu_gray(width, height),
expected_gray(width, height);

gil::rgb8_image_t original_rgb(width, height), otsu_rgb(width, height), expected_rgb(width, height);

void fill_gray()
{
    gil::view(original_gray)(0, 0) = gil::gray8_pixel_t(56);
    gil::view(original_gray)(1, 0) = gil::gray8_pixel_t(89);
    gil::view(original_gray)(0, 1) = gil::gray8_pixel_t(206);
    gil::view(original_gray)(1, 1) = gil::gray8_pixel_t(139);
}

void fill_rgb()
{
    gil::view(original_rgb)(0, 0) = gil::rgb8_pixel_t(15, 158, 150);
    gil::view(original_rgb)(1, 0) = gil::rgb8_pixel_t(200, 175, 150);
    gil::view(original_rgb)(0, 1) = gil::rgb8_pixel_t(230, 170, 150);
    gil::view(original_rgb)(1, 1) = gil::rgb8_pixel_t(25, 248, 150);
}

void test_gray_regular()
{
    gil::view(expected_gray)(0, 0) = gil::gray8_pixel_t(0);
    gil::view(expected_gray)(1, 0) = gil::gray8_pixel_t(0);
    gil::view(expected_gray)(0, 1) = gil::gray8_pixel_t(255);
    gil::view(expected_gray)(1, 1) = gil::gray8_pixel_t(255);

    gil::threshold_optimal(
        gil::view(original_gray),
        gil::view(otsu_gray),
        gil::threshold_optimal_value::otsu
    );

    BOOST_TEST(gil::equal_pixels(gil::view(otsu_gray), gil::view(expected_gray)));
}

void test_gray_inverse()
{
    gil::view(expected_gray)(0, 0) = gil::gray8_pixel_t(255);
    gil::view(expected_gray)(1, 0) = gil::gray8_pixel_t(255);
    gil::view(expected_gray)(0, 1) = gil::gray8_pixel_t(0);
    gil::view(expected_gray)(1, 1) = gil::gray8_pixel_t(0);

    gil::threshold_optimal(
        gil::view(original_gray),
        gil::view(otsu_gray),
        gil::threshold_optimal_value::otsu,
        gil::threshold_direction::inverse
    );

    BOOST_TEST(gil::equal_pixels(gil::view(otsu_gray), gil::view(expected_gray)));
}

void test_rgb_regular()
{
    gil::view(expected_rgb)(0, 0) = gil::rgb8_pixel_t(0, 0, 255);
    gil::view(expected_rgb)(1, 0) = gil::rgb8_pixel_t(255, 0, 255);
    gil::view(expected_rgb)(0, 1) = gil::rgb8_pixel_t(255, 0, 255);
    gil::view(expected_rgb)(1, 1) = gil::rgb8_pixel_t(0, 255, 255);

    gil::threshold_optimal(
        gil::view(original_rgb),
        gil::view(otsu_rgb),
        gil::threshold_optimal_value::otsu
    );

    BOOST_TEST(gil::equal_pixels(gil::view(otsu_rgb), gil::view(expected_rgb)));
}

void test_rgb_inverse()
{
    gil::view(expected_rgb)(0, 0) = gil::rgb8_pixel_t(255, 255, 0);
    gil::view(expected_rgb)(1, 0) = gil::rgb8_pixel_t(0, 255, 0);
    gil::view(expected_rgb)(0, 1) = gil::rgb8_pixel_t(0, 255, 0);
    gil::view(expected_rgb)(1, 1) = gil::rgb8_pixel_t(255, 0, 0);

    gil::threshold_optimal(
        gil::view(original_rgb),
        gil::view(otsu_rgb),
        gil::threshold_optimal_value::otsu,
        gil::threshold_direction::inverse
    );

    BOOST_TEST(gil::equal_pixels(gil::view(otsu_rgb), gil::view(expected_rgb)));
}

int main()
{
    fill_gray();
    fill_rgb();

    test_gray_regular();
    test_gray_inverse();
    test_rgb_regular();
    test_rgb_inverse();

    return boost::report_errors();
}
