// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2018, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#ifndef BOOST_GEOMETRY_TEST_EMPTY_GEOMETRY_HPP
#define BOOST_GEOMETRY_TEST_EMPTY_GEOMETRY_HPP

template <typename Point, typename Strategy>
void test_more_empty_input_pointlike_pointlike(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::multi_point<Point> multipoint_empty;

    Point point = from_wkt<Point>("point(0 0)");

    // 1st geometry is empty
    test_empty_input(multipoint_empty, point, strategy);

    // 2nd geometry is empty
    test_empty_input(point, multipoint_empty, strategy);

    // both geometries are empty
    test_empty_input(multipoint_empty, multipoint_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}


template <typename Point, typename Strategy>
void test_more_empty_input_pointlike_linear(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::linestring<Point> line_empty;
    bg::model::multi_point<Point> multipoint_empty;
    bg::model::multi_linestring<bg::model::linestring<Point> > multiline_empty;

    Point point = from_wkt<Point>("POINT(0 0)");
    bg::model::linestring<Point> line =
        from_wkt<bg::model::linestring<Point> >("LINESTRING(0 0,1 1,2 2)");

    // 1st geometry is empty
    test_empty_input(multipoint_empty, line, strategy);

    // 2nd geometry is empty
    test_empty_input(line, multipoint_empty, strategy);
    test_empty_input(point, line_empty, strategy);
    test_empty_input(point, multiline_empty, strategy);

    // both geometries are empty
    test_empty_input(multipoint_empty, line_empty, strategy);
    test_empty_input(multipoint_empty, multiline_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}


template <typename Point, typename Strategy>
void test_more_empty_input_pointlike_areal(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::multi_point<Point> multipoint_empty;

    bg::model::polygon<Point> polygon_empty;
    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon_empty;

    Point point = from_wkt<Point>("POINT(0 0)");
    bg::model::polygon<Point> polygon =
        from_wkt<bg::model::polygon<Point> >("POLYGON((0 0,1 0,1 1,0 1,0 0))");

    // 1st geometry is empty
    test_empty_input(multipoint_empty, polygon, strategy);
    test_empty_input(polygon_empty, point, strategy);
    test_empty_input(multipolygon_empty, point, strategy);

    // 2nd geometry is empty
    test_empty_input(point, polygon_empty, strategy);
    test_empty_input(point, multipolygon_empty, strategy);
    test_empty_input(polygon, multipoint_empty, strategy);

    // both geometries are empty
    test_empty_input(multipoint_empty, polygon_empty, strategy);
    test_empty_input(multipoint_empty, multipolygon_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}


template <typename Point, typename Strategy>
void test_more_empty_input_linear_linear(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::linestring<Point> line_empty;
    bg::model::multi_linestring<bg::model::linestring<Point> > multiline_empty;

    bg::model::linestring<Point> line =
        from_wkt<bg::model::linestring<Point> >("LINESTRING(0 0,1 1)");

    // 1st geometry is empty
    test_empty_input(line_empty, line, strategy);
    test_empty_input(multiline_empty, line, strategy);

    // 2nd geometry is empty
    test_empty_input(line, line_empty, strategy);
    test_empty_input(line, multiline_empty, strategy);

    // both geometries are empty
    test_empty_input(line_empty, line_empty, strategy);
    test_empty_input(multiline_empty, line_empty, strategy);
    test_empty_input(multiline_empty, multiline_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}


template <typename Point, typename Strategy>
void test_more_empty_input_linear_areal(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::linestring<Point> line_empty;
    bg::model::multi_linestring<bg::model::linestring<Point> > multiline_empty;

    bg::model::polygon<Point> polygon_empty;
    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon_empty;

    bg::model::linestring<Point> line =
        from_wkt<bg::model::linestring<Point> >("LINESTRING(0 0,1 1)");
    bg::model::polygon<Point> polygon =
        from_wkt<bg::model::polygon<Point> >("POLYGON((0 0,1 0,1 1,0 1,0 0))");

    // 1st geometry is empty
    test_empty_input(line_empty, polygon, strategy);
    test_empty_input(multiline_empty, polygon, strategy);
    test_empty_input(polygon_empty, line, strategy);
    test_empty_input(multipolygon_empty, line, strategy);

    // 2nd geometry is empty
    test_empty_input(line, polygon_empty, strategy);
    test_empty_input(line, multipolygon_empty, strategy);
    test_empty_input(polygon, line_empty, strategy);
    test_empty_input(polygon, multiline_empty, strategy);

    // both geometries are empty
    test_empty_input(line_empty, polygon_empty, strategy);
    test_empty_input(line_empty, multipolygon_empty, strategy);
    test_empty_input(multiline_empty, polygon_empty, strategy);
    test_empty_input(multiline_empty, multipolygon_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}


template <typename Point, typename Strategy>
void test_more_empty_input_areal_areal(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::polygon<Point> polygon_empty;
    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon_empty;

    bg::model::polygon<Point> polygon =
        from_wkt<bg::model::polygon<Point> >("POLYGON((0 0,1 0,1 1,0 1,0 0))");

    // 1st geometry is empty
    test_empty_input(polygon_empty, polygon, strategy);
    test_empty_input(multipolygon_empty, polygon, strategy);

    // 2nd geometry is empty
    test_empty_input(polygon, polygon_empty, strategy);
    test_empty_input(polygon, multipolygon_empty, strategy);

    // both geometries are empty
    test_empty_input(polygon_empty, multipolygon_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}

#endif // BOOST_GEOMETRY_TEST_EMPTY_GEOMETRY_HPP
