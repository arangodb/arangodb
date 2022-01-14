// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#define BOOST_GEOMETRY_TEST_GEO_INTERSECTION_TEST_SIMILAR
#include "segment_intersection_sph.hpp"


#include <boost/geometry/strategies/spherical/intersection.hpp>


template <typename S, typename P>
void test_spherical_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                             char m, std::size_t expected_count,
                             std::string const& ip0_wkt = "", std::string const& ip1_wkt = "")
{
    bg::strategy::intersection::spherical_segments<> strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt);
}

template <typename T>
void test_spherical()
{
    typedef bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > point_t;
    typedef bg::model::segment<point_t> segment_t;

    // crossing   X
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-45 -45, 45 45)", "SEGMENT(-45 45, 45 -45)", 'i', 1, "POINT(0 0)");
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-45 -45, 45 45)", "SEGMENT(45 -45, -45 45)", 'i', 1, "POINT(0 0)");
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(45 45, -45 -45)", "SEGMENT(-45 45, 45 -45)", 'i', 1, "POINT(0 0)");
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(45 45, -45 -45)", "SEGMENT(45 -45, -45 45)", 'i', 1, "POINT(0 0)");
    // crossing   X
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-1 1, 1 -1)", 'i', 1, "POINT(0 0)");
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(1 -1, -1 1)", 'i', 1, "POINT(0 0)");
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(1 1, -1 -1)", "SEGMENT(-1 1, 1 -1)", 'i', 1, "POINT(0 0)");
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(1 1, -1 -1)", "SEGMENT(1 -1, -1 1)", 'i', 1, "POINT(0 0)");

    // equal
    //   //
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-45 -45, 45 45)", "SEGMENT(-45 -45, 45 45)", 'e', 2, "POINT(-45 -45)", "POINT(45 45)");
    //   //
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-45 -45, 45 45)", "SEGMENT(45 45, -45 -45)", 'e', 2, "POINT(-45 -45)", "POINT(45 45)");

    // starting outside s1
    //    /
    //   |
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-2 -2, -1 -1)", 'a', 1, "POINT(-1 -1)");
    //   /
    //  /|
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-2 -2, 0 0)", 'm', 1, "POINT(0 0)");
    //   /|
    //  / |
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-2 -2, 1 1)", 't', 1, "POINT(1 1)");
    //   |/
    //  /|
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-2 -2, 2 2)", 'i', 1, "POINT(0 0)");
    //       ------
    // ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-2 0, -1 0)", 'a', 1, "POINT(-1 0)");
    //    ------
    // ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-2 0, 0 0)", 'c', 2, "POINT(-1 0)", "POINT(0 0)");
    //    ------
    // ---------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-2 0, 1 0)", 'c', 2, "POINT(-1 0)", "POINT(1 0)");
    //    ------
    // ------------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-2 0, 2 0)", 'c', 2, "POINT(-1 0)", "POINT(1 0)");

    // starting at s1
    //  /
    // //
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-1 -1, 0 0)", 'c', 2, "POINT(-1 -1)", "POINT(0 0)");
    //  //
    // //
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-1 -1, 1 1)", 'e', 2, "POINT(-1 -1)", "POINT(1 1)");
    // | /
    // |/
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-1 -1, 2 2)", 'f', 1, "POINT(-1 -1)");
    // ------
    // ---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-1 0, 0 0)", 'c', 2, "POINT(-1 0)", "POINT(0 0)");
    // ------
    // ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-1 0, 1 0)", 'e', 2, "POINT(-1 0)", "POINT(1 0)");
    // ------
    // ---------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-1 0, 2 0)", 'c', 2, "POINT(-1 0)", "POINT(1 0)");
    
    // starting inside
    //   //
    //  /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(0 0, 1 1)", 'c', 2, "POINT(0 0)", "POINT(1 1)");
    //   |/
    //   /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(0 0, 2 2)", 's', 1, "POINT(0 0)");
    // ------
    //    ---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(0 0, 1 0)", 'c', 2, "POINT(0 0)", "POINT(1 0)");
    // ------
    //    ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(0 0, 2 0)", 'c', 2, "POINT(0 0)", "POINT(1 0)");
    
    // starting at p2
    //    |
    //   /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(1 1, 2 2)", 'a', 1, "POINT(1 1)");
    // ------
    //       ---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(1 0, 2 0)", 'a', 1, "POINT(1 0)");

    // disjoint, crossing
    //     /
    //  |
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-3 -3, -2 -2)", 'd', 0);
    //     |
    //  /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(2 2, 3 3)", 'd', 0);
    // disjoint, collinear
    //          ------
    // ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-3 0, -2 0)", 'd', 0);
    // ------
    //           ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(2 0, 3 0)", 'd', 0);

    // degenerated
    //    /
    // *
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-2 -2, -2 -2)", 'd', 0);
    //    /
    //   *
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(-1 -1, -1 -1)", '0', 1, "POINT(-1 -1)");
    //    /
    //   *
    //  /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(0 0, 0 0)", '0', 1, "POINT(0 0)");
    //    *
    //   /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(1 1, 1 1)", '0', 1, "POINT(1 1)");
    //       *
    //   /
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 -1, 1 1)", "SEGMENT(2 2, 2 2)", 'd', 0);
    // similar to above, collinear
    // *   ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-2 0, -2 0)", 'd', 0);
    //    *------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(-1 0, -1 0)", '0', 1, "POINT(-1 0)");
    //    ---*---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(0 0, 0 0)", '0', 1, "POINT(0 0)");
    //    ------*
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(1 0, 1 0)", '0', 1, "POINT(1 0)");
    //    ------   *
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 0, 1 0)", "SEGMENT(2 0, 2 0)", 'd', 0);

    // Northern hemisphere
    // ---   ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-3 50, -2 50)", 'd', 0);
    //    ------
    // ---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-2 50, -1 50)", 'a', 1, "POINT(-1 50)");
    //  \/
    //  /\                   (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-2 50, 0 50)", 'i', 1, "POINT(-0.5 50.0032229484023)");
    //  ________
    // /   _____\            (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-2 50, 1 50)", 't', 1, "POINT(1 50)");
    //  _________
    // /  _____  \           (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-2 50, 2 50)", 'd', 0);
    //  ______
    // /___   \              (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-1 50, 0 50)", 'f', 1, "POINT(-1 50)");
    // ------
    // ------
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-1 50, 1 50)", 'e', 2, "POINT(-1 50)", "POINT(1 50)");
    //  ________
    // /_____   \            (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(-1 50, 2 50)", 'f', 1, "POINT(-1 50)");
    //  ______
    // /   ___\              (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(0 50, 1 50)", 't', 1, "POINT(1 50)");
    //  \/
    //  /\                   (avoid multi-line comment)
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(0 50, 2 50)", 'i', 1, "POINT(0.5 50.0032229484023)");
    // ------
    //       ---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(1 50, 2 50)", 'a', 1, "POINT(1 50)");
    // ------   ---
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(-1 50, 1 50)", "SEGMENT(2 50, 3 50)", 'd', 0);

    // ___|
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(0 0, 1 0)", "SEGMENT(1 0, 1 1)", 'a', 1, "POINT(1 0)");
    // ___|
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(1 0, 1 1)", "SEGMENT(0 0, 1 0)", 'a', 1, "POINT(1 0)");

    //   |/
    //  /|
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(10 -1, 20 1)", "SEGMENT(12.5 -1, 12.5 1)", 'i', 1, "POINT(12.5 -0.50051443471392)");
    //   |/
    //  /|
    test_spherical_strategy<segment_t, point_t>(
        "SEGMENT(10 -1, 20 1)", "SEGMENT(17.5 -1, 17.5 1)", 'i', 1, "POINT(17.5 0.50051443471392)");
}

template <typename T>
void test_spherical_radian()
{
    typedef bg::model::point<T, 2, bg::cs::spherical_equatorial<bg::radian> > point_t;
    typedef bg::model::segment<point_t> segment_t;

    bg::strategy::intersection::spherical_segments<> strategy;

    // https://github.com/boostorg/geometry/issues/470
    point_t p0(0.00001, 0.00001);
    point_t p1(0.00001, 0.00005);
    point_t p2(0.00005, 0.00005);
    segment_t s1(p0, p1);
    segment_t s2(p1, p2);    
    test_strategy_one(s1, s1, strategy, 'e', 2, p0, p1);
    test_strategy_one(s2, s2, strategy, 'e', 2, p1, p2);
}

int test_main(int, char* [])
{
    //test_spherical<float>();
    test_spherical<double>();

    test_spherical_radian<double>();

    return 0;
}
