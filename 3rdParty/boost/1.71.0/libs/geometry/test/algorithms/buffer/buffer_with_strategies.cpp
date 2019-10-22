// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015-2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "geometry_test_common.hpp"

#include <boost/geometry/algorithms/buffer.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/geometries/geometries.hpp>

// For test
#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/num_points.hpp>


// This unit test tests boost::geometry::buffer (overload with strategies)
// More detailed tests are, per geometry type, available in buffer_<TYPE>.cpp
// (testing boost::geometry::buffer_inserter)

// SVG's are not created (they are in detailed tests)

static std::string const polygon_simplex = "POLYGON ((0 0,1 5,6 1,0 0))";
static std::string const polygon_empty = "POLYGON()";
static std::string const multi_polygon_empty = "MULTIPOLYGON()";
static std::string const multi_polygon_simplex
    = "MULTIPOLYGON(((0 1,2 5,5 3,0 1)),((1 1,5 2,5 0,1 1)))";


template
<
    typename Geometry,
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy,
    typename SideStrategy,
    typename PointStrategy,
    typename DistanceStrategy
>
void test_with_strategies(std::string const& caseid,
        std::string const& wkt,
        JoinStrategy const& join_strategy,
        EndStrategy const& end_strategy,
        SideStrategy const& side_strategy,
        PointStrategy const& point_strategy,
        DistanceStrategy const& distance_strategy,
        double expected_area,
        std::size_t expected_numpoints,
        double tolerance = 0.01)
{
    namespace bg = boost::geometry;
    Geometry g;
    bg::read_wkt(wkt, g);
    bg::correct(g);

    GeometryOut result;

    bg::buffer(g, result,
                distance_strategy, side_strategy,
                join_strategy, end_strategy, point_strategy);

    BOOST_CHECK_MESSAGE
        (
            bg::num_points(result) == expected_numpoints,
            "Buffer " << caseid
            << " numpoints expected: " << expected_numpoints
            << " detected: " << bg::num_points(result)
        );

    double const area = bg::area(result);
    double const difference = area - expected_area;

    BOOST_CHECK_MESSAGE
        (
            bg::math::abs(difference) < tolerance,
            "Buffer " << caseid
            << std::setprecision(12)
            << " area expected: " << expected_area
            << " detected: " << area
        );
}


template <bool Clockwise, typename Point>
void test_all()
{
    typedef bg::model::polygon<Point, Clockwise> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    bg::strategy::buffer::join_round join(160);
    bg::strategy::buffer::end_round end(160);
    bg::strategy::buffer::point_circle circle(160);
    bg::strategy::buffer::side_straight side;

    typedef bg::strategy::buffer::distance_symmetric
    <
        typename bg::coordinate_type<Point>::type
    > distance;

    test_with_strategies<multi_polygon, multi_polygon>(
        "multi_polygon_empty", multi_polygon_empty,
        join, end, side, circle, distance(1.0),
        0.0, 0);


    // PostGIS: 34.2550669294223 216 (40 / qcircle)
    // SQL Server: 34.2550419903829 220 (default options)
    test_with_strategies<multi_polygon, multi_polygon>(
        "multi_polygon_simplex", multi_polygon_simplex,
        join, end, side, circle, distance(1.0),
        34.2551, 219);

    test_with_strategies<polygon, multi_polygon>(
        "polygon_empty", polygon_empty,
        join, end, side, circle, distance(1.0),
        0.0, 0);

    //
    // PostGIS: 35.2256914798762 164 (40 / qcircle)
    // SQL Server: 35.2252355201605 153 (default options)
    test_with_strategies<polygon, multi_polygon>(
        "polygon_simplex", polygon_simplex,
        join, end, side, circle, distance(1.0),
        35.2257, 166);
}

int test_main(int, char* [])
{
    test_all<true, bg::model::point<double, 2, bg::cs::cartesian> >();
    return 0;
}
