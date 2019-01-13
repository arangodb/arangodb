// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_is_simple_geo
#endif

#include "test_is_simple.hpp"


inline bg::srs::spheroid<double> sph(double a, double rf)
{
    double b = a - a / rf;
    return bg::srs::spheroid<double>(a, b);
}

typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> >  point_type;
typedef bg::model::segment<point_type>                  segment_type;
typedef bg::model::linestring<point_type>               linestring_type;
typedef bg::model::multi_linestring<linestring_type>    multi_linestring_type;
// ccw open and closed polygons
typedef bg::model::polygon<point_type,false,false>      open_ccw_polygon_type;
typedef bg::model::polygon<point_type,false,true>       closed_ccw_polygon_type;
// multi-geometries
typedef bg::model::multi_point<point_type>              multi_point_type;
typedef bg::model::multi_polygon<open_ccw_polygon_type> multi_polygon_type;
// box
typedef bg::model::box<point_type>                      box_type;


BOOST_AUTO_TEST_CASE( test_is_simple_geo_multipoint )
{
    typedef multi_point_type G;

    bg::strategy::intersection::geographic_segments<> s;

    test_simple_s(from_wkt<G>("MULTIPOINT(0 90, 0 90)"), s, false);
    test_simple_s(from_wkt<G>("MULTIPOINT(0 90, 1 90)"), s, false);
    test_simple_s(from_wkt<G>("MULTIPOINT(0 -90, 0 -90)"), s, false);
    test_simple_s(from_wkt<G>("MULTIPOINT(0 -90, 1 -90)"), s, false);
    test_simple_s(from_wkt<G>("MULTIPOINT(0 80, 1 80)"), s, true);
}

BOOST_AUTO_TEST_CASE( test_is_simple_geo_linestring )
{
    typedef linestring_type G;

    bg::srs::spheroid<double> sph_wgs84;
    bg::srs::spheroid<double> sph_4053(6371228, 6371228);
    bg::srs::spheroid<double> sph_near_4053(6371228, 6371227);

    bg::strategy::intersection::geographic_segments<> s(sph_wgs84);
    bg::strategy::intersection::geographic_segments<> s_4053(sph_4053);
    bg::strategy::intersection::geographic_segments<> s_near_4053(sph_near_4053);

    // Two cases which in Cartesian would be a spike, but in Geographic
    // they go over the equator (first segment) and then over the pole
    // (second segment)
    test_simple_s(from_wkt<G>("LINESTRING(0 0, -90 0, 90 0)"), s, true);
    test_simple_s(from_wkt<G>("LINESTRING(0 0, 90 0, -90 0)"), s, true);

    // Two similar cases, but these do not go over the pole back, but
    // over the equator, and therefore make a spike
    test_simple_s(from_wkt<G>("LINESTRING(0 0, -80 0, 80 0)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(0 0, 80 0, -80 0)"), s, false);

    // Going over the equator in a normal way, eastwards and westwards
    test_simple_s(from_wkt<G>("LINESTRING(-90 0, 0 0, 90 0)"), s, true);
    test_simple_s(from_wkt<G>("LINESTRING(90 0, 0 0, -90 0)"), s, true);

    test_simple_s(from_wkt<G>("LINESTRING(0 90, -90 0, 90 0)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(0 90, -90 50, 90 0)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(0 90, -90 -50, 90 0)"), s, true);

    // invalid linestrings
    test_simple_s(from_wkt<G>("LINESTRING(0 90, 0 90)"), s, false, false);
    test_simple_s(from_wkt<G>("LINESTRING(0 -90, 0 -90)"), s, false, false);
    test_simple_s(from_wkt<G>("LINESTRING(0 90, 1 90)"), s, false, false);
    test_simple_s(from_wkt<G>("LINESTRING(0 -90, 1 -90)"), s, false, false);

    // FAILING
    //test_simple_s(from_wkt<G>("LINESTRING(0 90, 0 80, 1 80, 0 90)"), s, false);
    //test_simple_s(from_wkt<G>("LINESTRING(0 -90, 0 -80, 1 -80, 0 -90)"), s, false);
    //test_simple_s(from_wkt<G>("LINESTRING(0 90, 0 80, 1 80, 1 90)"), s, false);
    //test_simple_s(from_wkt<G>("LINESTRING(0 -90, 0 -80, 1 -80, 1 -90)"), s, false);

    test_simple_s(from_wkt<G>("LINESTRING(35 0, 110 36, 159 0, 82 30)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(135 0, -150 36, -101 0, -178 30)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(45 0, 120 36, 169 0, 92 30)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(179 0, -179 1, -179 0, 179 1)"), s, false);

    test_simple_s(from_wkt<G>("LINESTRING(-121 -19,37 8,-19 -15,-104 -58)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(-121 -19,37 8,-19 -15,-104 -58)"), s_4053, false);
    test_simple_s(from_wkt<G>("LINESTRING(-121 -19,37 8,-19 -15,-104 -58)"), s_near_4053, false);
    
    // The segments are very close to each other, in WGS84 they cross,
    // in spherical or nearly spherical they don't cross
    test_simple_s(from_wkt<G>("LINESTRING(106 22,21 39,40 -12,-91 68)"), s, false);
    test_simple_s(from_wkt<G>("LINESTRING(106 22,21 39,40 -12,-91 68)"), s_4053, true);
    test_simple_s(from_wkt<G>("LINESTRING(106 22,21 39,40 -12,-91 68)"), s_near_4053, true);
}

BOOST_AUTO_TEST_CASE( test_is_simple_geo_multilinestring )
{
    typedef multi_linestring_type G;

    bg::strategy::intersection::geographic_segments<> s_wgs84; // EPSG 4326
    bg::strategy::intersection::geographic_segments<> s_bessel((sph(6377397.155,299.1528128))); // EPSG 4804, 4813, 4820
    
    // FAILING
    //test_simple_s(from_wkt<G>("MULTILINESTRING((0 90, 0 80),(1 90, 1 80))"), s_wgs84, false);
    //test_simple_s(from_wkt<G>("MULTILINESTRING((0 -90, 0 -80),(1 -90, 1 -80))"), s_wgs84, false);

    test_simple_s(from_wkt<G>("MULTILINESTRING((35 0, 110 36),(159 0, 82 30))"), s_wgs84, false);
    test_simple_s(from_wkt<G>("MULTILINESTRING((135 0, -150 36),(-101 0, -178 30))"), s_wgs84, false);
    test_simple_s(from_wkt<G>("MULTILINESTRING((45 0, 120 36),(169 0, 92 30))"), s_wgs84, false);
    test_simple_s(from_wkt<G>("MULTILINESTRING((179 0, -179 1),(-179 0, 179 1))"), s_wgs84, false);

    test_simple_s(from_wkt<G>("MULTILINESTRING((35 2,110 36),(72 51,67 28,16 53,159 3,82 30))"), s_wgs84, false);
    test_simple_s(from_wkt<G>("MULTILINESTRING((35 2,110 36),(72 51,67 28,16 53,159 3,82 30))"), s_bessel, false);
}
