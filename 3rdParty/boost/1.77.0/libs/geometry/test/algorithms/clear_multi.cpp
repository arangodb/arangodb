// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2011-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/range/empty.hpp>

#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/num_points.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/geometries/adapted/boost_variant.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/geometry_collection.hpp>
#include <boost/geometry/geometries/point_xy.hpp>


template <typename Geometry>
void test_geometry(std::string const& wkt, std::size_t expected_before, std::size_t expected_after)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> variant_geometry(geometry);
    bg::model::geometry_collection<boost::variant<Geometry>> gc{ variant_geometry };

    BOOST_CHECK_EQUAL(bg::num_points(geometry), expected_before);
    bg::clear(geometry);
    BOOST_CHECK_EQUAL(bg::num_points(geometry), expected_after);

    BOOST_CHECK_EQUAL(bg::num_points(variant_geometry), expected_before);
    bg::clear(variant_geometry);
    BOOST_CHECK_EQUAL(bg::num_points(variant_geometry), expected_after);

    BOOST_CHECK(! boost::empty(gc));
    bg::clear(gc);
    BOOST_CHECK(boost::empty(gc));
}


template <typename Point>
void test_all()
{
    typedef bg::model::polygon<Point> poly;
    typedef bg::model::linestring<Point> ls;
    typedef bg::model::multi_point<Point> mpoint;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<Point>("POINT(0 0)", 1, 1);
    test_geometry<ls>("LINESTRING(0 0,0 1)", 2, 0);
    test_geometry<poly>("POLYGON((0 0,0 1,1 0,0 0))", 4, 0);
    test_geometry<mpoint>("MULTIPOINT((0 0),(0 1),(1 0),(0 0))", 4, 0);
    test_geometry<mls>("MULTILINESTRING((0 0,0 1),(1 0,0 0))", 4, 0);
    test_geometry<mpoly>("MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((10 0,10 1,11 0,10 0)))", 8, 0);
}


int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
