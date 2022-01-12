// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/io/wkt/read.hpp>

template <std::size_t D, typename T = double>
struct box_dD
{
    typedef boost::geometry::model::box
        <
            boost::geometry::model::point<T, D, boost::geometry::cs::cartesian>
        > type;
};

template <typename Geometry>
inline void test_num_points(std::string const& wkt,
                            std::size_t expected,
                            std::size_t expected_add_for_open)
{
    namespace bg = boost::geometry;
    Geometry geometry;
    boost::geometry::read_wkt(wkt, geometry);
    std::size_t detected = bg::num_points(geometry);
    BOOST_CHECK_EQUAL(expected, detected);
    detected = bg::num_points(geometry, false);
    BOOST_CHECK_EQUAL(expected, detected);
    detected = bg::num_points(geometry, true);
    BOOST_CHECK_EQUAL(expected_add_for_open, detected);
}

template <typename Geometry>
inline void test_num_points(std::string const& wkt, std::size_t expected)
{
    test_num_points<Geometry>(wkt, expected, expected);
}

int test_main(int, char* [])
{
    typedef bg::model::point<double,2,bg::cs::cartesian> point;
    typedef bg::model::linestring<point> linestring;
    typedef bg::model::segment<point> segment;
    typedef bg::model::box<point> box;
    typedef bg::model::ring<point> ring;
    typedef bg::model::polygon<point> polygon;
    typedef bg::model::multi_point<point> multi_point;
    typedef bg::model::multi_linestring<linestring> multi_linestring;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    // open geometries
    typedef bg::model::ring<point, true, false> open_ring;
    typedef bg::model::polygon<point, true, false> open_polygon;
    typedef bg::model::multi_polygon<open_polygon> open_multi_polygon;

    using variant = boost::variant<linestring, polygon>;
    using open_variant = boost::variant<linestring, open_polygon>;

    using geometry_collection = bg::model::geometry_collection<variant>;
    using open_geometry_collection = bg::model::geometry_collection<open_variant>;

    test_num_points<point>("POINT(0 0)", 1u);
    test_num_points<linestring>("LINESTRING(0 0,1 1)", 2u);
    test_num_points<segment>("LINESTRING(0 0,1 1)", 2u);
    test_num_points<box>("POLYGON((0 0,10 10))", 4u);
    test_num_points<box_dD<3>::type>("BOX(0 0 0,1 1 1)", 8u);
    test_num_points<box_dD<4>::type>("BOX(0 0 0 0,1 1 1 1)", 16u);
    test_num_points<box_dD<5>::type>("BOX(0 0 0 0 0,1 1 1 1 1)", 32u);
    test_num_points<ring>("POLYGON((0 0,1 1,0 1,0 0))", 4u);
    test_num_points<polygon>("POLYGON((0 0,10 10,0 10,0 0))", 4u);
    test_num_points<polygon>("POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4))", 10u);
    test_num_points<multi_point>("MULTIPOINT((0 0),(1 1))", 2u);
    test_num_points<multi_linestring>("MULTILINESTRING((0 0,1 1),(2 2,3 3,4 4))", 5u);
    test_num_points<multi_polygon>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 10,1 10,1 9,0 10)))", 9u);

    // test open geometries
    test_num_points<open_ring>("POLYGON((0 0,1 1,0 1))", 3u, 4u);
    test_num_points<open_ring>("POLYGON((0 0,1 1,0 1,0 0))", 3u, 4u);
    test_num_points<open_polygon>("POLYGON((0 0,10 10,0 10))", 3u, 4u);
    test_num_points<open_polygon>("POLYGON((0 0,10 10,0 10,0 0))", 3u, 4u);
    test_num_points<open_multi_polygon>("MULTIPOLYGON(((0 0,0 10,10 10,10 0)),((0 10,1 10,1 9)))", 7u, 9u);
    test_num_points<open_multi_polygon>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 10,1 10,1 9,0 10)))", 7u, 9u);

    test_num_points<variant>("POLYGON((0 0,1 1,0 1,0 0))", 4u);
    test_num_points<open_variant>("POLYGON((0 0,1 1,0 1))", 3u, 4u);

    test_num_points<geometry_collection>("GEOMETRYCOLLECTION(POLYGON((0 0,1 1,0 1,0 0)),LINESTRING(0 0,1 1))", 6u);
    test_num_points<open_geometry_collection>("GEOMETRYCOLLECTION(POLYGON((0 0,1 1,0 1)),LINESTRING(0 0,1 1))", 5u, 6u);

    return 0;
}

