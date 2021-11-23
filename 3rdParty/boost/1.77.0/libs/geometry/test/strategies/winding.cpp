// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014-2020.
// Modifications copyright (c) 2014-2020 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <strategies/test_within.hpp>


template <typename Point>
void test_cartesian()
{
    typedef bg::model::polygon<Point> polygon;

    std::string const box = "POLYGON((0 0,0 2,2 2,2 0,0 0))";
    std::string const triangle = "POLYGON((0 0,0 4,6 0,0 0))";
    std::string const with_hole = "POLYGON((0 0,0 3,3 3,3 0,0 0),(1 1,2 1,2 2,1 2,1 1))";

    bg::strategy::within::cartesian_winding<> s;


    test_geometry<Point, polygon>("b1", "POINT(1 1)", box, s, true);
    test_geometry<Point, polygon>("b2", "POINT(3 3)", box, s, false);

    // Test ALL corners (officialy false but some strategies might answer true)
    test_geometry<Point, polygon>("b3a", "POINT(0 0)", box, s, false);
    test_geometry<Point, polygon>("b3b", "POINT(0 2)", box, s, false);
    test_geometry<Point, polygon>("b3c", "POINT(2 2)", box, s, false);
    test_geometry<Point, polygon>("b3d", "POINT(2 0)", box, s, false);

    // Test ALL sides (officialy false but some strategies might answer true)
    test_geometry<Point, polygon>("b4a", "POINT(0 1)", box, s, false);
    test_geometry<Point, polygon>("b4b", "POINT(1 2)", box, s, false);
    test_geometry<Point, polygon>("b4c", "POINT(2 1)", box, s, false);
    test_geometry<Point, polygon>("b4d", "POINT(1 0)", box, s, false);


    test_geometry<Point, polygon>("t1", "POINT(1 1)", triangle, s, true);
    test_geometry<Point, polygon>("t2", "POINT(3 3)", triangle, s, false);

    test_geometry<Point, polygon>("t3a", "POINT(0 0)", triangle, s, false);
    test_geometry<Point, polygon>("t3b", "POINT(0 4)", triangle, s, false);
    test_geometry<Point, polygon>("t3c", "POINT(5 0)", triangle, s, false);

    test_geometry<Point, polygon>("t4a", "POINT(0 2)", triangle, s, false);
    test_geometry<Point, polygon>("t4b", "POINT(3 2)", triangle, s, false);
    test_geometry<Point, polygon>("t4c", "POINT(2 0)", triangle, s, false);


    test_geometry<Point, polygon>("h1", "POINT(0.5 0.5)", with_hole, s, true);
    test_geometry<Point, polygon>("h2a", "POINT(1.5 1.5)", with_hole, s, false);
    test_geometry<Point, polygon>("h2b", "POINT(5 5)", with_hole, s, false);

    test_geometry<Point, polygon>("h3a", "POINT(1 1)", with_hole, s, false);
    test_geometry<Point, polygon>("h3b", "POINT(2 2)", with_hole, s, false);
    test_geometry<Point, polygon>("h3c", "POINT(0 0)", with_hole, s, false);

    test_geometry<Point, polygon>("h4a", "POINT(1 1.5)", with_hole, s, false);
    test_geometry<Point, polygon>("h4b", "POINT(1.5 2)", with_hole, s, false);

    // Lying ON (one of the sides of) interior ring
    test_geometry<Point, polygon>("#77-1", "POINT(6 3.5)",
        "POLYGON((5 3,5 4,4 4,4 5,3 5,3 6,5 6,5 5,7 5,7 6,8 6,8 5,9 5,9 2,8 2,8 1,7 1,7 2,5 2,5 3),(6 3,8 3,8 4,6 4,6 3))",
        s, false);
}

template <typename T>
void test_spherical()
{
    typedef bg::model::point<T, 2, bg::cs::spherical_equatorial<bg::degree> > point;
    typedef bg::model::polygon<point> polygon;

    bg::strategy::within::spherical_winding<> s;


    // Ticket #9354
    test_geometry<point, polygon>(
        "#9354",
        "POINT(-78.1239 25.9556)",
        "POLYGON((-97.08466667 25.95683333, -97.13683333 25.954, -97.1 26, -97.08466667 25.95683333))",
        s,
        false);

    test_geometry<point, polygon>(
        "sph1N",
        "POINT(0 10.001)",
        "POLYGON((-10 10, 10 10, 10 -10, -10 -10, -10 10))",
        s,
        bg::strategy::side::spherical_side_formula<>::apply(
            point(-10, 10),
            point(10, 10),
            point(0, (T)10.001)) == -1 // right side
        /*true*/);
    test_geometry<point, polygon>(
        "sph1S",
        "POINT(0 -10.001)",
        "POLYGON((-10 10, 10 10, 10 -10, -10 -10, -10 10))",
        s,
        bg::strategy::side::spherical_side_formula<>::apply(
              point(10, -10),
              point(-10, -10),
              point(0, (T)-10.001)) == -1 // right side
      /*true*/);

    test_geometry<point, polygon>(
        "sph2S",
        "POINT(0 10.001)",
        "POLYGON((-10 20, 10 20, 10 10, -10 10, -10 20))",
        s,
        bg::strategy::side::spherical_side_formula<>::apply(
              point(10, 10),
              point(-10, 10),
              point(0, (T)10.001)) == -1 // right side
        /*false*/);

    test_geometry<point, polygon>(
        "sph3N",
        "POINT(0 10)",
        "POLYGON((-10 10, 10 10, 10 -10, -10 -10, -10 10))",
        s,
        bg::strategy::side::spherical_side_formula<>::apply(
            point(-10, 10),
            point(10, 10),
            point(0, (T)10.001)) == -1 // right side
        /*true*/);
    test_geometry<point, polygon>(
        "sph3S",
        "POINT(0 -10)",
        "POLYGON((-10 10, 10 10, 10 -10, -10 -10, -10 10))",
        s,
        bg::strategy::side::spherical_side_formula<>::apply(
              point(10, -10),
              point(-10, -10),
              point(0, (T)-10.001)) == -1 // right side
      /*true*/);

    test_geometry<point, polygon>(
        "sphEq1",
        "POINT(179 10)",
        "POLYGON((170 10, -170 10, -170 0, 170 0, 170 10))",
        s,
        true,
        false);
    test_geometry<point, polygon>(
        "sphEq2",
        "POINT(179 10)",
        "POLYGON((170 20, -170 20, -170 10, 170 10, 170 20))",
        s,
        false,
        false);
    test_geometry<point, polygon>(
        "sphEq3",
        "POINT(-179 10)",
        "POLYGON((170 10, -170 10, -170 0, 170 0, 170 10))",
        s,
        true,
        false);
    test_geometry<point, polygon>(
        "sphEq4",
        "POINT(-179 10)",
        "POLYGON((170 20, -170 20, -170 10, 170 10, 170 20))",
        s,
        false,
        false);

    test_geometry<point, polygon>(
        "sphEq5",
        "POINT(169 10)",
        "POLYGON((170 20, -170 20, -170 10, 170 10, 170 20))",
        s,
        false,
        false);
    test_geometry<point, polygon>(
        "sphEq6",
        "POINT(-169 10)",
        "POLYGON((170 20, -170 20, -170 10, 170 10, 170 20))",
        s,
        false,
        false);
    test_geometry<point, polygon>(
        "sphEq7",
        "POINT(169 10)",
        "POLYGON((170 10, -170 10, -170 0, 170 0, 170 10))",
        s,
        false,
        false);
    test_geometry<point, polygon>(
        "sphEq8",
        "POINT(-169 10)",
        "POLYGON((170 10, -170 10, -170 0, 170 0, 170 10))",
        s,
        false,
        false);
}

int test_main(int, char* [])
{
    test_cartesian<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_cartesian<bg::model::point<double, 2, bg::cs::cartesian> >();

    test_spherical<float>();
    test_spherical<double>();

    return 0;
}
