// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit test

// Copyright (c) 2015-2020, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_intersection_linear_linear_areal
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define BOOST_GEOMETRY_DEBUG_TURNS
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#endif

#include <boost/test/included/unit_test.hpp>

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include "test_intersection_linear_linear.hpp"

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::multi_linestring
    <
        bg::model::linestring<point_type>
    >  multi_linestring_type;

typedef bg::model::ring<point_type, true, false> open_ring_type;
typedef bg::model::polygon<point_type, true, false> open_polygon_type;
typedef bg::model::multi_polygon<open_polygon_type> open_multipolygon_type;

typedef bg::model::ring<point_type> closed_ring_type;
typedef bg::model::polygon<point_type> closed_polygon_type;
typedef bg::model::multi_polygon<closed_polygon_type> closed_multipolygon_type;


template
<
    typename OpenAreal1,
    typename OpenAreal2,
    typename ClosedAreal1,
    typename ClosedAreal2,
    typename MultiLinestring
>
struct test_intersection_aal
{
    static inline void apply(std::string const& case_id,
                             OpenAreal1 const& open_areal1,
                             OpenAreal2 const& open_areal2,
                             MultiLinestring const& expected1,
                             MultiLinestring const& expected2)
    {
        typedef test_intersection_of_geometries
            <
                OpenAreal1, OpenAreal2, MultiLinestring
            > tester;

        tester::apply(open_areal1, open_areal2, expected1, expected2, case_id);

        ClosedAreal1 closed_areal1;
        ClosedAreal2 closed_areal2;
        bg::convert(open_areal1, closed_areal1);
        bg::convert(open_areal2, closed_areal2);

        typedef test_intersection_of_geometries
            <
                ClosedAreal1, ClosedAreal2, MultiLinestring
            > tester_of_closed;

        std::string case_id_closed = case_id + "-closed";

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "testing closed areal geometries..." << std::endl;
#endif
        tester_of_closed::apply(closed_areal1, closed_areal2,
                                expected1, expected2, case_id_closed);
    }

    static inline void apply(std::string const& case_id,
                             OpenAreal1 const& open_areal1,
                             OpenAreal2 const& open_areal2,
                             MultiLinestring const& expected)
    {
        apply(case_id, open_areal1, open_areal2, expected, expected);
    }
};


BOOST_AUTO_TEST_CASE( test_intersection_ring_ring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** RING / RING / LINEAR INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif
    typedef open_ring_type OG;
    typedef closed_ring_type CG;
    typedef multi_linestring_type ML;

    typedef test_intersection_aal<OG, OG, CG, CG, ML> tester;

    tester::apply
        ("r-r-01",
         from_wkt<OG>("POLYGON((0 0,0 2,2 2,2 0))"),
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<ML>("MULTILINESTRING((2 1,2 2),(2 0,1 0),(2 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 1),(2 0,1 0),(2 1,2 1))")
         );

    tester::apply
        ("r-r-02",
         from_wkt<OG>("POLYGON(())"),
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("r-r-03",
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<OG>("POLYGON(())"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("r-r-04",
         from_wkt<OG>("POLYGON(())"),
         from_wkt<OG>("POLYGON(())"),
         from_wkt<ML>("MULTILINESTRING()")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_ring_polygon_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** RING / POLYGON / LINEAR INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif
    typedef open_ring_type OG1;
    typedef open_polygon_type OG2;
    typedef closed_ring_type CG1;
    typedef closed_polygon_type CG2;
    typedef multi_linestring_type ML;

    typedef test_intersection_aal<OG1, OG2, CG1, CG2, ML> tester;

    tester::apply
        ("r-pg-01",
         from_wkt<OG1>("POLYGON((0 0,0 2,2 2,2 0))"),
         from_wkt<OG2>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<ML>("MULTILINESTRING((2 1,2 2),(2 0,1 0),(2 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 1),(2 0,1 0),(2 1,2 1))")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_ring_multipolygon_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** RING / MULTIPOLYGON / LINEAR INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif
    typedef open_ring_type OG1;
    typedef open_multipolygon_type OG2;
    typedef closed_ring_type CG1;
    typedef closed_multipolygon_type CG2;
    typedef multi_linestring_type ML;

    typedef test_intersection_aal<OG1, OG2, CG1, CG2, ML> tester;

    tester::apply
        ("r-mpg-01",
         from_wkt<OG1>("POLYGON((0 0,0 2,2 2,2 0))"),
         from_wkt<OG2>("MULTIPOLYGON(((2 1,2 4,4 4,4 0,1 0)))"),
         from_wkt<ML>("MULTILINESTRING((2 1,2 2),(2 0,1 0),(2 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 1),(2 0,1 0),(2 1,2 1))")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_polygon_polygon_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POLYGON / POLYGON / LINEAR INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif
    typedef open_polygon_type OG;
    typedef closed_polygon_type CG;
    typedef multi_linestring_type ML;

    typedef test_intersection_aal<OG, OG, CG, CG, ML> tester;

    tester::apply
        ("pg-pg-01",
         from_wkt<OG>("POLYGON((0 0,0 2,2 2,2 0))"),
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<ML>("MULTILINESTRING((2 1,2 2),(2 0,1 0),(2 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 1),(2 0,1 0),(2 1,2 1))")
         );

    tester::apply
        ("pg-pg-02",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 2,2 7,7 7,7 2))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 2),(2 2,2 7,7 7,7 2,2 2),(2 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 2),(2 2,7 2,7 7,2 7,2 2),(2 2,2 2))")
         );

    tester::apply
        ("pg-pg-03",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 3,2 6,6 6,6 3))"),
         from_wkt<ML>("MULTILINESTRING((2 3,2 6),(2 3,2 3))")
         );

    tester::apply
        ("pg-pg-04",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 3,2 7,6 7,6 3))"),
         from_wkt<ML>("MULTILINESTRING((2 3,2 7,6 7),(2 3,2 3))")
         );

    tester::apply
        ("pg-pg-05",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 3,2 7,7 7,7 3))"),
         from_wkt<ML>("MULTILINESTRING((2 3,2 7,7 7,7 3),(2 3,2 3))")
         );

    tester::apply
        ("pg-pg-06",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 3,2 7,7 7,7 3))"),
         from_wkt<ML>("MULTILINESTRING((2 3,2 7,7 7,7 3),(2 3,2 3))")
         );

    tester::apply
        ("pg-pg-07",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 5,5 7,7 5,5 2))"),
         from_wkt<ML>("MULTILINESTRING((2 5,2 5),(5 7,5 7),(7 5,7 5),(5 2,5 2))")
         );

    tester::apply
        ("pg-pg-08",
         from_wkt<OG>("POLYGON((0 0,0 10,10 10,10 0),(2 2,7 2,7 7,2 7))"),
         from_wkt<OG>("POLYGON((2 5,4 7,6 7,7 5,5 2))"),
         from_wkt<ML>("MULTILINESTRING((2 5,2 5),(4 7,6 7),(7 5,7 5),(5 2,5 2))")
         );

    tester::apply
        ("pg-pg-09",
         from_wkt<OG>("POLYGON(())"),
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("pg-pg-10",
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0))"),
         from_wkt<OG>("POLYGON(())"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("pg-pg-11",
         from_wkt<OG>("POLYGON(())"),
         from_wkt<OG>("POLYGON(())"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("pg-pg-12",
         from_wkt<OG>("POLYGON((),())"),
         from_wkt<OG>("POLYGON((),(),())"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("pg-pg-13",
         from_wkt<OG>("POLYGON((2 1,2 4,4 4,4 0,1 0),())"),
         from_wkt<OG>("POLYGON(())"),
         from_wkt<ML>("MULTILINESTRING()")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_polygon_multipolygon_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POLYGON / MULTIPOLYGON / LINEAR INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif
    typedef open_polygon_type OG1;
    typedef open_multipolygon_type OG2;
    typedef closed_polygon_type CG1;
    typedef closed_multipolygon_type CG2;
    typedef multi_linestring_type ML;

    typedef test_intersection_aal<OG1, OG2, CG1, CG2, ML> tester;

    tester::apply
        ("pg-mpg-01",
         from_wkt<OG1>("POLYGON((0 0,0 2,2 2,2 0))"),
         from_wkt<OG2>("MULTIPOLYGON(((2 1,2 4,4 4,4 0,1 0)))"),
         from_wkt<ML>("MULTILINESTRING((2 1,2 2),(2 0,1 0),(2 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 1),(2 0,1 0),(2 1,2 1))")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_multipolygon_multipolygon_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOLYGON / MULTIPOLYGON / LINEAR INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif
    typedef open_multipolygon_type OG;
    typedef closed_multipolygon_type CG;
    typedef multi_linestring_type ML;

    typedef test_intersection_aal<OG, OG, CG, CG, ML> tester;

    tester::apply
        ("mpg-mpg-01",
         from_wkt<OG>("MULTIPOLYGON(((0 0,0 2,2 2,2 0)))"),
         from_wkt<OG>("MULTIPOLYGON(((2 1,2 4,4 4,4 0,1 0)))"),
         from_wkt<ML>("MULTILINESTRING((2 1,2 2),(2 0,1 0),(2 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 2,2 1),(2 0,1 0),(2 1,2 1))")
         );

    tester::apply
        ("mpg-mpg-02",
         from_wkt<OG>("MULTIPOLYGON(((0 0,0 10,10 10,10 0),(2 2,8 2,8 8,2 8)))"),
         from_wkt<OG>("MULTIPOLYGON(((2 4,2 6,8 6,8 4)))"),
         from_wkt<ML>("MULTILINESTRING((2 4,2 4),(2 4,2 6),(8 6,8 4))")
         );

    tester::apply
        ("mpg-mpg-03",
         from_wkt<OG>("MULTIPOLYGON()"),
         from_wkt<OG>("MULTIPOLYGON(((2 1,2 4,4 4,4 0,1 0)))"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("mpg-mpg-04",
         from_wkt<OG>("MULTIPOLYGON(((2 1,2 4,4 4,4 0,1 0)))"),
         from_wkt<OG>("MULTIPOLYGON()"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("mpg-mpg-05",
         from_wkt<OG>("MULTIPOLYGON()"),
         from_wkt<OG>("MULTIPOLYGON()"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("mpg-mpg-06",
         from_wkt<OG>("MULTIPOLYGON((()),((),()))"),
         from_wkt<OG>("MULTIPOLYGON()"),
         from_wkt<ML>("MULTILINESTRING()")
         );

    tester::apply
        ("mpg-mpg-07",
         from_wkt<OG>("MULTIPOLYGON(((2 1,2 4,4 4,4 0,1 0),(),()))"),
         from_wkt<OG>("MULTIPOLYGON()"),
         from_wkt<ML>("MULTILINESTRING()")
         );
}
