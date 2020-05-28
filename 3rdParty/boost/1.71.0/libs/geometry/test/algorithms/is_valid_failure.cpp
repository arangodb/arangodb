// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_is_valid_failure
#endif

#include <iostream>
#include <string>

#include <boost/variant/variant.hpp>

#include <boost/test/included/unit_test.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/reverse.hpp>
#include <boost/geometry/algorithms/validity_failure_type.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <from_wkt.hpp>


namespace bg = ::boost::geometry;

typedef bg::validity_failure_type failure_type;

typedef bg::model::point<double, 2, bg::cs::cartesian> point_type;
typedef bg::model::segment<point_type>                 segment_type;
typedef bg::model::box<point_type>                     box_type;
typedef bg::model::linestring<point_type>              linestring_type;
typedef bg::model::multi_linestring<linestring_type>   multi_linestring_type;
typedef bg::model::multi_point<point_type>             multi_point_type;


char const* to_string(failure_type failure)
{
    switch (failure)
    {
    case bg::no_failure:
        return "no_failure";
    case bg::failure_few_points:
        return "failure_few_points";
    case bg::failure_wrong_topological_dimension:
        return "failure_wrong_topological_dimension";
    case bg::failure_spikes:
        return "failure_spikes";
    case bg::failure_duplicate_points:
        return "failure_duplicate_points";
    case bg::failure_not_closed:
        return "failure_not_closed";
    case bg::failure_self_intersections:
        return "failure_self_intersections";
    case bg::failure_wrong_orientation:
        return "failure_wrong_orientation";
    case bg::failure_interior_rings_outside:
        return "failure_interior_rings_outside";
    case bg::failure_nested_interior_rings:
        return "failure_nested_interior_rings";
    case bg::failure_disconnected_interior:
        return "failure_disconnected_interior";
    case bg::failure_intersecting_interiors:
        return "failure_intersecting_interiors";
    case bg::failure_wrong_corner_order:
        return "failure_wrong_corner_order";
    default:
        return ""; // to avoid warnings (-Wreturn-type)
    }    
}


template <typename Geometry>
struct test_failure
{
    static inline void apply(std::string const& case_id,
                             Geometry const& geometry,
                             failure_type expected)
    {
        failure_type detected;
        bg::is_valid(geometry, detected);
        std::string expected_msg = bg::validity_failure_type_message(expected);
        std::string detected_msg;
        bg::is_valid(geometry, detected_msg);
        std::string detected_msg_short
            = detected_msg.substr(0, expected_msg.length());

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "----------------------" << std::endl;
        std::cout << "case id: " << case_id << std::endl;
        std::cout << "Geometry: " << bg::wkt(geometry) << std::endl;
        std::cout << "Expected reason: " << expected_msg << std::endl;
        std::cout << "Detected reason: " << detected_msg << std::endl;
        std::cout << "Expected: " << to_string(expected) << std::endl;
        std::cout << "Detected: " << to_string(detected) << std::endl;
        std::cout << std::endl;
#endif

        BOOST_CHECK_MESSAGE(expected == detected,
            "case id: " << case_id
            << ", Geometry: " << bg::wkt(geometry)
            << ", expected: " << to_string(expected)
            << ", detected: " << to_string(detected));

        BOOST_CHECK(detected_msg_short == expected_msg);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "----------------------" << std::endl;
        std::cout << std::endl << std::endl;
#endif
    }

    static inline void apply(std::string const& case_id,
                             std::string const& wkt,
                             failure_type expected)
    {
        Geometry geometry = from_wkt<Geometry>(wkt);
        apply(case_id, geometry, expected);
    }
};


BOOST_AUTO_TEST_CASE( test_failure_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: POINT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef point_type G;
    typedef test_failure<G> test;

    test::apply("p01", "POINT(0 0)", bg::no_failure);
}

BOOST_AUTO_TEST_CASE( test_failure_multipoint )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: MULTIPOINT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef multi_point_type G;
    typedef test_failure<G> test;

    test::apply("mp01", "MULTIPOINT()", bg::no_failure);
    test::apply("mp02", "MULTIPOINT(0 0,0 0)", bg::no_failure);
    test::apply("mp03", "MULTIPOINT(0 0,1 0,1 1,0 1)", bg::no_failure);
    test::apply("mp04", "MULTIPOINT(0 0,1 0,1 1,1 0,0 1)", bg::no_failure);
}

BOOST_AUTO_TEST_CASE( test_failure_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: SEGMENT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef segment_type G;
    typedef test_failure<G> test;

    test::apply("s01",
                "SEGMENT(0 0,0 0)",
                bg::failure_wrong_topological_dimension);
    test::apply("s02", "SEGMENT(0 0,1 0)", bg::no_failure);
}

BOOST_AUTO_TEST_CASE( test_failure_box )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: BOX " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef box_type G;
    typedef test_failure<G> test;

    // boxes where the max corner and below and/or to the left of min corner
    test::apply("b01",
                "BOX(0 0,-1 0)",
                bg::failure_wrong_topological_dimension);
    test::apply("b02", "BOX(0 0,0 -1)", bg::failure_wrong_corner_order);
    test::apply("b03", "BOX(0 0,-1 -1)", bg::failure_wrong_corner_order);

    // boxes of zero area; they are not 2-dimensional, so invalid
    test::apply("b04", "BOX(0 0,0 0)", bg::failure_wrong_topological_dimension);
    test::apply("b05", "BOX(0 0,1 0)", bg::failure_wrong_topological_dimension);
    test::apply("b06", "BOX(0 0,0 1)", bg::failure_wrong_topological_dimension);

    test::apply("b07", "BOX(0 0,1 1)", bg::no_failure);
}

BOOST_AUTO_TEST_CASE( test_failure_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: LINESTRING " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef linestring_type G;
    typedef test_failure<G> test;

    // empty linestring
    test::apply("l01", "LINESTRING()", bg::failure_few_points);

    // 1-point linestrings
    test::apply("l02", "LINESTRING(0 0)", bg::failure_few_points);
    test::apply("l03",
                "LINESTRING(0 0,0 0)",
                bg::failure_wrong_topological_dimension);
    test::apply("l04",
                "LINESTRING(0 0,0 0,0 0)",
                bg::failure_wrong_topological_dimension);

    // 2-point linestrings
    test::apply("l05", "LINESTRING(0 0,1 2)", bg::no_failure);
    test::apply("l06", "LINESTRING(0 0,1 2,1 2)", bg::no_failure);
    test::apply("l07", "LINESTRING(0 0,0 0,1 2,1 2)", bg::no_failure);
    test::apply("l08", "LINESTRING(0 0,0 0,0 0,1 2,1 2)", bg::no_failure);

    // 3-point linestring
    test::apply("l09", "LINESTRING(0 0,1 0,2 10)", bg::no_failure);

    // linestrings with spikes
    test::apply("l10", "LINESTRING(0 0,1 2,0 0)", bg::no_failure);
}

BOOST_AUTO_TEST_CASE( test_failure_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: MULTILINESTRING " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef multi_linestring_type G;
    typedef test_failure<G> test;

    // empty multilinestring
    test::apply("mls01", "MULTILINESTRING()", bg::no_failure);

    // multilinestring with empty linestring(s)
    test::apply("mls02", "MULTILINESTRING(())", bg::failure_few_points);
    test::apply("mls03", "MULTILINESTRING((),(),())", bg::failure_few_points);
    test::apply("mls04",
                "MULTILINESTRING((),(0 1,1 0))",
                bg::failure_few_points);

    // multilinestring with invalid linestrings
    test::apply("mls05",
                "MULTILINESTRING((0 0),(0 1,1 0))",
                bg::failure_few_points);
    test::apply("mls06",
                "MULTILINESTRING((0 0,0 0),(0 1,1 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("mls07", "MULTILINESTRING((0 0),(1 0))",
                bg::failure_few_points);
    test::apply("mls08",
                "MULTILINESTRING((0 0,0 0),(1 0,1 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("mls09",
                "MULTILINESTRING((0 0),(0 0))",
                bg::failure_few_points);
    test::apply("mls10",
                "MULTILINESTRING((0 0,1 0,0 0),(5 0))",
                bg::failure_few_points);

    // multilinstring that has linestrings with spikes
    test::apply("mls11",
                "MULTILINESTRING((0 0,1 0,0 0),(5 0,1 0,4 1))",
                bg::no_failure);
    test::apply("mls12",
                "MULTILINESTRING((0 0,1 0,0 0),(1 0,2 0))",
                bg::no_failure);

    // valid multilinestrings
    test::apply("mls13",
                "MULTILINESTRING((0 0,1 0,2 0),(5 0,1 0,4 1))",
                bg::no_failure);
    test::apply("mls14",
                "MULTILINESTRING((0 0,1 0,2 0),(1 0,2 0))",
                bg::no_failure);
    test::apply("mls15",
                "MULTILINESTRING((0 0,1 1),(0 1,1 0))",
                bg::no_failure);
    test::apply("mls16",
                "MULTILINESTRING((0 0,1 1,2 2),(0 1,1 0,2 2))",
                bg::no_failure);
}

template <typename Point>
inline void test_open_rings()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: RING (open) " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::ring<Point, false, false> G; // ccw, open ring
    typedef test_failure<G> test;

    // not enough points
    test::apply("r01", "POLYGON(())", bg::failure_few_points);
    test::apply("r02", "POLYGON((0 0))", bg::failure_few_points);
    test::apply("r03", "POLYGON((0 0,1 0))", bg::failure_few_points);

    // duplicate points
    test::apply("r04",
                "POLYGON((0 0,0 0,0 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("r05",
                "POLYGON((0 0,1 0,1 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("r06",
                "POLYGON((0 0,1 0,0 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("r07", "POLYGON((0 0,1 0,1 1,0 0,0 0))", bg::no_failure);
    test::apply("r08", "POLYGON((0 0,1 0,1 0,1 1))", bg::no_failure);
    test::apply("r09", "POLYGON((0 0,1 0,1 0,1 1,0 0))", bg::no_failure);

    // with spikes
    test::apply("r10", "POLYGON((0 0,2 0,2 2,0 2,1 2))", bg::failure_spikes);
    test::apply("r11", "POLYGON((0 0,2 0,1 0,2 2))", bg::failure_spikes);
    test::apply("r12",
                "POLYGON((0 0,1 0,2 0,1 0,4 0,4 4))",
                bg::failure_spikes);
    test::apply("r13", "POLYGON((0 0,2 0,2 2,1 0))", bg::failure_spikes);
    test::apply("r14", "POLYGON((0 0,2 0,1 0))", bg::failure_spikes);
    test::apply("r15",
                "POLYGON((0 0,5 0,5 5,4 4,5 5,0 5))",
                bg::failure_spikes);
    test::apply("r16",
                "POLYGON((0 0,5 0,5 5,4 4,3 3,5 5,0 5))",
                bg::failure_spikes);

    // with spikes and duplicate points
    test::apply("r17",
                "POLYGON((0 0,0 0,2 0,2 0,1 0,1 0))",
                bg::failure_spikes);

    // with self-crossings
    test::apply("r18",
                "POLYGON((0 0,5 0,5 5,3 -1,0 5))",
                bg::failure_self_intersections);

    // with self-crossings and duplicate points
    test::apply("r19",
                "POLYGON((0 0,5 0,5 5,5 5,3 -1,0 5,0 5))",
                bg::failure_self_intersections);

    // with self-intersections
    test::apply("r20",
                "POLYGON((0 0,5 0,5 5,3 5,3 0,2 0,2 5,0 5))",
                bg::failure_self_intersections);
    test::apply("r21",
                "POLYGON((0 0,5 0,5 5,3 5,3 0,2 5,0 5))",
                bg::failure_self_intersections);
    test::apply("r22",
                "POLYGON((0 0,5 0,5 1,1 1,1 2,2 2,3 1,4 2,5 2,5 5,0 5))",
                bg::failure_self_intersections);

    // with self-intersections and duplicate points
    test::apply("r23",
                "POLYGON((0 0,5 0,5 5,3 5,3 5,3 0,3 0,2 0,2 0,2 5,2 5,0 5))",
                bg::failure_self_intersections);

    // next two suggested by Adam Wulkiewicz
    test::apply("r24",
                "POLYGON((0 0,5 0,5 5,0 5,4 4,2 2,0 5))",
                bg::failure_self_intersections);
    test::apply("r25",
                "POLYGON((0 0,5 0,5 5,1 4,4 4,4 1,0 5))",
                bg::failure_self_intersections);

    // and a few more
    test::apply("r26",
                "POLYGON((0 0,5 0,5 5,4 4,1 4,1 1,4 1,4 4,0 5))",
                bg::failure_self_intersections);
    test::apply("r27",
                "POLYGON((0 0,5 0,5 5,4 4,4 1,1 1,1 4,4 4,0 5))",
                bg::failure_self_intersections);

    // valid rings
    test::apply("r28", "POLYGON((0 0,1 0,1 1))", bg::no_failure);
    test::apply("r29", "POLYGON((1 0,1 1,0 0))", bg::no_failure);
    test::apply("r30", "POLYGON((0 0,1 0,1 1,0 1))", bg::no_failure);
    test::apply("r31", "POLYGON((1 0,1 1,0 1,0 0))", bg::no_failure);

    // test cases coming from buffer
    test::apply("r32",
                "POLYGON((1.1713032141645456 -0.9370425713316364,\
                          5.1713032141645456 4.0629574286683638,\
                          4.7808688094430307 4.3753049524455756,\
                          4.7808688094430307 4.3753049524455756,\
                          0.7808688094430304 -0.6246950475544243,\
                          0.7808688094430304 -0.6246950475544243))",
                bg::no_failure);

    // wrong orientation
    test::apply("r33",
                "POLYGON((0 0,0 1,1 1,0 0))",
                bg::failure_wrong_orientation);
}

template <typename Point>
void test_closed_rings()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: RING (closed) " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::ring<Point, false, true> G; // ccw, closed ring
    typedef test_failure<G> test;

    // not enough points
    test::apply("r01c", "POLYGON(())", bg::failure_few_points);
    test::apply("r02c", "POLYGON((0 0))", bg::failure_few_points);
    test::apply("r03c", "POLYGON((0 0,0 0))", bg::failure_few_points);
    test::apply("r04c", "POLYGON((0 0,1 0))", bg::failure_few_points);
    test::apply("r05c", "POLYGON((0 0,1 0,1 0))", bg::failure_few_points);
    test::apply("r06c", "POLYGON((0 0,1 0,2 0))", bg::failure_few_points);
    test::apply("r07c",
                "POLYGON((0 0,1 0,1 0,2 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("r08c",
                "POLYGON((0 0,1 0,2 0,2 0))",
                bg::failure_wrong_topological_dimension);

    // boundary not closed
    test::apply("r09c", "POLYGON((0 0,1 0,1 1,1 2))", bg::failure_not_closed);
    test::apply("r10c",
                "POLYGON((0 0,1 0,1 0,1 1,1 1,1 2))",
                bg::failure_not_closed);

    // with spikes
    test::apply("r11c", "POLYGON((0 0,1 0,1 0,2 0,0 0))", bg::failure_spikes);
    test::apply("r12c",
                "POLYGON((0 0,1 0,1 1,2 2,0.5 0.5,0 1,0 0))",
                bg::failure_spikes);

    // wrong orientation
    test::apply("r13c",
                "POLYGON((0 0,0 1,1 1,2 0,0 0))",
                bg::failure_wrong_orientation);

}

BOOST_AUTO_TEST_CASE( test_failure_ring )
{
    test_open_rings<point_type>();
    test_closed_rings<point_type>();
}


template <typename Point>
void test_open_polygons()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: POLYGON (open) " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<Point, false, false> G; // ccw, open
    typedef test_failure<G> test;

    // not enough points in exterior ring
    test::apply("pg001", "POLYGON(())", bg::failure_few_points);
    test::apply("pg002", "POLYGON((0 0))", bg::failure_few_points);
    test::apply("pg003", "POLYGON((0 0,1 0))", bg::failure_few_points);

    // not enough points in interior ring
    test::apply("pg004",
                "POLYGON((0 0,10 0,10 10,0 10),())",
                bg::failure_few_points);
    test::apply("pg005",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1))",
                bg::failure_few_points);
    test::apply("pg006",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2))",
                bg::failure_few_points);

    // duplicate points in exterior ring
    test::apply("pg007",
                "POLYGON((0 0,0 0,0 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("pg008",
                "POLYGON((0 0,1 0,1 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("pg009",
                "POLYGON((0 0,1 0,0 0))",
                bg::failure_wrong_topological_dimension);
    test::apply("pg010", "POLYGON((0 0,1 0,1 1,0 0,0 0))", bg::no_failure);
    test::apply("pg011", "POLYGON((0 0,1 0,1 0,1 1))", bg::no_failure);
    test::apply("pg012", "POLYGON((0 0,1 0,1 0,1 1,0 0))",  bg::no_failure);

    test::apply("pg013",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 1,1 1))",
                bg::failure_wrong_topological_dimension);
    test::apply("pg014",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,2 1))",
                bg::failure_wrong_topological_dimension);
    test::apply("pg015",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,1 1))",
                bg::failure_wrong_topological_dimension);
    test::apply("pg016",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2,2 1,1 1,1 1))",
                bg::no_failure);
    test::apply("pg017",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2,2 2,2 1))",
                bg::no_failure);
    test::apply("pg018",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2,2 1,2 1,1 1))",
                bg::no_failure);

    // with spikes in exterior ring
    test::apply("pg019", "POLYGON((0 0,2 0,2 2,0 2,1 2))", bg::failure_spikes);
    test::apply("pg020", "POLYGON((0 0,2 0,1 0,2 2))", bg::failure_spikes);
    test::apply("pg021",
                "POLYGON((0 0,1 0,2 0,1 0,4 0,4 4))",
                bg::failure_spikes);
    test::apply("pg022", "POLYGON((0 0,2 0,2 2,1 0))", bg::failure_spikes);
    test::apply("pg023", "POLYGON((0 0,2 0,1 0))", bg::failure_spikes);
    test::apply("pg024",
                "POLYGON((0 0,5 0,5 5,4 4,5 5,0 5))",
                bg::failure_spikes);
    test::apply("pg025",
                "POLYGON((0 0,5 0,5 5,4 4,3 3,5 5,0 5))",
                bg::failure_spikes);

    // with spikes in interior ring
    test::apply("pg026",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,3 3,1 3,2 3))",
                bg::failure_spikes);
    test::apply("pg027",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,2 1,3 3))",
                bg::failure_spikes);
    test::apply("pg028",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,3 1,2 1,4 1,4 4))",
                bg::failure_spikes);
    test::apply("pg029",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,3 3,2 1))",
                bg::failure_spikes);
    test::apply("pg030",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,2 1))",
                bg::failure_spikes);

    // with self-crossings in exterior ring
    test::apply("pg031",
                "POLYGON((0 0,5 0,5 5,3 -1,0 5))",
                bg::failure_self_intersections);

    // example from Norvald Ryeng
    test::apply("pg032",
                "POLYGON((100 1300,140 1300,140 170,100 1700))",
                bg::failure_wrong_orientation);
    // and with point order reversed
    test::apply("pg033",
                "POLYGON((100 1300,100 1700,140 170,140 1300))",
                bg::failure_self_intersections);

    // with self-crossings in interior ring
    // the self-crossing causes the area of the interior ring to have
    // the wrong sign, hence the "wrong orientation" failure
    test::apply("pg034",
                "POLYGON((0 0,10 0,10 10,0 10),(3 3,3 7,4 6,2 6))",
                bg::failure_wrong_orientation);

    // with self-crossings between rings
    test::apply("pg035",
                "POLYGON((0 0,5 0,5 5,0 5),(1 1,2 1,1 -1))",
                bg::failure_self_intersections);

    // with self-intersections in exterior ring
    test::apply("pg036",
                "POLYGON((0 0,5 0,5 5,3 5,3 0,2 0,2 5,0 5))",
                bg::failure_self_intersections);
    test::apply("pg037",
                "POLYGON((0 0,5 0,5 5,3 5,3 0,2 5,0 5))",
                bg::failure_self_intersections);
    test::apply("pg038",
                "POLYGON((0 0,5 0,5 1,1 1,1 2,2 2,3 1,4 2,5 2,5 5,0 5))",
                bg::failure_self_intersections);

    // next two suggested by Adam Wulkiewicz
    test::apply("pg039",
                "POLYGON((0 0,5 0,5 5,0 5,4 4,2 2,0 5))",
                bg::failure_self_intersections);
    test::apply("pg040",
                "POLYGON((0 0,5 0,5 5,1 4,4 4,4 1,0 5))",
                bg::failure_self_intersections);
    test::apply("pg041",
                "POLYGON((0 0,5 0,5 5,4 4,1 4,1 1,4 1,4 4,0 5))",
                bg::failure_self_intersections);
    test::apply("pg042",
                "POLYGON((0 0,5 0,5 5,4 4,4 1,1 1,1 4,4 4,0 5))",
                bg::failure_self_intersections);

    // with self-intersections in interior ring

    // same comment as for pg034
    test::apply("pg043",
                "POLYGON((-10 -10,10 -10,10 10,-10 10),(0 0,5 0,5 5,3 5,3 0,2 0,2 5,0 5))",
                bg::failure_wrong_orientation);

    // same comment as for pg034
    test::apply("pg044",
                "POLYGON((-10 -10,10 -10,10 10,-10 10),(0 0,5 0,5 5,3 5,3 0,2 5,0 5))",
                bg::failure_wrong_orientation);

    // same comment as for pg034
    test::apply("pg045",
                "POLYGON((-10 -10,10 -10,10 10,-10 10),(0 0,5 0,5 1,1 1,1 2,2 2,3 1,4 2,5 2,5 5,0 5))",
                bg::failure_wrong_orientation);

    // with self-intersections between rings
    // hole has common segment with exterior ring
    test::apply("pg046",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 10,2 10,2 1))",
                bg::failure_self_intersections);
    test::apply("pg047",
                "POLYGON((0 0,0 0,10 0,10 10,0 10,0 10),(1 1,1 10,1 10,2 10,2 10,2 1))",
                bg::failure_self_intersections);
    // hole touches exterior ring at one point
    test::apply("pg048",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 10,2 1))",
                bg::no_failure);

    // "hole" is outside the exterior ring, but touches it
    // TODO: return the failure value failure_interior_rings_outside
    test::apply("pg049",
                "POLYGON((0 0,10 0,10 10,0 10),(5 10,4 11,6 11))",
                bg::failure_self_intersections);

    // hole touches exterior ring at vertex
    test::apply("pg050",
                "POLYGON((0 0,10 0,10 10,0 10),(0 0,1 4,4 1))",
                bg::no_failure);

    // "hole" is completely outside the exterior ring
    test::apply("pg051",
                "POLYGON((0 0,10 0,10 10,0 10),(20 20,20 21,21 21,21 20))",
                bg::failure_interior_rings_outside);

    // two "holes" completely outside the exterior ring, that touch
    // each other
    test::apply("pg052",
                "POLYGON((0 0,10 0,10 10,0 10),(20 0,25 10,21 0),(30 0,25 10,31 0))",
                bg::failure_interior_rings_outside);

    // example from Norvald Ryeng
    test::apply("pg053",
                "POLYGON((58 31,56.57 30,62 33),(35 9,28 14,31 16),(23 11,29 5,26 4))",
                bg::failure_interior_rings_outside);
    // and with points reversed
    test::apply("pg054",
                "POLYGON((58 31,62 33,56.57 30),(35 9,31 16,28 14),(23 11,26 4,29 5))",
                bg::failure_wrong_orientation);

    // "hole" is completely inside another "hole"
    test::apply("pg055",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,8 2))",
                bg::failure_nested_interior_rings);
    test::apply("pg056",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,8 2,8 8,2 8))",
                bg::failure_wrong_orientation);

    // "hole" is inside another "hole" (touching)
    // TODO: return the failure value failure_nested_interior_rings
    test::apply("pg057",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                bg::failure_self_intersections);
    // TODO: return the failure value failure_nested_interior_rings
    test::apply("pg058",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                bg::failure_self_intersections);
    test::apply("pg058a",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,8 2,9 6,8 8,2 8))",
                bg::failure_wrong_orientation);
    // TODO: return the failure value failure_nested_interior_rings
    test::apply("pg059",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                bg::failure_self_intersections);
    test::apply("pg059a",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,9 1,9 9,1 9),(2 2,2 8,8 8,9 6,8 2))",
                bg::failure_wrong_orientation);
    // TODO: return the failure value failure_nested_interior_rings
    test::apply("pg060",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                bg::failure_self_intersections);
    test::apply("pg060a",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,9 1,9 9,1 9),(2 2,8 2,9 6,8 8,2 8))",
                bg::failure_wrong_orientation);
    // hole touches exterior ring at two points
    test::apply("pg061",
                "POLYGON((0 0,10 0,10 10,0 10),(5 0,0 5,5 5))",
                bg::failure_disconnected_interior);

    // cases with more holes
    // two holes, touching the exterior at the same point
    test::apply("pg062",
                "POLYGON((0 0,10 0,10 10,0 10),(0 0,1 9,2 9),(0 0,9 2,9 1))",
                bg::no_failure);
    test::apply("pg063",
                "POLYGON((0 0,0 0,10 0,10 10,0 10,0 0),(0 0,0 0,1 9,2 9),(0 0,0 0,9 2,9 1))",
                bg::no_failure);
    test::apply("pg063",
                "POLYGON((0 10,0 0,0 0,0 0,10 0,10 10),(2 9,0 0,0 0,1 9),(9 1,0 0,0 0,9 2))",
                bg::no_failure);
    // two holes, one inside the other
    // TODO: return the failure value failure_nested_interior_rings
    test::apply("pg064",
                "POLYGON((0 0,10 0,10 10,0 10),(0 0,1 9,9 1),(0 0,4 5,5 4))",
                bg::failure_self_intersections);
    // 1st hole touches has common segment with 2nd hole
    test::apply("pg066",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 5,5 5,5 1),(5 4,5 8,8 8,8 4))",
                bg::failure_self_intersections);
    // 1st hole touches 2nd hole at two points
    test::apply("pg067",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 8,2 8,2 1),(2 5,5 8,5 5))",
                bg::failure_disconnected_interior);
    // polygon with many holes, where the last two touch at two points
    test::apply("pg068",
                "POLYGON((0 0,20 0,20 20,0 20),(1 18,1 19,2 19,2 18),(3 18,3 19,4 19,4 18),(5 18,5 19,6 19,6 18),(7 18,7 19,8 19,8 18),(9 18,9 19,10 19,10 18),(11 18,11 19,12 19,12 18),(13 18,13 19,14 19,14 18),(15 18,15 19,16 19,16 18),(17 18,17 19,18 19,18 18),(1 1,1 9,9 9,9 8,2 8,2 1),(2 5,5 8,5 5))",
                bg::failure_disconnected_interior);
    // two holes completely inside exterior ring but touching each
    // other at a point
    test::apply("pg069",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,2 9),(1 1,9 2,9 1))",
                bg::no_failure);
    // four holes, each two touching at different points
    test::apply("pg070",
                "POLYGON((0 0,10 0,10 10,0 10),(0 10,2 1,1 1),(0 10,4 1,3 1),(10 10,9 1,8 1),(10 10,7 1,6 1))",
                bg::no_failure);
    // five holes, with two pairs touching each at some point, and
    // fifth hole creating a disconnected component for the interior
    test::apply("pg071",
                "POLYGON((0 0,10 0,10 10,0 10),(0 10,2 1,1 1),(0 10,4 1,3 1),(10 10,9 1,8 1),(10 10,7 1,6 1),(4 1,4 4,6 4,6 1))",
                bg::failure_disconnected_interior);
    // five holes, with two pairs touching each at some point, and
    // fifth hole creating three disconnected components for the interior
    test::apply("pg072",
                "POLYGON((0 0,10 0,10 10,0 10),(0 10,2 1,1 1),(0 10,4 1,3 1),(10 10,9 1,8 1),(10 10,7 1,6 1),(4 1,4 4,6 4,6 1,5 0))",
                bg::failure_disconnected_interior);

    // both examples: a polygon with one hole, where the hole contains
    // the exterior ring
    test::apply("pg073",
                "POLYGON((0 0,1 0,1 1,0 1),(-10 -10,-10 10,10 10,10 -10))",
                bg::failure_interior_rings_outside);
    // TODO: return the failure value failure_interior_rings_outside
    test::apply("pg074",
                "POLYGON((-10 -10,1 0,1 1,0 1),(-10 -10,-10 10,10 10,10 -10))",
                bg::failure_self_intersections);
}

template <typename Point>
inline void test_doc_example_polygon()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: doc example polygon " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<Point> CCW_CG;
    typedef test_failure<CCW_CG> test;

    test::apply("pg-doc",
                "POLYGON((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 2,0 0),(0 0,2 9,1 9,0 0),(2 9,9 2,9 9,2 9))",
                bg::failure_disconnected_interior);
}

BOOST_AUTO_TEST_CASE( test_failure_polygon )
{
    test_open_polygons<point_type>();
    test_doc_example_polygon<point_type>();
}


template <typename Point>
void test_open_multipolygons()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: MULTIPOLYGON (open) " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<point_type,false,false> ccw_open_polygon_type;
    typedef bg::model::multi_polygon<ccw_open_polygon_type> G;

    typedef test_failure<G> test;

    // not enough points
    test::apply("mpg01", "MULTIPOLYGON()", bg::no_failure);
    test::apply("mpg02", "MULTIPOLYGON((()))", bg::failure_few_points);
    test::apply("mpg03", "MULTIPOLYGON(((0 0)),(()))", bg::failure_few_points);
    test::apply("mpg04", "MULTIPOLYGON(((0 0,1 0)))", bg::failure_few_points);

    // two disjoint polygons
    test::apply("mpg05",
                "MULTIPOLYGON(((0 0,1 0,1 1,0 1)),((2 2,3 2,3 3,2 3)))",
                bg::no_failure);

    // two disjoint polygons with multiple points
    test::apply("mpg06",
                "MULTIPOLYGON(((0 0,1 0,1 0,1 1,0 1)),((2 2,3 2,3 3,3 3,2 3)))",
                bg::no_failure);

    // two polygons touch at a point
    test::apply("mpg07",
                "MULTIPOLYGON(((0 0,1 0,1 1,0 1)),((1 1,2 1,2 2,1 2)))",
                bg::no_failure);

    // two polygons share a segment at a point
    test::apply("mpg08",
                "MULTIPOLYGON(((0 0,1.5 0,1.5 1,0 1)),((1 1,2 1,2 2,1 2)))",
                bg::failure_self_intersections);

    // one polygon inside another and boundaries touching
    test::apply("mpg09",
                "MULTIPOLYGON(((0 0,10 0,10 10,0 10)),((0 0,9 1,9 2)))",
                bg::failure_intersecting_interiors);
    test::apply("mpg09_2",
                "MULTIPOLYGON(((0 0,5 1,10 0,10 10,0 10)),((1 1,9 1,9 2)))",
                bg::failure_intersecting_interiors);

    // one polygon inside another and boundaries not touching
    test::apply("mpg10",
                "MULTIPOLYGON(((0 0,10 0,10 10,0 10)),((1 1,9 1,9 2)))",
                bg::failure_intersecting_interiors);

    // free space is disconnected
    test::apply("mpg11",
                "MULTIPOLYGON(((0 0,1 0,1 1,0 1)),((1 1,2 1,2 2,1 2)),((0 1,0 2,-1 2,-1 -1)),((1 2,1 3,0 3,0 2)))",
                bg::no_failure);

    // multi-polygon with a polygon inside the hole of another polygon
    test::apply("mpg12",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(1 1,1 99,99 99,99 1)),((2 2,98 2,98 98,2 98)))",
                bg::no_failure);
    test::apply("mpg13",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(1 1,1 99,99 99,99 1)),((1 1,98 2,98 98,2 98)))",
                bg::no_failure);

    // test case suggested by Barend Gehrels: take two valid polygons P1 and
    // P2 with holes H1 and H2, respectively, and consider P2 to be
    // fully inside H1; now invalidate the multi-polygon by
    // considering H2 as a hole of P1 and H1 as a hole of P2; this
    // should be invalid
    //
    // first the valid case:
    test::apply("mpg14",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(1 1,1 99,99 99,99 1)),((2 2,98 2,98 98,2 98),(3 3,3 97,97 97,97 3)))",
                bg::no_failure);
    // and the invalid case:
    test::apply("mpg15",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(3 3,3 97,97 97,97 3)),((2 2,98 2,98 98,2 98),(1 1,1 99,99 99,99 1)))",
                bg::failure_interior_rings_outside);

    test::apply
        ("mpg16",
         "MULTIPOLYGON(((-1 4,8 -10,-10 10,7 -6,8 -2,\
                      -10 10,-10 1,-3 -4,4 1,-1 2,4 3,-8 10,-5 -9,-1 6,-5 0)),\
                      ((-10 -3,-8 1,2 -8,-2 6,-4 0,8 -5,-1 5,8 2)),\
                      ((-6 -10,1 10,4 -8,-7 -2,2 0,-4 3,-10 9)),\
                      ((10 -1,-2 8,-7 3,-6 8,-9 -7,7 -5)),\
                      ((7 7,-4 -4,9 -8,-10 -6)))",
         bg::failure_wrong_orientation);

    test::apply
        ("mpg17",
         "MULTIPOLYGON(((-1 4,8 -10,-10 10,7 -6,8 -2,\
                      -10 10,-10 1,-3 -4,4 1,-1 2,4 3,-8 10,-5 -9,-1 6,-5 0)),\
                      ((-10 -3,-8 1,2 -8,-2 6,-4 0,8 -5,-1 5,8 2)),\
                      ((-6 -10,-10 9,-4 3,2 0,-7 -2,4 -8,1 10)),\
                      ((10 -1,-2 8,-7 3,-6 8,-9 -7,7 -5)),\
                      ((7 7,-10 -6,9 -8,-4 -4)))",
         bg::failure_spikes);

    // test cases coming from buffer
    {
        std::string wkt = "MULTIPOLYGON(((1.1713032141645456 -0.9370425713316364,5.1713032141645456 4.0629574286683638,4.7808688094430307 4.3753049524455756,4.7808688094430307 4.3753049524455756,0.7808688094430304 -0.6246950475544243,0.7808688094430304 -0.6246950475544243,1.1713032141645456 -0.9370425713316364)))";

        G open_mpgn = from_wkt<G>(wkt);
        bg::reverse(open_mpgn);

        test::apply("mpg18", open_mpgn, bg::failure_wrong_orientation);
    }
    {
        std::string wkt = "MULTIPOLYGON(((5.2811206375710933 9.9800205994776228,5.2446420208654896 10.0415020265598844,5.1807360092909640 10.1691699739962242,5.1261005500004773 10.3010716408018013,5.0810140527710059 10.4365348863171388,5.0457062680576819 10.5748694208940446,5.0203571162381344 10.7153703234534277,5.0050957707794934 10.8573216336015328,5.0000000000000000 10.9999999999999964,5.0050957707794925 11.1426783663984619,5.0203571162381344 11.2846296765465670,5.0457062680576801 11.4251305791059501,5.0810140527710042 11.5634651136828559,5.1261005500004755 11.6989283591981934,5.1807360092909622 11.8308300260037704,5.2446420208654869 11.9584979734401102,5.3174929343376363 12.0812816349111927,5.3989175181512774 12.1985553330226910,5.4885008512914810 12.3097214678905669,5.5857864376269024 12.4142135623730923,5.6902785321094269 12.5114991487085145,5.8014446669773028 12.6010824818487190,5.9187183650888020 12.6825070656623602,6.0415020265598844 12.7553579791345104,6.1691699739962260 12.8192639907090360,6.3010716408018030 12.8738994499995236,6.4365348863171405 12.9189859472289950,6.5748694208940472 12.9542937319423199,6.7153703234534312 12.9796428837618656,6.8573216336015381 12.9949042292205075,7.0000000000000036 13.0000000000000000,7.1426783663984690 12.9949042292205075,7.2846296765465750 12.9796428837618656,7.4251305791059590 12.9542937319423181,7.5634651136828657 12.9189859472289932,7.6989283591982032 12.8738994499995201,7.8308300260037802 12.8192639907090324,7.9584979734401209 12.7553579791345069,8.0812816349112033 12.6825070656623566,8.1985553330227017 12.6010824818487137,8.3097214678905793 12.5114991487085092,8.4142135623731029 12.4142135623730869,8.5114991487085252 12.3097214678905598,8.6010824818487297 12.1985553330226821,8.6825070656623708 12.0812816349111838,8.7553579791345193 11.9584979734400996,8.8192639907090431 11.8308300260037580,8.8738994499995290 11.6989283591981810,8.9189859472290003 11.5634651136828417,8.9542937319423235 11.4251305791059359,8.9796428837618691 11.2846296765465510,8.9949042292205093 11.1426783663984441,9.0000000000000000 11.0000000000000000,8.9949042292205075 10.8573216336015346,8.9796428837618656 10.7153703234534294,8.9542937319423181 10.5748694208940464,8.9189859472289950 10.4365348863171405,8.8738994499995236 10.3010716408018030,8.8192639907090360 10.1691699739962278,8.7553579791345122 10.0415020265598862,8.7188787869375428 9.9800200826281831,8.8573216336015381 9.9949042292205075,9.0000000000000036 10.0000000000000000,9.1426783663984690 9.9949042292205075,9.2846296765465759 9.9796428837618656,9.4251305791059590 9.9542937319423181,9.5634651136828648 9.9189859472289932,9.6989283591982041 9.8738994499995201,9.8308300260037793 9.8192639907090324,9.9584979734401209 9.7553579791345069,10.0812816349112033 9.6825070656623566,10.1985553330227017 9.6010824818487137,10.3097214678905793 9.5114991487085092,10.4142135623731029 9.4142135623730869,10.5114991487085252 9.3097214678905598,10.6010824818487297 9.1985553330226821,10.6825070656623708 9.0812816349111838,10.7553579791345193 8.9584979734400996,10.8192639907090431 8.8308300260037580,10.8738994499995290 8.6989283591981810,10.9189859472290003 8.5634651136828417,10.9542937319423235 8.4251305791059359,10.9796428837618691 8.2846296765465510,10.9949042292205093 8.1426783663984441,11.0000000000000000 8.0000000000000000,10.9949042292205075 7.8573216336015355,10.9796428837618656 7.7153703234534294,10.9542937319423181 7.5748694208940464,10.9189859472289950 7.4365348863171405,10.8738994499995236 7.3010716408018030,10.8192639907090360 7.1691699739962269,10.7553579791345122 7.0415020265598862,10.6825070656623620 6.9187183650888047,10.6010824818487208 6.8014446669773063,10.5114991487085163 6.6902785321094296,10.4142135623730958 6.5857864376269051,10.3097214678905704 6.4885008512914837,10.1985553330226946 6.3989175181512792,10.0812816349111962 6.3174929343376380,9.9584979734401138 6.2446420208654887,9.8308300260037740 6.1807360092909640,9.6989283591981970 6.1261005500004764,9.5634651136828595 6.0810140527710050,9.4251305791059536 6.0457062680576810,9.2846296765465706 6.0203571162381344,9.1426783663984654 6.0050957707794925,9.0000000000000018 6.0000000000000000,8.8573216336015363 6.0050957707794925,8.7153703234534312 6.0203571162381344,8.5748694208940481 6.0457062680576810,8.4365348863171423 6.0810140527710050,8.3010716408018048 6.1261005500004764,8.1691699739962278 6.1807360092909622,8.0415020265598880 6.2446420208654878,7.9187183650888064 6.3174929343376363,7.8014446669773072 6.3989175181512783,7.6902785321094314 6.4885008512914819,7.5857864376269060 6.5857864376269033,7.4885008512914846 6.6902785321094278,7.3989175181512810 6.8014446669773045,7.3174929343376389 6.9187183650888029,7.2446420208654896 7.0415020265598844,7.1807360092909640 7.1691699739962251,7.1261005500004773 7.3010716408018013,7.0810140527710059 7.4365348863171379,7.0457062680576819 7.5748694208940437,7.0203571162381344 7.7153703234534268,7.0050957707794934 7.8573216336015328,7.0000000000000000 7.9999999999999973,7.0050957707794925 8.1426783663984619,7.0203571162381344 8.2846296765465670,7.0457062680576801 8.4251305791059501,7.0810140527710042 8.5634651136828559,7.1261005500004755 8.6989283591981934,7.1807360092909622 8.8308300260037704,7.2446420208654869 8.9584979734401102,7.2811219724467575 9.0199799990140797,7.1426783663984654 9.0050957707794925,7.0000000000000009 9.0000000000000000,6.8573216336015363 9.0050957707794925,6.7188786030357956 9.0199806804111571,6.7553579791345184 8.9584979734400996,6.8192639907090431 8.8308300260037580,6.8738994499995290 8.6989283591981810,6.9189859472290003 8.5634651136828417,6.9542937319423235 8.4251305791059359,6.9796428837618683 8.2846296765465510,6.9949042292205084 8.1426783663984441,7.0000000000000000 8.0000000000000000,6.9949042292205075 7.8573216336015355,6.9796428837618656 7.7153703234534294,6.9542937319423190 7.5748694208940464,6.9189859472289950 7.4365348863171405,6.8738994499995236 7.3010716408018030,6.8192639907090369 7.1691699739962269,6.7553579791345113 7.0415020265598862,6.6825070656623620 6.9187183650888047,6.6010824818487208 6.8014446669773063,6.5114991487085163 6.6902785321094296,6.4142135623730949 6.5857864376269051,6.3097214678905704 6.4885008512914837,6.1985553330226946 6.3989175181512792,6.0812816349111953 6.3174929343376380,5.9584979734401138 6.2446420208654887,5.8308300260037731 6.1807360092909640,5.6989283591981970 6.1261005500004764,5.5634651136828603 6.0810140527710050,5.4251305791059536 6.0457062680576810,5.2846296765465715 6.0203571162381344,5.1426783663984654 6.0050957707794925,5.0000000000000009 6.0000000000000000,4.8573216336015363 6.0050957707794925,4.7153703234534312 6.0203571162381344,4.5748694208940481 6.0457062680576810,4.4365348863171423 6.0810140527710050,4.3010716408018048 6.1261005500004764,4.1691699739962287 6.1807360092909622,4.0415020265598880 6.2446420208654878,3.9187183650888064 6.3174929343376363,3.8014446669773077 6.3989175181512783,3.6902785321094314 6.4885008512914819,3.5857864376269064 6.5857864376269033,3.4885008512914846 6.6902785321094278,3.3989175181512805 6.8014446669773045,3.3174929343376389 6.9187183650888029,3.2446420208654896 7.0415020265598844,3.1807360092909640 7.1691699739962251,3.1261005500004773 7.3010716408018013,3.0810140527710059 7.4365348863171379,3.0457062680576819 7.5748694208940437,3.0203571162381349 7.7153703234534268,3.0050957707794934 7.8573216336015328,3.0000000000000000 7.9999999999999973,3.0050957707794925 8.1426783663984619,3.0203571162381344 8.2846296765465670,3.0457062680576801 8.4251305791059501,3.0810140527710042 8.5634651136828559,3.1261005500004755 8.6989283591981934,3.1807360092909618 8.8308300260037704,3.2446420208654869 8.9584979734401102,3.3174929343376358 9.0812816349111927,3.3989175181512770 9.1985553330226910,3.4885008512914810 9.3097214678905669,3.5857864376269024 9.4142135623730923,3.6902785321094269 9.5114991487085145,3.8014446669773028 9.6010824818487190,3.9187183650888020 9.6825070656623602,4.0415020265598844 9.7553579791345104,4.1691699739962260 9.8192639907090360,4.3010716408018030 9.8738994499995236,4.4365348863171405 9.9189859472289950,4.5748694208940472 9.9542937319423199,4.7153703234534312 9.9796428837618656,4.8573216336015381 9.9949042292205075,5.0000000000000036 10.0000000000000000,5.1426783663984690 9.9949042292205075)))";

        G open_mpgn = from_wkt<G>(wkt);
        bg::reverse(open_mpgn);

        // polygon has a self-touching point
        test::apply("mpg19", open_mpgn, bg::failure_self_intersections);
    }
    {
        std::string wkt = "MULTIPOLYGON(((-1.1713032141645421 0.9370425713316406,-1.2278293047051545 0.8616467945203863,-1.2795097139219473 0.7828504914601357,-1.3261404828502752 0.7009646351604617,-1.3675375811487496 0.6163123916860891,-1.4035376333829217 0.5292278447680804,-1.4339985637934827 0.4400546773279756,-1.4588001570043776 0.3491448151183161,-1.4778445324579732 0.2568570378324778,-1.4910565307049013 0.1635555631651331,-1.4983840100240693 0.0696086094114048,-1.4997980522022116 -0.0246130577225216,-1.4952930766608652 -0.1187375883622537,-1.4848868624803642 -0.2123935159867641,-1.4686204782339323 -0.3052112234370423,-1.4465581199087858 -0.3968244016261590,-1.4187868575539013 -0.4868714951938814,-1.3854162916543107 -0.5749971294005020,-1.3465781205880585 -0.6608535126285795,-1.3024256208728704 -0.7441018089575634,-1.2531330422537639 -0.8244134753943718,-1.1988949200189114 -0.9014715584824893,-1.1399253072577331 -0.9749719451724563,-1.0764569300911435 -1.0446245630171400,-1.0087402692078766 -1.1101545249551616,-0.9370425713316382 -1.1713032141645441,-0.8616467945203836 -1.2278293047051563,-0.7828504914601331 -1.2795097139219491,-0.7009646351604588 -1.3261404828502767,-0.6163123916860862 -1.3675375811487509,-0.5292278447680773 -1.4035376333829228,-0.4400546773279725 -1.4339985637934838,-0.3491448151183129 -1.4588001570043785,-0.2568570378324746 -1.4778445324579736,-0.1635555631651299 -1.4910565307049017,-0.0696086094114016 -1.4983840100240695,0.0246130577225248 -1.4997980522022114,0.1187375883622569 -1.4952930766608650,0.2123935159867673 -1.4848868624803639,0.3052112234370455 -1.4686204782339316,0.3968244016261621 -1.4465581199087849,0.4868714951938845 -1.4187868575539002,0.5749971294005050 -1.3854162916543096,0.6608535126285824 -1.3465781205880569,0.7441018089575662 -1.3024256208728686,0.8244134753943745 -1.2531330422537621,0.9014715584824917 -1.1988949200189096,0.9749719451724583 -1.1399253072577313,1.0446245630171418 -1.0764569300911420,1.1101545249551634 -1.0087402692078746,1.1713032141645456 -0.9370425713316364,5.1713032141645456 4.0629574286683638,5.1713032141645439 4.0629574286683621,5.2278293047051561 4.1383532054796159,5.2795097139219491 4.2171495085398671,5.3261404828502767 4.2990353648395407,5.3675375811487509 4.3836876083139131,5.4035376333829230 4.4707721552319217,5.4339985637934838 4.5599453226720268,5.4588001570043785 4.6508551848816859,5.4778445324579739 4.7431429621675241,5.4910565307049017 4.8364444368348689,5.4983840100240693 4.9303913905885972,5.4997980522022116 5.0246130577225232,5.4952930766608645 5.1187375883622552,5.4848868624803639 5.2123935159867658,5.4686204782339320 5.3052112234370439,5.4465581199087856 5.3968244016261604,5.4187868575539007 5.4868714951938822,5.3854162916543107 5.5749971294005025,5.3465781205880578 5.6608535126285799,5.3024256208728699 5.7441018089575637,5.2531330422537632 5.8244134753943726,5.1988949200189110 5.9014715584824895,5.1399253072577329 5.9749719451724559,5.0764569300911440 6.0446245630171394,5.0087402692078768 6.1101545249551616,4.9370425713316379 6.1713032141645439,4.8616467945203841 6.2278293047051561,4.7828504914601337 6.2795097139219482,4.7009646351604593 6.3261404828502759,4.6163123916860869 6.3675375811487509,4.5292278447680783 6.4035376333829230,4.4400546773279732 6.4339985637934838,4.3491448151183141 6.4588001570043785,4.2568570378324750 6.4778445324579739,4.1635555631651311 6.4910565307049017,4.0696086094114028 6.4983840100240693,3.9753869422774759 6.4997980522022116,3.8812624116377439 6.4952930766608645,3.7876064840132333 6.4848868624803639,3.6947887765629552 6.4686204782339320,3.6031755983738387 6.4465581199087847,3.5131285048061165 6.4187868575539007,3.4250028705994957 6.3854162916543098,3.3391464873714183 6.3465781205880578,3.2558981910424345 6.3024256208728691,3.1755865246056261 6.2531330422537623,3.0985284415175087 6.1988949200189101,3.0250280548275423 6.1399253072577320,2.9553754369828584 6.0764569300911422,2.8898454750448366 6.0087402692078751,2.8286967858354544 5.9370425713316362,-1.1713032141645456 0.9370425713316364,-1.1713032141645421 0.9370425713316406)))";

        G open_mpgn = from_wkt<G>(wkt);
        bg::reverse(open_mpgn);

        // polygon contains a spike
        test::apply("mpg20", open_mpgn, bg::failure_spikes);
    }
}

BOOST_AUTO_TEST_CASE( test_failure_multipolygon )
{
    test_open_multipolygons<point_type>();
}

BOOST_AUTO_TEST_CASE( test_failure_variant )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid_failure: variant support" << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<point_type> polygon_type; // cw, closed

    typedef boost::variant
        <
            linestring_type, multi_linestring_type, polygon_type
        > variant_geometry;
    typedef test_failure<variant_geometry> test;

    variant_geometry vg;

    linestring_type valid_linestring =
        from_wkt<linestring_type>("LINESTRING(0 0,1 0)");
    multi_linestring_type invalid_multi_linestring =
        from_wkt<multi_linestring_type>("MULTILINESTRING((0 0,1 0),(0 0))");
    polygon_type valid_polygon =
        from_wkt<polygon_type>("POLYGON((0 0,1 1,1 0,0 0))");
    polygon_type invalid_polygon =
        from_wkt<polygon_type>("POLYGON((0 0,2 2,2 0,1 0))");

    vg = valid_linestring;
    test::apply("v01", vg, bg::no_failure);
    vg = invalid_multi_linestring;
    test::apply("v02", vg, bg::failure_few_points);
    vg = valid_polygon;
    test::apply("v03", vg, bg::no_failure);
    vg = invalid_polygon;
    test::apply("v04", vg, bg::failure_not_closed);
}
