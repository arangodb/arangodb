// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015.
// Modifications copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/numeric/conversion/bounds.hpp>

#include "test_envelope.hpp"

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/geometries/adapted/std_pair_as_segment.hpp>
#include <test_common/test_point.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)


template <typename P>
void test_2d()
{
    test_envelope<P>("POINT(1 1)", 1, 1, 1, 1);
    test_envelope<bg::model::linestring<P> >("LINESTRING(1 1,2 2)", 1, 2, 1, 2);
    test_envelope<bg::model::polygon<P> >("POLYGON((1 1,1 3,3 3,3 1,1 1))", 1, 3, 1, 3);

    test_envelope<bg::model::ring<P> >("POLYGON((1 1,1 3,3 3,3 1,1 1))", 1, 3, 1, 3);
    test_envelope<bg::model::box<P> >("BOX(1 1,3 3)", 1, 3, 1, 3);

    // Triangle, closed and open, and CCW.
    // Note that for the envelope algorithm,
    // these combinations should theoretically not differ
    test_envelope<bg::model::ring<P> >("POLYGON((4 1,0 7,7 9,4 1))", 0, 7, 1, 9);
    test_envelope<bg::model::ring<P, true, false> >("POLYGON((4 1,0 7,7 9))", 0, 7, 1, 9);
    test_envelope<bg::model::ring<P, false> >("POLYGON((4 1,7 9,0 7,4 1))", 0, 7, 1, 9);
    test_envelope<bg::model::ring<P, false, false> >("POLYGON((4 1,7 9,0 7))", 0, 7, 1, 9);

    typedef std::pair<P, P> segment_type;
    test_envelope<segment_type>("SEGMENT(1 1,3 3)", 1, 3, 1, 3);
}

template <typename P>
void test_3d()
{
    test_envelope<P>("POINT(1 2 3)", 1, 1, 2, 2, 3, 3);
    test_envelope<P>("POINT(3 2 1)", 3, 3, 2, 2, 1, 1);
    test_envelope<bg::model::linestring<P> >("LINESTRING(1 1 1,2 2 2)", 1, 2, 1, 2, 1, 2);
    test_envelope<bg::model::box<P> >("BOX(1 1 1,3 3 3)", 1, 3, 1, 3, 1, 3);
}

template <typename Geometry>
void test_empty_geometry(std::string const& wkt)
{
    typedef typename bg::coordinate_type<Geometry>::type ct;
    ct high_val = boost::numeric::bounds<ct>::highest();
    ct low_val = boost::numeric::bounds<ct>::lowest();

    test_envelope<Geometry>(wkt, high_val, low_val, high_val, low_val);
}

template <typename P>
void test_empty()
{
    test_empty_geometry<bg::model::linestring<P> >("LINESTRING()");
    test_empty_geometry<bg::model::ring<P> >("POLYGON(())");

    test_empty_geometry<bg::model::polygon<P> >("POLYGON(())");

    test_empty_geometry<bg::model::multi_point<P> >("MULTIPOINT()");

    test_empty_geometry
        <
            bg::model::multi_linestring<bg::model::linestring<P> >
        >("MULTILINESTRING()");

    test_empty_geometry
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON()");
}

template <typename P>
void test_invalid()
{
    // polygon with empty exterior and interior rings
    test_empty_geometry<bg::model::polygon<P> >("POLYGON((),(),())");

    // polygon with empty interior rings
    test_envelope
        <
            bg::model::polygon<P>
        >("POLYGON((1 2,1 20,22 20,22 2,1 2),(),())",
          1, 22, 2, 20);

    // another polygon with empty interior rings
    test_envelope
        <
            bg::model::polygon<P>
        >("POLYGON((1 2,1 20,22 20,22 2,1 2),(),(3 4,19 4,19 18,3 18,3 4),())",
          1, 22, 2, 20);

    // polygon with empty exterior ring
    test_envelope
        <
            bg::model::polygon<P>
        >("POLYGON((),(),(3 4,19 4,19 18,3 18,3 4),())",
          3, 19, 4, 18);

    // another polygon with empty exterior ring
    test_envelope
        <
            bg::model::polygon<P>
        >("POLYGON((),(),(3 4,19 4,19 18,3 18,3 4),(4 5,18 5,18 17,4 17,4 5))",
          3, 19, 4, 18);

    // yet one more polygon with empty exterior ring
    test_envelope
        <
            bg::model::polygon<P>
        >("POLYGON((),(),(4 5,18 5,18 17,4 17,4 5),(3 4,19 4,19 18,3 18,3 4))",
          3, 19, 4, 18);

    // multilinestring with empty linestrings
    test_empty_geometry
        <
            bg::model::multi_linestring<bg::model::linestring<P> >
        >("MULTILINESTRING((),(),())");

    // multilinestring with empty and non-empty linestrings
    test_envelope
        <
            bg::model::multi_linestring<bg::model::linestring<P> >
        >("MULTILINESTRING((),(10 20),())", 10, 10, 20, 20);

    // multipolygon with empty polygon
    test_empty_geometry
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON((()))");

    // multipolygon with many empty polygons
    test_empty_geometry
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON(((),(),()),(()),((),(),(),(),()))");

    // multipolygon with empty polygons and non-empty (valid) polygon
    test_envelope
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON(((),(),()),((10 30,10 40,20 30,10 30)),\
          ((),(),()),(()))", 10, 20, 30, 40);

    // multipolygon with empty polygons and non-empty (valid) polygon
    // that has an interior ring
    test_envelope
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON(((),(),()),(()),\
          ((1 2,1 20,22 20,22 2,1 2),(3 4,19 4,19 18,3 18,3 4)),(()))",
          1, 22, 2, 20);

    // multipolygon with empty polygons and non-empty (invalid) polygon
    // that has an interior ring but empty exterior ring
    test_envelope
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON(((),(),()),(()),((),(3 4,19 4,19 18,3 18,3 4)),(()))",
          3, 19, 4, 18);

    // multipolygon with empty polygons and non-empty (invalid) polygon
    // that has an interior ring but empty exterior ring
    test_envelope
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON(((),(),()),((),(),(3 4,19 4,19 18,3 18,3 4),()),(()))",
          3, 19, 4, 18);

    // multipolygon with empty polygons and non-empty (invalid) polygon
    // that has two non-empty interior rings but empty exterior ring
    test_envelope
        <
            bg::model::multi_polygon<bg::model::polygon<P> >
        >("MULTIPOLYGON(((),(),()),\
          ((),(),(3 4,19 4,19 18,3 18,3 4),(4 5,18 5,18 17,4 17,4 5),()),\
          (()))",
          3, 19, 4, 18);
}

int test_main(int, char* [])
{
    //test_2d<int[2]>();
    //test_2d<float[2]>();
    //test_2d<double[2]>();
    test_2d<boost::tuple<float, float> >();
    test_2d<bg::model::d2::point_xy<int> >();
    test_2d<bg::model::d2::point_xy<float> >();
    test_2d<bg::model::d2::point_xy<double> >();

    test_3d<test::test_point>();
    test_3d<boost::tuple<int, int, int> >();

    test_empty<boost::tuple<float, float> >();
    test_empty<bg::model::d2::point_xy<int> >();
    test_empty<bg::model::d2::point_xy<float> >();
    test_empty<bg::model::d2::point_xy<double> >();

    test_invalid<boost::tuple<float, float> >();
    test_invalid<bg::model::d2::point_xy<int> >();
    test_invalid<bg::model::d2::point_xy<float> >();
    test_invalid<bg::model::d2::point_xy<double> >();

#ifdef HAVE_TTMATH
    test_2d<bg::model::d2::point_xy<ttmath_big> >();
    test_3d<boost::tuple<ttmath_big, ttmath_big, ttmath_big> >();
#endif

    return 0;
}
