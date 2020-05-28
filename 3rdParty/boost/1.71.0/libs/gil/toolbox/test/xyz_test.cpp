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

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <iostream>
#include <limits>

using namespace boost;
using namespace std;

const float SKEW = 0.0001f;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE(rgb32f_xyz32f_1)
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb32f_pixel_t p32f(.934351f, 0.785446f, .105858f), p32f_b;
    gil::color_convert(p32f, xyz32f);
    gil::color_convert(xyz32f, p32f_b);

    BOOST_TEST_MESSAGE( p32f[0] << " "
                        << p32f[1] << " "
                        << p32f[2] << " -> "
                        << xyz32f[0] << " "
                        << xyz32f[1] << " "
                        << xyz32f[2] << " -> "
                        << p32f_b[0] << " "
                        << p32f_b[1] << " "
                        << p32f_b[2] );

    BOOST_CHECK( abs(p32f[0] - p32f_b[0]) < SKEW );
    BOOST_CHECK( abs(p32f[1] - p32f_b[1]) < SKEW );
    BOOST_CHECK( abs(p32f[2] - p32f_b[2]) < SKEW );
    BOOST_CHECK( abs( xyz32f[0] - 0.562669) < SKEW );
    BOOST_CHECK( abs( xyz32f[1] - 0.597462) < SKEW );
    BOOST_CHECK( abs( xyz32f[2] - 0.096050) < SKEW );
}

BOOST_AUTO_TEST_CASE(rgb32f_xyz32f_2)
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb32f_pixel_t p32f(.694617f, 0.173810f, 0.218710f), p32f_b;
    gil::color_convert(p32f, xyz32f);
    gil::color_convert(xyz32f, p32f_b);

    BOOST_TEST_MESSAGE( p32f[0] << " "
                        << p32f[1] << " "
                        << p32f[2] << " -> "
                        << xyz32f[0] << " "
                        << xyz32f[1] << " "
                        << xyz32f[2] << " -> "
                        << p32f_b[0] << " "
                        << p32f_b[1] << " "
                        << p32f_b[2] );

    BOOST_CHECK( abs(p32f[0] - p32f_b[0]) < SKEW );
    BOOST_CHECK( abs(p32f[1] - p32f_b[1]) < SKEW );
    BOOST_CHECK( abs(p32f[2] - p32f_b[2]) < SKEW );
    BOOST_CHECK( abs( xyz32f[0] - 0.197823) < SKEW );
    BOOST_CHECK( abs( xyz32f[1] - 0.114731) < SKEW );
    BOOST_CHECK( abs( xyz32f[2] - 0.048848) < SKEW );
}

BOOST_AUTO_TEST_CASE(xyz32f_rgb32f_1)
{
    gil::xyz32f_pixel_t xyz32f(.332634f, .436288f, .109853f), xyz32f_b;
    gil::rgb32f_pixel_t p32f;
    gil::color_convert(xyz32f, p32f);
    gil::color_convert(p32f, xyz32f_b);

    BOOST_TEST_MESSAGE( xyz32f[0] << " "
                        << xyz32f[1] << " "
                        << xyz32f[2] << " -> "
                        << p32f[0] << " "
                        << p32f[1] << " "
                        << p32f[2] << " -> "
                        << xyz32f_b[0] << " "
                        << xyz32f_b[1] << " "
                        << xyz32f_b[2] );

    BOOST_CHECK( abs(xyz32f_b[0] - xyz32f[0]) < SKEW );
    BOOST_CHECK( abs(xyz32f_b[1] - xyz32f[1]) < SKEW );
    BOOST_CHECK( abs(xyz32f_b[2] - xyz32f[2]) < SKEW );
    BOOST_CHECK( abs( p32f[0] - 0.628242) < SKEW );
    BOOST_CHECK( abs( p32f[1] - 0.735771) < SKEW );
    BOOST_CHECK( abs( p32f[2] - 0.236473) < SKEW );
}

BOOST_AUTO_TEST_CASE(xyz32f_rgb32f_2)
{
    gil::xyz32f_pixel_t xyz32f(.375155f, .352705f, .260025f), xyz32f_b;
    gil::rgb32f_pixel_t p32f;
    gil::color_convert(xyz32f, p32f);
    gil::color_convert(p32f, xyz32f_b);

    BOOST_TEST_MESSAGE( xyz32f[0] << " "
                        << xyz32f[1] << " "
                        << xyz32f[2] << " -> "
                        << p32f[0] << " "
                        << p32f[1] << " "
                        << p32f[2] << " -> "
                        << xyz32f_b[0] << " "
                        << xyz32f_b[1] << " "
                        << xyz32f_b[2] );

    BOOST_CHECK( abs(xyz32f_b[0] - xyz32f[0]) < SKEW );
    BOOST_CHECK( abs(xyz32f_b[1] - xyz32f[1]) < SKEW );
    BOOST_CHECK( abs(xyz32f_b[2] - xyz32f[2]) < SKEW );
    BOOST_CHECK( abs( p32f[0] - 0.763580) < SKEW );
    BOOST_CHECK( abs( p32f[1] - 0.591622) < SKEW );
    BOOST_CHECK( abs( p32f[2] - 0.510392) < SKEW );
}

BOOST_AUTO_TEST_CASE(rgb8u_xyz32f_1)
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb8_pixel_t p8u(177, 44, 56), p8u_b;
    gil::color_convert(p8u, xyz32f);
    gil::color_convert(xyz32f, p8u_b);

    BOOST_TEST_MESSAGE( static_cast<int>(p8u[0]) << " "
                   << static_cast<int>(p8u[1]) << " "
                   << static_cast<int>(p8u[2]) << " -> "
                   << xyz32f[0] << " "
                   << xyz32f[1] << " "
                   << xyz32f[2] << " -> "
                   << static_cast<int>(p8u_b[0]) << " "
                   << static_cast<int>(p8u_b[1]) << " "
                   << static_cast<int>(p8u_b[2]) );

    BOOST_CHECK(p8u[0] == p8u_b[0]);
    BOOST_CHECK(p8u[1] == p8u_b[1]);
    BOOST_CHECK(p8u[2] == p8u_b[2]);
}

BOOST_AUTO_TEST_CASE(rgb8u_xyz32f_2)
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb8_pixel_t p8u(72, 90, 165), p8u_b;
    gil::color_convert(p8u, xyz32f);
    gil::color_convert(xyz32f, p8u_b);

    BOOST_TEST_MESSAGE(
                      static_cast<int>(p8u[0]) << " "
                   << static_cast<int>(p8u[1]) << " "
                   << static_cast<int>(p8u[2]) << " -> "
                   << xyz32f[0] << " "
                   << xyz32f[1] << " "
                   << xyz32f[2] << " -> "
                   << static_cast<int>(p8u_b[0]) << " "
                   << static_cast<int>(p8u_b[1]) << " "
                   << static_cast<int>(p8u_b[2]) );

    BOOST_CHECK(p8u[0] == p8u_b[0]);
    BOOST_CHECK(p8u[1] == p8u_b[1]);
    BOOST_CHECK(p8u[2] == p8u_b[2]);
}

BOOST_AUTO_TEST_CASE(rgb16u_xyz32f_1)
{
    gil::xyz32f_pixel_t xyz32f;
    gil::rgb16_pixel_t p16u(12564, 20657, 200), p16u_b;
    gil::color_convert(p16u, xyz32f);
    gil::color_convert(xyz32f, p16u_b);

    BOOST_TEST_MESSAGE(
                      static_cast<int>(p16u[0]) << " "
                   << static_cast<int>(p16u[1]) << " "
                   << static_cast<int>(p16u[2]) << " -> "
                   << xyz32f[0] << " "
                   << xyz32f[1] << " "
                   << xyz32f[2] << " -> "
                   << static_cast<int>(p16u_b[0]) << " "
                   << static_cast<int>(p16u_b[1]) << " "
                   << static_cast<int>(p16u_b[2]) );

    BOOST_CHECK(p16u[0] == p16u_b[0]);
    BOOST_CHECK(p16u[1] == p16u_b[1]);
    BOOST_CHECK(p16u[2] == p16u_b[2]);
}

BOOST_AUTO_TEST_SUITE_END()
