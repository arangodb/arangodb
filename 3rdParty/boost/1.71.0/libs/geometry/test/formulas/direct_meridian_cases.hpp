// Boost.Geometry
// Unit Test

// Copyright (c) 2018 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_DIRECT_MERIDIAN_CASES_HPP
#define BOOST_GEOMETRY_TEST_DIRECT_MERIDIAN_CASES_HPP

struct coordinates
{
    double lon;
    double lat;
};

struct expected_results
{
    coordinates p1;
    double distance;
    bool direction;//north=true, south=false
    coordinates p2;
};

expected_results expected[] =
{
    { {0, 0}, 100000, true, {0, 0.90435537782} },
    { {0, 0}, 200000, true, {0, 1.8087062846} },
    { {0, 0}, 300000, true, {0, 2.7130880778385226826} },
    { {0, 0}, 400000, true, {0, 3.6174296804714929365} },
    { {0, 0}, 500000, true, {0, 4.521753233678117212} },
    { {0, 0}, 600000, true, {0, 5.4260542596891729872} },
    { {0, 0}, 700000, true, {0, 6.3303283037366426811} },
    { {0, 0}, 800000, true, {0, 7.234570938597034484} },
    { {0, 0}, 900000, true, {0, 8.1386639105161684427} },
    { {0, 0}, 1000000, true, {0, 9.0428195432854963087} },
    { {0, 0}, 10000000, true, {0, 89.982400761362015373} },
    { {0, 0}, 10001965, true, {0, 90} },
    { {0, 0}, 11000000, true, {180, 81.063836359} },
    { {10, 0}, 11000000, true, {-170, 81.063836359} },
    { {0, 1}, 100000, true, {0, 1.9043634563000368942} },
    { {0, 10}, 100000, true, {0, 10.904070448} },
    { {0, 80}, 100000, true, {0, 80.895552833} },
    { {0, 0}, 40010000, true, {0, 0.01932683869} },//wrap the spheroid once
    { {0, 10}, 100000, false, {0, 9.0956502718} },
    { {0, 10}, 2000000, false, {0, -8.0858305929} },
    { {0, 0}, 11000000, false, {180, -81.063836359} },
    { {0, 0}, 40010000, false, {0, -0.01932683869} } //wrap the spheroid once
};

size_t const expected_size = sizeof(expected) / sizeof(expected_results);

#endif // BOOST_GEOMETRY_TEST_DIRECT_MERIDIAN_CASES_HPP
