// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015 Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_sym_difference_areal_areal
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define BOOST_GEOMETRY_DEBUG_TURNS
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#endif

#include <boost/test/included/unit_test.hpp>

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/algorithms/sym_difference.hpp>

#include "../difference/test_difference.hpp"
#include <from_wkt.hpp>

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::ring<point_type> ring_type; // ccw, closed
typedef bg::model::polygon<point_type> polygon_type; // ccw, closed
typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

double const default_tolerance = 0.0001;

template
<
    typename Areal1, typename Areal2, typename PolygonOut
>
struct test_sym_difference_of_areal_geometries
{
    static inline void apply(std::string const& case_id,
                             Areal1 const& areal1,
                             Areal2 const& areal2,
                             int expected_polygon_count,
                             int expected_point_count,
                             double expected_area,
                             double tolerance = default_tolerance)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        bg::model::multi_polygon<PolygonOut> sdf;

        bg::sym_difference(areal1, areal2, sdf);

        std::cout << "Case ID: " << case_id << std::endl;
        std::cout << "Geometry #1: " << bg::wkt(areal1) << std::endl;
        std::cout << "Geometry #2: " << bg::wkt(areal2) << std::endl;
        std::cout << "Sym diff: " << bg::wkt(sdf) << std::endl;
        std::cout << "Polygon count: expected: "
                  << expected_polygon_count
                  << "; detected: " << sdf.size() << std::endl;
        std::cout << "Point count: expected: "
                  << expected_point_count
                  << "; detected: " << bg::num_points(sdf) << std::endl;
        std::cout << "Area: expected: "
                  << expected_area
                  << "; detected: " << bg::area(sdf) << std::endl;
#endif
        ut_settings settings;
        settings.percentage = tolerance;

        test_difference
            <
                PolygonOut
            >(case_id, areal1, areal2,
              expected_polygon_count, expected_point_count, expected_area,
              true, settings);
    }
};


//===========================================================================
//===========================================================================
//===========================================================================


BOOST_AUTO_TEST_CASE( test_sym_difference_ring_ring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** RING / RING SYMMETRIC DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef ring_type R;
    typedef polygon_type PG;

    typedef test_sym_difference_of_areal_geometries<R, R, PG> tester;

    tester::apply("r-r-sdf00",
                  from_wkt<R>("POLYGON((0 0,0 10,10 10,10 0,0 0))"),
                  from_wkt<R>("POLYGON((10 0,10 20,20 20,20 0,10 0))"),
                  1,
                  8,
                  300);

    tester::apply("r-r-sdf01",
                  from_wkt<R>("POLYGON((0 0,0 10,10 10,10 0,0 0))"),
                  from_wkt<R>("POLYGON((9 0,9 20,20 20,20 0,9 0))"),
                  2,
                  12,
                  300);
}


BOOST_AUTO_TEST_CASE( test_sym_difference_polygon_multipolygon )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POLYGON / MULTIPOLYGON SYMMETRIC DIFFERENCE ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef polygon_type PG;
    typedef multi_polygon_type MPG;

    typedef test_sym_difference_of_areal_geometries<PG, MPG, PG> tester;

    tester::apply
        ("pg-mpg-sdf00",
         from_wkt<PG>("POLYGON((10 0,10 10,20 10,20 0,10 0))"),
         from_wkt<MPG>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),\
                       ((20 0,20 10,30 10,30 0,20 0)))"),
         1,
         9,
         300);
}

