// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <string>

#include "test_convex_hull.hpp"

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>


void test_all()
{
    typedef bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> >
        SphericalPoint;
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > GeoPoint;

    test_geometry_sph_geo
        <
            bg::model::polygon<SphericalPoint>,
            bg::model::polygon<GeoPoint>
        >("polygon((0 0,2 1,2 -1,0 0))", 4, 4,
          0.00060931217091786914, 24621363089.253811);

    test_geometry_sph_geo
        <
            bg::model::polygon<SphericalPoint>,
            bg::model::polygon<GeoPoint>
        >("polygon((0 0,1 1,0 1,0 0))", 4, 4,
          0.00015229324228191774, 6153921163.326375);

    //polygon crossing antimeridian
    test_geometry_sph_geo
        <
            bg::model::polygon<SphericalPoint>,
            bg::model::polygon<GeoPoint>
        >("polygon((359 0,1 1,1 -1,359 0))", 4, 4,
          0.00060931217091786914, 24621363089.253811);

    test_geometry_sph_geo
        <
            bg::model::polygon<SphericalPoint>,
            bg::model::polygon<GeoPoint>
        >("polygon((1 1, 1 4, 3 4, 3 3, 4 3, 4 4, 5 4, 5 1, 1 1))",
          9, 5, 0.003652987070377825, 147615606532.65408);

    // calculate convex hull with a non-default spherical side strategy
    test_geometry
        <
            bg::model::polygon<SphericalPoint>,
            sphrerical_side_by_cross_track<>
        >("polygon((359 0,1 1,1 -1,359 0))", 4, 4,
          0.00060931217091786914);

    test_empty_input<bg::model::linestring<SphericalPoint>>();
    test_empty_input<bg::model::ring<SphericalPoint>>();
    test_empty_input<bg::model::polygon<SphericalPoint>>();

    test_empty_input<bg::model::linestring<GeoPoint>>();
    test_empty_input<bg::model::ring<GeoPoint>>();
    test_empty_input<bg::model::polygon<GeoPoint>>();
}

int test_main(int, char* [])
{
    test_all();

    return 0;
}
