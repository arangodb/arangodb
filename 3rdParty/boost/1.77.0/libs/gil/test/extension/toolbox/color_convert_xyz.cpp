//
// Copyright 2013 Christian Henning
// Copyright 2013 Davide Anastasia <davideanastasia@users.sourceforge.net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/color_spaces/xyz.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>
#include <limits>

#ifndef BOOST_GIL_TEST_DEBUG_OSTREAM
#include <iostream>
#define BOOST_GIL_TEST_DEBUG_OSTREAM std::cout
#endif

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

const float SKEW = 0.0001f;

void test_rgb32f_xyz32f_1()
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb32f_pixel_t p32f(.934351f, 0.785446f, .105858f), p32f_b;
    gil::color_convert(p32f, xyz32f);
    gil::color_convert(xyz32f, p32f_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << p32f[0] << " "
        << p32f[1] << " "
        << p32f[2] << " -> "
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << p32f_b[0] << " "
        << p32f_b[1] << " "
        << p32f_b[2]
        << '\n';

    BOOST_TEST_LT(std::fabs(p32f[0] - p32f_b[0]), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[1] - p32f_b[1]), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[2] - p32f_b[2]), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f[0] - 0.562669), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f[1] - 0.597462), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f[2] - 0.096050), SKEW);
}

void test_rgb32f_xyz32f_2()
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb32f_pixel_t p32f(.694617f, 0.173810f, 0.218710f), p32f_b;
    gil::color_convert(p32f, xyz32f);
    gil::color_convert(xyz32f, p32f_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << p32f[0] << " "
        << p32f[1] << " "
        << p32f[2] << " -> "
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << p32f_b[0] << " "
        << p32f_b[1] << " "
        << p32f_b[2]
        << '\n';

    BOOST_TEST_LT(std::fabs(p32f[0] - p32f_b[0]), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[1] - p32f_b[1]), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[2] - p32f_b[2]), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f[0] - 0.197823), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f[1] - 0.114731), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f[2] - 0.048848), SKEW);
}

void test_xyz32f_rgb32f_1()
{
    gil::xyz32f_pixel_t xyz32f(.332634f, .436288f, .109853f), xyz32f_b;
    gil::rgb32f_pixel_t p32f;
    gil::color_convert(xyz32f, p32f);
    gil::color_convert(p32f, xyz32f_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << p32f[0] << " "
        << p32f[1] << " "
        << p32f[2] << " -> "
        << xyz32f_b[0] << " "
        << xyz32f_b[1] << " "
        << xyz32f_b[2]
        << '\n';

    BOOST_TEST_LT(std::fabs(xyz32f_b[0] - xyz32f[0]), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f_b[1] - xyz32f[1]), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f_b[2] - xyz32f[2]), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[0] - 0.628242), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[1] - 0.735771), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[2] - 0.236473), SKEW);
}

void test_xyz32f_rgb32f_2()
{
    gil::xyz32f_pixel_t xyz32f(.375155f, .352705f, .260025f), xyz32f_b;
    gil::rgb32f_pixel_t p32f;
    gil::color_convert(xyz32f, p32f);
    gil::color_convert(p32f, xyz32f_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << p32f[0] << " "
        << p32f[1] << " "
        << p32f[2] << " -> "
        << xyz32f_b[0] << " "
        << xyz32f_b[1] << " "
        << xyz32f_b[2]
        << '\n';

    BOOST_TEST_LT(std::fabs(xyz32f_b[0] - xyz32f[0]), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f_b[1] - xyz32f[1]), SKEW);
    BOOST_TEST_LT(std::fabs(xyz32f_b[2] - xyz32f[2]), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[0] - 0.763580), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[1] - 0.591622), SKEW);
    BOOST_TEST_LT(std::fabs(p32f[2] - 0.510392), SKEW);
}

void test_rgb8u_xyz32f_1()
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb8_pixel_t p8u(177, 44, 56), p8u_b;
    gil::color_convert(p8u, xyz32f);
    gil::color_convert(xyz32f, p8u_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << static_cast<int>(p8u[0]) << " "
        << static_cast<int>(p8u[1]) << " "
        << static_cast<int>(p8u[2]) << " -> "
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << static_cast<int>(p8u_b[0]) << " "
        << static_cast<int>(p8u_b[1]) << " "
        << static_cast<int>(p8u_b[2])
        << '\n';

    BOOST_TEST_EQ(p8u[0], p8u_b[0]);
    BOOST_TEST_EQ(p8u[1], p8u_b[1]);
    BOOST_TEST_EQ(p8u[2], p8u_b[2]);
}

void test_rgb8u_xyz32f_2()
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb8_pixel_t p8u(72, 90, 165), p8u_b;
    gil::color_convert(p8u, xyz32f);
    gil::color_convert(xyz32f, p8u_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << static_cast<int>(p8u[0]) << " "
        << static_cast<int>(p8u[1]) << " "
        << static_cast<int>(p8u[2]) << " -> "
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << static_cast<int>(p8u_b[0]) << " "
        << static_cast<int>(p8u_b[1]) << " "
        << static_cast<int>(p8u_b[2])
        << '\n';

    BOOST_TEST_EQ(p8u[0], p8u_b[0]);
    BOOST_TEST_EQ(p8u[1], p8u_b[1]);
    BOOST_TEST_EQ(p8u[2], p8u_b[2]);
}

void test_rgb16u_xyz32f_1()
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb16_pixel_t p16u(12564, 20657, 200), p16u_b;
    gil::color_convert(p16u, xyz32f);
    gil::color_convert(xyz32f, p16u_b);

    BOOST_GIL_TEST_DEBUG_OSTREAM
        << static_cast<int>(p16u[0]) << " "
        << static_cast<int>(p16u[1]) << " "
        << static_cast<int>(p16u[2]) << " -> "
        << xyz32f[0] << " "
        << xyz32f[1] << " "
        << xyz32f[2] << " -> "
        << static_cast<int>(p16u_b[0]) << " "
        << static_cast<int>(p16u_b[1]) << " "
        << static_cast<int>(p16u_b[2])
        << '\n';

    BOOST_TEST_EQ(p16u[0], p16u_b[0]);
    BOOST_TEST_EQ(p16u[1], p16u_b[1]);
    BOOST_TEST_EQ(p16u[2], p16u_b[2]);
}

int main()
{
    test_rgb32f_xyz32f_1();
    test_rgb32f_xyz32f_2();
    test_xyz32f_rgb32f_1();
    test_xyz32f_rgb32f_2();
    test_rgb8u_xyz32f_1();
    test_rgb8u_xyz32f_2();
    test_rgb16u_xyz32f_1();

    return ::boost::report_errors();
}
