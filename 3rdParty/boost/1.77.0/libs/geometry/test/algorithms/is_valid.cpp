// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_is_valid
#endif

#include <limits>
#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include "test_is_valid.hpp"
#include "overlay/overlay_cases.hpp"

#include <boost/geometry/core/coordinate_type.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/reverse.hpp>

BOOST_AUTO_TEST_CASE( test_is_valid_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: POINT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef point_type G;
    typedef default_validity_tester tester;
    typedef test_valid<tester, G> test;

    test::apply("p01", "POINT(0 0)", true);
}

BOOST_AUTO_TEST_CASE( test_is_valid_multipoint )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: MULTIPOINT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef multi_point_type G;
    typedef default_validity_tester tester;
    typedef test_valid<tester, G> test;

    test::apply("mp01", "MULTIPOINT()", true);
    test::apply("mp02", "MULTIPOINT(0 0,0 0)", true);
    test::apply("mp03", "MULTIPOINT(0 0,1 0,1 1,0 1)", true);
    test::apply("mp04", "MULTIPOINT(0 0,1 0,1 1,1 0,0 1)", true);
}

BOOST_AUTO_TEST_CASE( test_is_valid_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: SEGMENT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef segment_type G;
    typedef default_validity_tester tester;
    typedef test_valid<tester, G> test;

    test::apply("s01", "SEGMENT(0 0,0 0)", false);
    test::apply("s02", "SEGMENT(0 0,1 0)", true);
}

BOOST_AUTO_TEST_CASE( test_is_valid_box )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: BOX " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef box_type G;
    typedef default_validity_tester tester;
    typedef test_valid<tester, G> test;

    // boxes where the max corner and below and/or to the left of min corner
    test::apply("b01", "BOX(0 0,-1 0)", false);
    test::apply("b02", "BOX(0 0,0 -1)", false);
    test::apply("b03", "BOX(0 0,-1 -1)", false);

    // boxes of zero area; they are not 2-dimensional, so invalid
    test::apply("b04", "BOX(0 0,0 0)", false);
    test::apply("b05", "BOX(0 0,1 0)", false);
    test::apply("b06", "BOX(0 0,0 1)", false);

    test::apply("b07", "BOX(0 0,1 1)", true);
}

template <typename G, bool AllowSpikes>
void test_linestrings()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "SPIKES ALLOWED? "
              << std::boolalpha
              << AllowSpikes
              << std::noboolalpha
              << std::endl;
#endif

    typedef validity_tester_linear<AllowSpikes> tester;
    typedef test_valid<tester, G> test;

    // empty linestring
    test::apply("l01", "LINESTRING()", false);

    // 1-point linestrings
    test::apply("l02", "LINESTRING(0 0)", false);
    test::apply("l03", "LINESTRING(0 0,0 0)", false);
    test::apply("l04", "LINESTRING(0 0,0 0,0 0)", false);

    // 2-point linestrings
    test::apply("l05", "LINESTRING(0 0,1 2)", true);
    test::apply("l06", "LINESTRING(0 0,1 2,1 2)", true);
    test::apply("l07", "LINESTRING(0 0,0 0,1 2,1 2)", true);
    test::apply("l08", "LINESTRING(0 0,0 0,0 0,1 2,1 2)", true);

    // 3-point linestrings
    test::apply("l09", "LINESTRING(0 0,1 0,2 10)", true);
    test::apply("l10", "LINESTRING(0 0,1 0,2 10,0 0)", true);
    test::apply("l11", "LINESTRING(0 0,10 0,10 10,5 0)", true);

    // linestrings with spikes
    test::apply("l12", "LINESTRING(0 0,1 2,0 0)", AllowSpikes);
    test::apply("l13", "LINESTRING(0 0,1 2,1 2,0 0)", AllowSpikes);
    test::apply("l14", "LINESTRING(0 0,0 0,1 2,1 2,0 0)", AllowSpikes);
    test::apply("l15", "LINESTRING(0 0,0 0,0 0,1 2,1 2,0 0,0 0)", AllowSpikes);
    test::apply("l16", "LINESTRING(0 0,10 0,5 0)", AllowSpikes);
    test::apply("l17", "LINESTRING(0 0,10 0,10 10,5 0,0 0)", AllowSpikes);
    test::apply("l18", "LINESTRING(0 0,10 0,10 10,5 0,4 0,6 0)", AllowSpikes);
    test::apply("l19", "LINESTRING(0 0,1 0,1 1,5 5,4 4)", AllowSpikes);
    test::apply("l20", "LINESTRING(0 0,1 0,1 1,5 5,4 4,6 6)", AllowSpikes);
    test::apply("l21", "LINESTRING(0 0,1 0,1 1,5 5,4 4,4 0)", AllowSpikes);
    test::apply("l22",
                "LINESTRING(0 0,0 0,1 0,1 0,1 0,0 0,0 0,2 0)",
                AllowSpikes);
    test::apply("l23",
                "LINESTRING(0 0,1 0,0 0,2 0,0 0,3 0,0 0,4 0)",
                AllowSpikes);
    test::apply("l24",
                "LINESTRING(0 0,1 0,0 0,2 0,0 0,3 0,0 0,4 0,0 0)",
                AllowSpikes);

    // other examples
    test::apply("l25", "LINESTRING(0 0,10 0,10 10,5 0,4 0)", true);
    test::apply("l26", "LINESTRING(0 0,10 0,10 10,5 0,4 0,3 0)", true);
    test::apply("l27", "LINESTRING(0 0,10 0,10 10,5 0,4 0,-1 0)", true);
    test::apply("l28", "LINESTRING(0 0,1 0,1 1,-1 1,-1 0,0 0)", true);
    test::apply("l29", "LINESTRING(0 0,1 0,1 1,-1 1,-1 0,0.5 0)", true);
}

BOOST_AUTO_TEST_CASE( test_is_valid_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: LINESTRING " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    bool const allow_spikes = true;
    bool const do_not_allow_spikes = !allow_spikes;

    test_linestrings<linestring_type, allow_spikes>();
    test_linestrings<linestring_type, do_not_allow_spikes>();
}

template <typename G, bool AllowSpikes>
void test_multilinestrings()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "SPIKES ALLOWED? "
              << std::boolalpha
              << AllowSpikes
              << std::noboolalpha
              << std::endl;
#endif

    typedef validity_tester_linear<AllowSpikes> tester;
    typedef test_valid<tester, G> test;

    // empty multilinestring
    test::apply("mls01", "MULTILINESTRING()", true);

    // multilinestring with empty linestring(s)
    test::apply("mls02", "MULTILINESTRING(())", false);
    test::apply("mls03", "MULTILINESTRING((),(),())", false);
    test::apply("mls04", "MULTILINESTRING((),(0 1,1 0))", false);

    // multilinestring with invalid linestrings
    test::apply("mls05", "MULTILINESTRING((0 0),(0 1,1 0))", false);
    test::apply("mls06", "MULTILINESTRING((0 0,0 0),(0 1,1 0))", false);
    test::apply("mls07", "MULTILINESTRING((0 0),(1 0))", false);
    test::apply("mls08", "MULTILINESTRING((0 0,0 0),(1 0,1 0))", false);
    test::apply("mls09", "MULTILINESTRING((0 0),(0 0))", false);
    test::apply("mls10", "MULTILINESTRING((0 0,1 0,0 0),(5 0))", false);

    // multilinstring that has linestrings with spikes
    test::apply("mls11",
                "MULTILINESTRING((0 0,1 0,0 0),(5 0,1 0,4 1))",
                AllowSpikes);
    test::apply("mls12",
                "MULTILINESTRING((0 0,1 0,0 0),(1 0,2 0))",
                AllowSpikes);

    // valid multilinestrings
    test::apply("mls13", "MULTILINESTRING((0 0,1 0,2 0),(5 0,1 0,4 1))", true);
    test::apply("mls14", "MULTILINESTRING((0 0,1 0,2 0),(1 0,2 0))", true);
    test::apply("mls15", "MULTILINESTRING((0 0,1 1),(0 1,1 0))", true);
    test::apply("mls16", "MULTILINESTRING((0 0,1 1,2 2),(0 1,1 0,2 2))", true);
}

BOOST_AUTO_TEST_CASE( test_is_valid_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: MULTILINESTRING " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    bool const allow_spikes = true;
    bool const do_not_allow_spikes = !allow_spikes;

    test_multilinestrings<multi_linestring_type, allow_spikes>();
    test_multilinestrings<multi_linestring_type, do_not_allow_spikes>();
}

template <typename Point, bool AllowDuplicates>
inline void test_open_rings()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: RING (open) " << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << "DUPLICATES ALLOWED? "
              << std::boolalpha
              << AllowDuplicates
              << std::noboolalpha
              << std::endl;
#endif

    typedef bg::model::ring<Point, false, false> OG; // ccw, open ring
    typedef bg::model::ring<Point, false, true> CG; // ccw, closed ring
    typedef bg::model::ring<Point, true, false> CW_OG; // cw, open ring
    typedef bg::model::ring<Point, true, true> CW_CG; // cw, closed ring
 
    typedef validity_tester_areal<AllowDuplicates> tester;
    typedef test_valid<tester, OG, CG, CW_OG, CW_CG> test;

    // not enough points
    test::apply("r01", "POLYGON(())", false);
    test::apply("r02", "POLYGON((0 0))", false);
    test::apply("r03", "POLYGON((0 0,1 0))", false);

    // duplicate points
    test::apply("r04", "POLYGON((0 0,0 0,0 0))", false);
    test::apply("r05", "POLYGON((0 0,1 0,1 0))", false);
    test::apply("r06", "POLYGON((0 0,1 0,0 0))", false);
    test::apply("r07", "POLYGON((0 0,1 0,1 1,0 0,0 0))", AllowDuplicates);
    test::apply("r08", "POLYGON((0 0,1 0,1 0,1 1))", AllowDuplicates);
    test::apply("r09", "POLYGON((0 0,1 0,1 0,1 1,0 0))", AllowDuplicates);

    // with spikes
    test::apply("r10", "POLYGON((0 0,2 0,2 2,0 2,1 2))", false);
    test::apply("r11", "POLYGON((0 0,2 0,1 0,2 2))", false);
    test::apply("r12", "POLYGON((0 0,1 0,2 0,1 0,4 0,4 4))", false);
    test::apply("r13", "POLYGON((0 0,2 0,2 2,1 0))", false);
    test::apply("r14", "POLYGON((0 0,2 0,1 0))", false);
    test::apply("r15", "POLYGON((0 0,5 0,5 5,4 4,5 5,0 5))", false);
    test::apply("r16", "POLYGON((0 0,5 0,5 5,4 4,3 3,5 5,0 5))", false);

    // with spikes and duplicate points
    test::apply("r17", "POLYGON((0 0,0 0,2 0,2 0,1 0,1 0))", false);

    // with self-crossings
    test::apply("r18", "POLYGON((0 0,5 0,5 5,3 -1,0 5))", false);

    // with self-crossings and duplicate points
    test::apply("r19", "POLYGON((0 0,5 0,5 5,5 5,3 -1,0 5,0 5))", false);

    // with self-intersections
    test::apply("r20", "POLYGON((0 0,5 0,5 5,3 5,3 0,2 0,2 5,0 5))", false);

    test::apply("r21", "POLYGON((0 0,5 0,5 5,3 5,3 0,2 5,0 5))", false);

    test::apply("r22",
                "POLYGON((0 0,5 0,5 1,1 1,1 2,2 2,3 1,4 2,5 2,5 5,0 5))",
                false);

    // with self-intersections and duplicate points
    test::apply("r23",
                "POLYGON((0 0,5 0,5 5,3 5,3 5,3 0,3 0,2 0,2 0,2 5,2 5,0 5))",
                false);

    // next two suggested by Adam Wulkiewicz
    test::apply("r24",
                "POLYGON((0 0,5 0,5 5,0 5,4 4,2 2,0 5))",
                false);
    test::apply("r25",
                "POLYGON((0 0,5 0,5 5,1 4,4 4,4 1,0 5))",
                false);

    // and a few more
    test::apply("r26",
                "POLYGON((0 0,5 0,5 5,4 4,1 4,1 1,4 1,4 4,0 5))",
                false);
    test::apply("r27",
                "POLYGON((0 0,5 0,5 5,4 4,4 1,1 1,1 4,4 4,0 5))",
                false);

    // valid rings
    test::apply("r28", "POLYGON((0 0,1 0,1 1))", true);
    test::apply("r29", "POLYGON((1 0,1 1,0 0))", true);
    test::apply("r30", "POLYGON((0 0,1 0,1 1,0 1))", true);
    test::apply("r31", "POLYGON((1 0,1 1,0 1,0 0))", true);

    // test cases coming from buffer
    test::apply("r32",
                "POLYGON((1.1713032141645456 -0.9370425713316364,\
                          5.1713032141645456 4.0629574286683638,\
                          4.7808688094430307 4.3753049524455756,\
                          4.7808688094430307 4.3753049524455756,\
                          0.7808688094430304 -0.6246950475544243,\
                          0.7808688094430304 -0.6246950475544243))",
                AllowDuplicates);

    // wrong orientation
    test::apply("r33", "POLYGON((0 0,0 1,1 1))", false);

    // Normal case, plus spikes formed in two different ways
    test::apply("r34", "POLYGON((0 0,4 0,4 4,0 4,0 0))", true);
    test::apply("r35", "POLYGON((0 0,5 0,4 0,4 4,0 4,0 0))", false);
    test::apply("r36", "POLYGON((0 0,4 0,4 -1,4 4,0 4,0 0))", false);
}


template <typename Point, bool AllowDuplicates>
inline void test_closed_rings()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: RING (closed) " << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << "DUPLICATES ALLOWED? "
              << std::boolalpha
              << AllowDuplicates
              << std::noboolalpha
              << std::endl;
#endif

    typedef bg::model::ring<Point, false, true> CG; // ccw, closed ring
    typedef bg::model::ring<Point, true, true> CW_CG; // cw, closed ring

    typedef validity_tester_areal<AllowDuplicates> tester;
    typedef test_valid<tester, CG, CG, CW_CG> test;

    // not enough points
    test::apply("r01c", "POLYGON(())", false);
    test::apply("r02c", "POLYGON((0 0))", false);
    test::apply("r03c", "POLYGON((0 0,0 0))", false);
    test::apply("r04c", "POLYGON((0 0,1 0))", false);
    test::apply("r05c", "POLYGON((0 0,1 0,1 0))", false);
    test::apply("r06c", "POLYGON((0 0,1 0,2 0))", false);
    test::apply("r07c", "POLYGON((0 0,1 0,1 0,2 0))", false);
    test::apply("r08c", "POLYGON((0 0,1 0,2 0,2 0))", false);

    // boundary not closed
    test::apply("r09c", "POLYGON((0 0,1 0,1 1,1 2))", false);
    test::apply("r10c", "POLYGON((0 0,1 0,1 0,1 1,1 1,1 2))", false);

    // with spikes
    test::apply("r11c", "POLYGON((0 0,1 0,1 0,2 0,0 0))", false);
    test::apply("r12c", "POLYGON((0 0,1 0,1 1,2 2,0.5 0.5,0 1,0 0))", false);

    // wrong orientation
    test::apply("r13c", "POLYGON((0 0,0 1,1 1,2 0,0 0))", false);
}

BOOST_AUTO_TEST_CASE( test_is_valid_ring )
{
    bool const allow_duplicates = true;
    bool const do_not_allow_duplicates = !allow_duplicates;

    test_open_rings<point_type, allow_duplicates>();
    test_open_rings<point_type, do_not_allow_duplicates>();

    test_closed_rings<point_type, allow_duplicates>();
    test_closed_rings<point_type, do_not_allow_duplicates>();
}

template <typename Point, bool AllowDuplicates>
inline void test_open_polygons()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: POLYGON (open) " << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << "DUPLICATES ALLOWED? "
              << std::boolalpha
              << AllowDuplicates
              << std::noboolalpha
              << std::endl;
#endif

    typedef bg::model::polygon<Point, false, false> OG; // ccw, open
    typedef bg::model::polygon<Point, false, true> CG; // ccw, closed
    typedef bg::model::polygon<Point, true, false> CW_OG; // cw, open
    typedef bg::model::polygon<Point, true, true> CW_CG; // cw, closed

    typedef validity_tester_areal<AllowDuplicates> tester;
    typedef test_valid<tester, OG, CG, CW_OG, CW_CG> test;

    // not enough points in exterior ring
    test::apply("pg001", "POLYGON(())", false);
    test::apply("pg002", "POLYGON((0 0))", false);
    test::apply("pg003", "POLYGON((0 0,1 0))", false);

    // not enough points in interior ring
    test::apply("pg004", "POLYGON((0 0,10 0,10 10,0 10),())", false);
    test::apply("pg005", "POLYGON((0 0,10 0,10 10,0 10),(1 1))", false);
    test::apply("pg006", "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2))", false);

    // duplicate points in exterior ring
    test::apply("pg007", "POLYGON((0 0,0 0,0 0))", false);
    test::apply("pg008", "POLYGON((0 0,1 0,1 0))", false);
    test::apply("pg009", "POLYGON((0 0,1 0,0 0))", false);
    test::apply("pg010", "POLYGON((0 0,1 0,1 1,0 0,0 0))", AllowDuplicates);
    test::apply("pg011", "POLYGON((0 0,1 0,1 0,1 1))", AllowDuplicates);
    test::apply("pg012", "POLYGON((0 0,1 0,1 0,1 1,0 0))",  AllowDuplicates);

    // duplicate points in interior ring
    test::apply("pg013", "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 1,1 1))", false);
    test::apply("pg014", "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,2 1))", false);
    test::apply("pg015", "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,1 1))", false);
    test::apply("pg016",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2,2 1,1 1,1 1))",
                AllowDuplicates);
    test::apply("pg017",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2,2 2,2 1))",
                AllowDuplicates);
    test::apply("pg018",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 2,2 1,2 1,1 1))",
                AllowDuplicates);

    // with spikes in exterior ring
    test::apply("pg019", "POLYGON((0 0,2 0,2 2,0 2,1 2))", false);
    test::apply("pg020", "POLYGON((0 0,2 0,1 0,2 2))", false);
    test::apply("pg021", "POLYGON((0 0,1 0,2 0,1 0,4 0,4 4))", false);
    test::apply("pg022", "POLYGON((0 0,2 0,2 2,1 0))", false);
    test::apply("pg023", "POLYGON((0 0,2 0,1 0))", false);
    test::apply("pg024", "POLYGON((0 0,5 0,5 5,4 4,5 5,0 5))", false);
    test::apply("pg025", "POLYGON((0 0,5 0,5 5,4 4,3 3,5 5,0 5))", false);

    // with spikes in interior ring
    test::apply("pg026",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,3 3,1 3,2 3))",
                false);
    test::apply("pg027",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,2 1,3 3))",
                false);
    test::apply("pg028",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,3 1,2 1,4 1,4 4))",
                false);
    test::apply("pg029",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,3 3,2 1))",
                false);
    test::apply("pg030", "POLYGON((0 0,10 0,10 10,0 10),(1 1,3 1,2 1))", false);

    // with self-crossings in exterior ring
    test::apply("pg031", "POLYGON((0 0,5 0,5 5,3 -1,0 5))", false);

    // example from Norvald Ryeng
    test::apply("pg032",
                "POLYGON((100 1300,140 1300,140 170,100 1700))",
                false);
    // and with point order reversed
    test::apply("pg033",
                "POLYGON((100 1300,100 1700,140 170,140 1300))",
                false);

    // with self-crossings in interior ring
    test::apply("pg034",
                "POLYGON((0 0,10 0,10 10,0 10),(3 3,3 7,4 6,2 6))",
                false);

    // with self-crossings between rings
    test::apply("pg035", "POLYGON((0 0,5 0,5 5,0 5),(1 1,2 1,1 -1))", false);

    // with self-intersections in exterior ring
    test::apply("pg036", "POLYGON((0 0,5 0,5 5,3 5,3 0,2 0,2 5,0 5))", false);
    test::apply("pg037", "POLYGON((0 0,5 0,5 5,3 5,3 0,2 5,0 5))", false);
    test::apply("pg038",
                "POLYGON((0 0,5 0,5 1,1 1,1 2,2 2,3 1,4 2,5 2,5 5,0 5))",
                false);

    // next two suggested by Adam Wulkiewicz
    test::apply("pg039", "POLYGON((0 0,5 0,5 5,0 5,4 4,2 2,0 5))", false);
    test::apply("pg040", "POLYGON((0 0,5 0,5 5,1 4,4 4,4 1,0 5))", false);
    test::apply("pg041",
                "POLYGON((0 0,5 0,5 5,4 4,1 4,1 1,4 1,4 4,0 5))",
                false);
    test::apply("pg042",
                "POLYGON((0 0,5 0,5 5,4 4,4 1,1 1,1 4,4 4,0 5))",
                false);

    // with self-intersections in interior ring
    test::apply("pg043",
                "POLYGON((-10 -10,10 -10,10 10,-10 10),(0 0,5 0,5 5,3 5,3 0,2 0,2 5,0 5))",
                false);
    test::apply("pg044",
                "POLYGON((-10 -10,10 -10,10 10,-10 10),(0 0,5 0,5 5,3 5,3 0,2 5,0 5))",
                false);

    test::apply("pg045",
                "POLYGON((-10 -10,10 -10,10 10,-10 10),(0 0,5 0,5 1,1 1,1 2,2 2,3 1,4 2,5 2,5 5,0 5))",
                false);

    // with self-intersections between rings
    // hole has common segment with exterior ring
    test::apply("pg046",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 10,2 10,2 1))",
                false);
    test::apply("pg047",
                "POLYGON((0 0,0 0,10 0,10 10,0 10,0 10),(1 1,1 10,1 10,2 10,2 10,2 1))",
                false);
    // hole touches exterior ring at one point
    test::apply("pg048", "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 10,2 1))", true);

    // "hole" is outside the exterior ring, but touches it
    test::apply("pg049",
                "POLYGON((0 0,10 0,10 10,0 10),(5 10,4 11,6 11))",
                false);

    // hole touches exterior ring at vertex
    test::apply("pg050", "POLYGON((0 0,10 0,10 10,0 10),(0 0,1 4,4 1))", true);

    // "hole" is completely outside the exterior ring
    test::apply("pg051",
                "POLYGON((0 0,10 0,10 10,0 10),(20 20,20 21,21 21,21 20))",
                false);

    // two "holes" completely outside the exterior ring, that touch
    // each other
    test::apply("pg052",
                "POLYGON((0 0,10 0,10 10,0 10),(20 0,25 10,21 0),(30 0,25 10,31 0))",
                false);

    // example from Norvald Ryeng
    test::apply("pg053",
                "POLYGON((58 31,56.57 30,62 33),(35 9,28 14,31 16),(23 11,29 5,26 4))",
                false);
    // and with points reversed
    test::apply("pg054",
                "POLYGON((58 31,62 33,56.57 30),(35 9,31 16,28 14),(23 11,26 4,29 5))",
                false);

    // "hole" is completely inside another "hole"
    test::apply("pg055",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,8 2))",
                false);
    test::apply("pg056",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,8 2,8 8,2 8))",
                false);

    // "hole" is inside another "hole" (touching)
    test::apply("pg057",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                false);
    test::apply("pg058",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                false);
    test::apply("pg058a",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,8 2,9 6,8 8,2 8))",
                false);
    test::apply("pg059",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                false);
    test::apply("pg059a",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,9 1,9 9,1 9),(2 2,2 8,8 8,9 6,8 2))",
                false);
    test::apply("pg060",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1),(2 2,2 8,8 8,9 6,8 2))",
                false);
    test::apply("pg060a",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,9 1,9 9,1 9),(2 2,8 2,9 6,8 8,2 8))",
                false);
    // hole touches exterior ring at two points
    test::apply("pg061", "POLYGON((0 0,10 0,10 10,0 10),(5 0,0 5,5 5))", false);

    // cases with more holes
    // two holes, touching the exterior at the same point
    test::apply("pg062",
                "POLYGON((0 0,10 0,10 10,0 10),(0 0,1 9,2 9),(0 0,9 2,9 1))",
                true);
    test::apply("pg063",
                "POLYGON((0 0,0 0,10 0,10 10,0 10,0 0),(0 0,0 0,1 9,2 9),(0 0,0 0,9 2,9 1))",
                AllowDuplicates);
    test::apply("pg063",
                "POLYGON((0 10,0 0,0 0,0 0,10 0,10 10),(2 9,0 0,0 0,1 9),(9 1,0 0,0 0,9 2))",
                AllowDuplicates);
    // two holes, one inside the other
    test::apply("pg064",
                "POLYGON((0 0,10 0,10 10,0 10),(0 0,1 9,9 1),(0 0,4 5,5 4))",
                false);
    // 1st hole touches has common segment with 2nd hole
    test::apply("pg066",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 5,5 5,5 1),(5 4,5 8,8 8,8 4))",
                false);
    // 1st hole touches 2nd hole at two points
    test::apply("pg067",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 8,2 8,2 1),(2 5,5 8,5 5))",
                false);
    // polygon with many holes, where the last two touch at two points
    test::apply("pg068",
                "POLYGON((0 0,20 0,20 20,0 20),(1 18,1 19,2 19,2 18),(3 18,3 19,4 19,4 18),(5 18,5 19,6 19,6 18),(7 18,7 19,8 19,8 18),(9 18,9 19,10 19,10 18),(11 18,11 19,12 19,12 18),(13 18,13 19,14 19,14 18),(15 18,15 19,16 19,16 18),(17 18,17 19,18 19,18 18),(1 1,1 9,9 9,9 8,2 8,2 1),(2 5,5 8,5 5))",
                false);
    // two holes completely inside exterior ring but touching each
    // other at a point
    test::apply("pg069",
                "POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,2 9),(1 1,9 2,9 1))",
                true);
    // four holes, each two touching at different points
    test::apply("pg070",
                "POLYGON((0 0,10 0,10 10,0 10),(0 10,2 1,1 1),(0 10,4 1,3 1),(10 10,9 1,8 1),(10 10,7 1,6 1))",
                true);
    // five holes, with two pairs touching each at some point, and
    // fifth hole creating a disconnected component for the interior
    test::apply("pg071",
                "POLYGON((0 0,10 0,10 10,0 10),(0 10,2 1,1 1),(0 10,4 1,3 1),(10 10,9 1,8 1),(10 10,7 1,6 1),(4 1,4 4,6 4,6 1))",
                false);
    // five holes, with two pairs touching each at some point, and
    // fifth hole creating three disconnected components for the interior
    test::apply("pg072",
                "POLYGON((0 0,10 0,10 10,0 10),(0 10,2 1,1 1),(0 10,4 1,3 1),(10 10,9 1,8 1),(10 10,7 1,6 1),(4 1,4 4,6 4,6 1,5 0))",
                false);

    // both examples: a polygon with one hole, where the hole contains
    // the exterior ring
    test::apply("pg073",
                "POLYGON((0 0,1 0,1 1,0 1),(-10 -10,-10 10,10 10,10 -10))",
                false);
    test::apply("pg074",
                "POLYGON((-10 -10,1 0,1 1,0 1),(-10 -10,-10 10,10 10,10 -10))",
                false);

    test::apply
        ("pg075",
         "POLYGON((-6 -10,-6.6923076923076925 -6.711538461538462,\
                   -9 -7,-8.824742268041238 -6.123711340206185,\
                   -10 -6,-8.583333333333332 -4.916666666666667,\
                   -8.094117647058823 -2.4705882352941173,-10 -3,\
                   -8.526315789473683 -0.05263157894736803,-10 1,\
                   -10 10,-7.764705882352941 8.509803921568627,\
                   -7.65090909090909 7.789090909090909,-10 10,\
                   -7.574468085106383 7.304964539007091,-7.4375 6.4375,\
                   -6.5 5.5,-6.4 6,-7.574468085106383 7.304964539007091,\
                   -7.65090909090909 7.789090909090909,\
                   -6.297029702970297 6.514851485148515,\
                   0 0,-6.297029702970297 6.514851485148515,\
                   -4.848484848484849 5.151515151515151,-4 6,\
                   -6.117647058823529 7.411764705882352,\
                   0 0,-6.11764705882353 7.411764705882353,\
                   -7.764705882352941 8.509803921568627,-8 10,\
                   -2.9473684210526314 7.052631578947368,-2 8,\
                   -0.17821782178217824 6.633663366336634,1 10,\
                   1.8095238095238098 5.142857142857142,\
                   3.2038834951456314 4.097087378640777,7 7,\
                   3.7142857142857144 3.7142857142857144,\
                   4.4 3.1999999999999997,8 2,\
                   6.540540540540541 1.5945945945945947,10 -1,\
                   7.454545454545455 -4.393939393939394,8 -5,\
                   7.320754716981132 -4.716981132075472,7 -6,\
                   6.062068965517241 -5.117241379310345,\
                   4.9504132231404965 -5.256198347107438,\
                   6.1506849315068495 -7.123287671232877,9 -8,\
                   6.548387096774194 -7.741935483870968,8 -10,\
                   5.906976744186046 -7.674418604651163,\
                   3.9107142857142856 -7.464285714285714,4 -8,\
                   2.8043478260869565 -7.3478260869565215,\
                   1.7829457364341086 -7.24031007751938,2 -8,\
                   1.0728476821192054 -7.1655629139072845,\
                   -4.3583617747440275 -6.593856655290103,-5 -9,\
                   -5.2020725388601035 -7.720207253886011,-6 -10),\
                  (5.127659574468085 -6.808510638297872,\
                   3.72972972972973 -6.378378378378379,\
                   3.571428571428571 -5.428571428571429,\
                   3.8539325842696632 -5.393258426966292,\
                   5.127659574468085 -6.808510638297872),\
                  (-5.5 4.5,-6.5 5.5,-6.4 6,\
                   -5.263157894736842 4.736842105263158,-5.5 4.5))",
         false);

    // test cases coming from buffer
    test::apply
        ("pg076",
         "POLYGON((1.1713032141645456 -0.9370425713316364,\
                   5.1713032141645456 4.0629574286683638,\
                   4.7808688094430307 4.3753049524455756,\
                   4.7808688094430307 4.3753049524455756,\
                   0.7808688094430304 -0.6246950475544243,\
                   0.7808688094430304 -0.6246950475544243))",
         AllowDuplicates);


    // MySQL report on Sep 30, 2015
    test::apply
        ("pg077",
         "POLYGON((72.8714768817168 -167.0048853643874,9274.40641550926 3433.5957427942167,-58.09039811390054 187.50989457746405,-81.09039811390053 179.50989457746405,-207.99999999999997 135.36742435621204,-208 1,-208 0,-208 -276.9111154485375,49.8714768817168 -176.0048853643874))",
         true);

    test::apply("pg077-simplified",
                "POLYGON((-200 0,-207.99999999999997 135.36742435621204,-208 1,-208 0,-208 -276.9111154485375))",
                true);

    test::apply
        ("pg078",
         "POLYGON((0 10,-10 0,0 0,10 0))",
         true);

    test::apply
        ("pg078spike1",
         "POLYGON((0 10,-10 0,0 0,-10 0,10 0))",
         false);

    test::apply
        ("pg078spike2",
         "POLYGON((0 10,-10 0,0 0,-8 0,10 0))",
         false);

    test::apply
        ("pg078spike3",
         "POLYGON((0 10,-10 0,0 0,-11 0,10 0))",
         false);

    test::apply
        ("pg078reversed",
         "POLYGON((0 10,10 0,0 0,-10 0))",
         false);

    test::apply
        ("pg079",
         "POLYGON((10 0,0 10,0 0,0 -10))",
         true);

    test::apply
        ("pg079spike1",
         "POLYGON((10 0,0 10,0 0,0 10,0 -10))",
         false);

    test::apply
        ("pg079spike2",
         "POLYGON((10 0,0 10,0 0,0 8,0 -10))",
         false);

    test::apply
        ("pg079spike3",
         "POLYGON((10 0,0 10,0 0,0 11,0 -10))",
         false);

    test::apply
        ("pg079reversed",
         "POLYGON((10 0,0 -10,0 0,0 10))",
         false);
}


template <typename Point>
inline void test_doc_example_polygon()
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: doc example polygon " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<Point> ClockwiseClosedPolygon;
    typedef validity_tester_areal<true> tester;
    typedef test_valid<tester, ClockwiseClosedPolygon> test;

    test::apply("pg-doc",
                "POLYGON((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 2,0 0),(0 0,2 9,1 9,0 0),(2 9,9 2,9 9,2 9))",
                false);

    // Containing a self touching point, which should be valid
    test::apply("ggl_list_20190307_matthieu_2", ggl_list_20190307_matthieu_2[1], true);
}

BOOST_AUTO_TEST_CASE( test_is_valid_polygon )
{
    bool const allow_duplicates = true;
    bool const do_not_allow_duplicates = !allow_duplicates;

    test_open_polygons<point_type, allow_duplicates>();
    test_open_polygons<point_type, do_not_allow_duplicates>();
    test_doc_example_polygon<point_type>();
}

template <typename Point, bool AllowDuplicates>
inline void test_open_multipolygons()
{
    // WKT of multipolygons should be defined as ccw, open

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: MULTIPOLYGON (open) " << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << "DUPLICATES ALLOWED? "
              << std::boolalpha
              << AllowDuplicates
              << std::noboolalpha
              << std::endl;
#endif

    // cw, ccw, open and closed polygons
    typedef bg::model::polygon<point_type,false,false> ccw_open_polygon_type;
    typedef bg::model::polygon<point_type,false,true>  ccw_closed_polygon_type;
    typedef bg::model::polygon<point_type,true,false>  cw_open_polygon_type;
    typedef bg::model::polygon<point_type,true,true>   cw_closed_polygon_type;

    typedef bg::model::multi_polygon<ccw_open_polygon_type> OG;
    typedef bg::model::multi_polygon<ccw_closed_polygon_type> CG;
    typedef bg::model::multi_polygon<cw_open_polygon_type> CW_OG;
    typedef bg::model::multi_polygon<cw_closed_polygon_type> CW_CG;

    typedef validity_tester_areal<AllowDuplicates> tester;
    typedef test_valid<tester, OG, CG, CW_OG, CW_CG> test;

    // not enough points
    test::apply("mpg01", "MULTIPOLYGON()", true);
    test::apply("mpg02", "MULTIPOLYGON((()))", false);
    test::apply("mpg03", "MULTIPOLYGON(((0 0)),(()))", false);
    test::apply("mpg04", "MULTIPOLYGON(((0 0,1 0)))", false);

    // two disjoint polygons
    test::apply("mpg05",
                "MULTIPOLYGON(((0 0,1 0,1 1,0 1)),((2 2,3 2,3 3,2 3)))",
                true);

    // two disjoint polygons with multiple points
    test::apply("mpg06",
                "MULTIPOLYGON(((0 0,1 0,1 0,1 1,0 1)),((2 2,3 2,3 3,3 3,2 3)))",
                AllowDuplicates);

    // two polygons touch at a point
    test::apply("mpg07",
                "MULTIPOLYGON(((0 0,1 0,1 1,0 1)),((1 1,2 1,2 2,1 2)))",
                true);

    // two polygons share a segment at a point
    test::apply("mpg08",
                "MULTIPOLYGON(((0 0,1.5 0,1.5 1,0 1)),((1 1,2 1,2 2,1 2)))",
                false);

    // one polygon inside another and boundaries touching
    test::apply("mpg09",
                "MULTIPOLYGON(((0 0,10 0,10 10,0 10)),((0 0,9 1,9 2)))",
                false);
    test::apply("mpg09_2",
                "MULTIPOLYGON(((0 0,5 1,10 0,10 10,0 10)),((1 1,9 1,9 2)))",
                false);

    // one polygon inside another and boundaries not touching
    test::apply("mpg10",
                "MULTIPOLYGON(((0 0,10 0,10 10,0 10)),((1 1,9 1,9 2)))",
                false);

    // free space is disconnected
    test::apply("mpg11",
                "MULTIPOLYGON(((0 0,1 0,1 1,0 1)),((1 1,2 1,2 2,1 2)),((0 1,0 2,-1 2,-1 -1)),((1 2,1 3,0 3,0 2)))",
                true);

    // multi-polygon with a polygon inside the hole of another polygon
    test::apply("mpg12",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(1 1,1 99,99 99,99 1)),((2 2,98 2,98 98,2 98)))",
                true);
    test::apply("mpg13",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(1 1,1 99,99 99,99 1)),((1 1,98 2,98 98,2 98)))",
                true);

    // test case suggested by Barend Gehrels: take two valid polygons P1 and
    // P2 with holes H1 and H2, respectively, and consider P2 to be
    // fully inside H1; now invalidate the multi-polygon by
    // considering H2 as a hole of P1 and H1 as a hole of P2; this
    // should be invalid
    //
    // first the valid case:
    test::apply("mpg14",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(1 1,1 99,99 99,99 1)),((2 2,98 2,98 98,2 98),(3 3,3 97,97 97,97 3)))",
                true);
    // and the invalid case:
    test::apply("mpg15",
                "MULTIPOLYGON(((0 0,100 0,100 100,0 100),(3 3,3 97,97 97,97 3)),((2 2,98 2,98 98,2 98),(1 1,1 99,99 99,99 1)))",
                false);

    test::apply
        ("mpg16",
         "MULTIPOLYGON(((-1 4,8 -10,-10 10,7 -6,8 -2,\
                      -10 10,-10 1,-3 -4,4 1,-1 2,4 3,-8 10,-5 -9,-1 6,-5 0)),\
                      ((-10 -3,-8 1,2 -8,-2 6,-4 0,8 -5,-1 5,8 2)),\
                      ((-6 -10,1 10,4 -8,-7 -2,2 0,-4 3,-10 9)),\
                      ((10 -1,-2 8,-7 3,-6 8,-9 -7,7 -5)),\
                      ((7 7,-4 -4,9 -8,-10 -6)))",
         false);

    test::apply
        ("mpg17",
         "MULTIPOLYGON(((-1 4,8 -10,-10 10,7 -6,8 -2,\
                      -10 10,-10 1,-3 -4,4 1,-1 2,4 3,-8 10,-5 -9,-1 6,-5 0)),\
                      ((-10 -3,-8 1,2 -8,-2 6,-4 0,8 -5,-1 5,8 2)),\
                      ((-6 -10,-10 9,-4 3,2 0,-7 -2,4 -8,1 10)),\
                      ((10 -1,-2 8,-7 3,-6 8,-9 -7,7 -5)),\
                      ((7 7,-10 -6,9 -8,-4 -4)))",
         false);

    // test cases coming from buffer
    {
        std::string wkt = "MULTIPOLYGON(((1.1713032141645456 -0.9370425713316364,5.1713032141645456 4.0629574286683638,4.7808688094430307 4.3753049524455756,4.7808688094430307 4.3753049524455756,0.7808688094430304 -0.6246950475544243,0.7808688094430304 -0.6246950475544243,1.1713032141645456 -0.9370425713316364)))";

        OG open_mpgn = from_wkt<OG>(wkt);
        bg::reverse(open_mpgn);

        test::apply("mpg18", open_mpgn, false);
    }
    {
        std::string wkt = "MULTIPOLYGON(((5.2811206375710933 9.9800205994776228,5.2446420208654896 10.0415020265598844,5.1807360092909640 10.1691699739962242,5.1261005500004773 10.3010716408018013,5.0810140527710059 10.4365348863171388,5.0457062680576819 10.5748694208940446,5.0203571162381344 10.7153703234534277,5.0050957707794934 10.8573216336015328,5.0000000000000000 10.9999999999999964,5.0050957707794925 11.1426783663984619,5.0203571162381344 11.2846296765465670,5.0457062680576801 11.4251305791059501,5.0810140527710042 11.5634651136828559,5.1261005500004755 11.6989283591981934,5.1807360092909622 11.8308300260037704,5.2446420208654869 11.9584979734401102,5.3174929343376363 12.0812816349111927,5.3989175181512774 12.1985553330226910,5.4885008512914810 12.3097214678905669,5.5857864376269024 12.4142135623730923,5.6902785321094269 12.5114991487085145,5.8014446669773028 12.6010824818487190,5.9187183650888020 12.6825070656623602,6.0415020265598844 12.7553579791345104,6.1691699739962260 12.8192639907090360,6.3010716408018030 12.8738994499995236,6.4365348863171405 12.9189859472289950,6.5748694208940472 12.9542937319423199,6.7153703234534312 12.9796428837618656,6.8573216336015381 12.9949042292205075,7.0000000000000036 13.0000000000000000,7.1426783663984690 12.9949042292205075,7.2846296765465750 12.9796428837618656,7.4251305791059590 12.9542937319423181,7.5634651136828657 12.9189859472289932,7.6989283591982032 12.8738994499995201,7.8308300260037802 12.8192639907090324,7.9584979734401209 12.7553579791345069,8.0812816349112033 12.6825070656623566,8.1985553330227017 12.6010824818487137,8.3097214678905793 12.5114991487085092,8.4142135623731029 12.4142135623730869,8.5114991487085252 12.3097214678905598,8.6010824818487297 12.1985553330226821,8.6825070656623708 12.0812816349111838,8.7553579791345193 11.9584979734400996,8.8192639907090431 11.8308300260037580,8.8738994499995290 11.6989283591981810,8.9189859472290003 11.5634651136828417,8.9542937319423235 11.4251305791059359,8.9796428837618691 11.2846296765465510,8.9949042292205093 11.1426783663984441,9.0000000000000000 11.0000000000000000,8.9949042292205075 10.8573216336015346,8.9796428837618656 10.7153703234534294,8.9542937319423181 10.5748694208940464,8.9189859472289950 10.4365348863171405,8.8738994499995236 10.3010716408018030,8.8192639907090360 10.1691699739962278,8.7553579791345122 10.0415020265598862,8.7188787869375428 9.9800200826281831,8.8573216336015381 9.9949042292205075,9.0000000000000036 10.0000000000000000,9.1426783663984690 9.9949042292205075,9.2846296765465759 9.9796428837618656,9.4251305791059590 9.9542937319423181,9.5634651136828648 9.9189859472289932,9.6989283591982041 9.8738994499995201,9.8308300260037793 9.8192639907090324,9.9584979734401209 9.7553579791345069,10.0812816349112033 9.6825070656623566,10.1985553330227017 9.6010824818487137,10.3097214678905793 9.5114991487085092,10.4142135623731029 9.4142135623730869,10.5114991487085252 9.3097214678905598,10.6010824818487297 9.1985553330226821,10.6825070656623708 9.0812816349111838,10.7553579791345193 8.9584979734400996,10.8192639907090431 8.8308300260037580,10.8738994499995290 8.6989283591981810,10.9189859472290003 8.5634651136828417,10.9542937319423235 8.4251305791059359,10.9796428837618691 8.2846296765465510,10.9949042292205093 8.1426783663984441,11.0000000000000000 8.0000000000000000,10.9949042292205075 7.8573216336015355,10.9796428837618656 7.7153703234534294,10.9542937319423181 7.5748694208940464,10.9189859472289950 7.4365348863171405,10.8738994499995236 7.3010716408018030,10.8192639907090360 7.1691699739962269,10.7553579791345122 7.0415020265598862,10.6825070656623620 6.9187183650888047,10.6010824818487208 6.8014446669773063,10.5114991487085163 6.6902785321094296,10.4142135623730958 6.5857864376269051,10.3097214678905704 6.4885008512914837,10.1985553330226946 6.3989175181512792,10.0812816349111962 6.3174929343376380,9.9584979734401138 6.2446420208654887,9.8308300260037740 6.1807360092909640,9.6989283591981970 6.1261005500004764,9.5634651136828595 6.0810140527710050,9.4251305791059536 6.0457062680576810,9.2846296765465706 6.0203571162381344,9.1426783663984654 6.0050957707794925,9.0000000000000018 6.0000000000000000,8.8573216336015363 6.0050957707794925,8.7153703234534312 6.0203571162381344,8.5748694208940481 6.0457062680576810,8.4365348863171423 6.0810140527710050,8.3010716408018048 6.1261005500004764,8.1691699739962278 6.1807360092909622,8.0415020265598880 6.2446420208654878,7.9187183650888064 6.3174929343376363,7.8014446669773072 6.3989175181512783,7.6902785321094314 6.4885008512914819,7.5857864376269060 6.5857864376269033,7.4885008512914846 6.6902785321094278,7.3989175181512810 6.8014446669773045,7.3174929343376389 6.9187183650888029,7.2446420208654896 7.0415020265598844,7.1807360092909640 7.1691699739962251,7.1261005500004773 7.3010716408018013,7.0810140527710059 7.4365348863171379,7.0457062680576819 7.5748694208940437,7.0203571162381344 7.7153703234534268,7.0050957707794934 7.8573216336015328,7.0000000000000000 7.9999999999999973,7.0050957707794925 8.1426783663984619,7.0203571162381344 8.2846296765465670,7.0457062680576801 8.4251305791059501,7.0810140527710042 8.5634651136828559,7.1261005500004755 8.6989283591981934,7.1807360092909622 8.8308300260037704,7.2446420208654869 8.9584979734401102,7.2811219724467575 9.0199799990140797,7.1426783663984654 9.0050957707794925,7.0000000000000009 9.0000000000000000,6.8573216336015363 9.0050957707794925,6.7188786030357956 9.0199806804111571,6.7553579791345184 8.9584979734400996,6.8192639907090431 8.8308300260037580,6.8738994499995290 8.6989283591981810,6.9189859472290003 8.5634651136828417,6.9542937319423235 8.4251305791059359,6.9796428837618683 8.2846296765465510,6.9949042292205084 8.1426783663984441,7.0000000000000000 8.0000000000000000,6.9949042292205075 7.8573216336015355,6.9796428837618656 7.7153703234534294,6.9542937319423190 7.5748694208940464,6.9189859472289950 7.4365348863171405,6.8738994499995236 7.3010716408018030,6.8192639907090369 7.1691699739962269,6.7553579791345113 7.0415020265598862,6.6825070656623620 6.9187183650888047,6.6010824818487208 6.8014446669773063,6.5114991487085163 6.6902785321094296,6.4142135623730949 6.5857864376269051,6.3097214678905704 6.4885008512914837,6.1985553330226946 6.3989175181512792,6.0812816349111953 6.3174929343376380,5.9584979734401138 6.2446420208654887,5.8308300260037731 6.1807360092909640,5.6989283591981970 6.1261005500004764,5.5634651136828603 6.0810140527710050,5.4251305791059536 6.0457062680576810,5.2846296765465715 6.0203571162381344,5.1426783663984654 6.0050957707794925,5.0000000000000009 6.0000000000000000,4.8573216336015363 6.0050957707794925,4.7153703234534312 6.0203571162381344,4.5748694208940481 6.0457062680576810,4.4365348863171423 6.0810140527710050,4.3010716408018048 6.1261005500004764,4.1691699739962287 6.1807360092909622,4.0415020265598880 6.2446420208654878,3.9187183650888064 6.3174929343376363,3.8014446669773077 6.3989175181512783,3.6902785321094314 6.4885008512914819,3.5857864376269064 6.5857864376269033,3.4885008512914846 6.6902785321094278,3.3989175181512805 6.8014446669773045,3.3174929343376389 6.9187183650888029,3.2446420208654896 7.0415020265598844,3.1807360092909640 7.1691699739962251,3.1261005500004773 7.3010716408018013,3.0810140527710059 7.4365348863171379,3.0457062680576819 7.5748694208940437,3.0203571162381349 7.7153703234534268,3.0050957707794934 7.8573216336015328,3.0000000000000000 7.9999999999999973,3.0050957707794925 8.1426783663984619,3.0203571162381344 8.2846296765465670,3.0457062680576801 8.4251305791059501,3.0810140527710042 8.5634651136828559,3.1261005500004755 8.6989283591981934,3.1807360092909618 8.8308300260037704,3.2446420208654869 8.9584979734401102,3.3174929343376358 9.0812816349111927,3.3989175181512770 9.1985553330226910,3.4885008512914810 9.3097214678905669,3.5857864376269024 9.4142135623730923,3.6902785321094269 9.5114991487085145,3.8014446669773028 9.6010824818487190,3.9187183650888020 9.6825070656623602,4.0415020265598844 9.7553579791345104,4.1691699739962260 9.8192639907090360,4.3010716408018030 9.8738994499995236,4.4365348863171405 9.9189859472289950,4.5748694208940472 9.9542937319423199,4.7153703234534312 9.9796428837618656,4.8573216336015381 9.9949042292205075,5.0000000000000036 10.0000000000000000,5.1426783663984690 9.9949042292205075)))";

        OG open_mpgn = from_wkt<OG>(wkt);
        bg::reverse(open_mpgn);

        // polygon has a self-touching point
        test::apply("mpg19", open_mpgn, false);
    }
    {
        std::string wkt = "MULTIPOLYGON(((-1.1713032141645421 0.9370425713316406,-1.2278293047051545 0.8616467945203863,-1.2795097139219473 0.7828504914601357,-1.3261404828502752 0.7009646351604617,-1.3675375811487496 0.6163123916860891,-1.4035376333829217 0.5292278447680804,-1.4339985637934827 0.4400546773279756,-1.4588001570043776 0.3491448151183161,-1.4778445324579732 0.2568570378324778,-1.4910565307049013 0.1635555631651331,-1.4983840100240693 0.0696086094114048,-1.4997980522022116 -0.0246130577225216,-1.4952930766608652 -0.1187375883622537,-1.4848868624803642 -0.2123935159867641,-1.4686204782339323 -0.3052112234370423,-1.4465581199087858 -0.3968244016261590,-1.4187868575539013 -0.4868714951938814,-1.3854162916543107 -0.5749971294005020,-1.3465781205880585 -0.6608535126285795,-1.3024256208728704 -0.7441018089575634,-1.2531330422537639 -0.8244134753943718,-1.1988949200189114 -0.9014715584824893,-1.1399253072577331 -0.9749719451724563,-1.0764569300911435 -1.0446245630171400,-1.0087402692078766 -1.1101545249551616,-0.9370425713316382 -1.1713032141645441,-0.8616467945203836 -1.2278293047051563,-0.7828504914601331 -1.2795097139219491,-0.7009646351604588 -1.3261404828502767,-0.6163123916860862 -1.3675375811487509,-0.5292278447680773 -1.4035376333829228,-0.4400546773279725 -1.4339985637934838,-0.3491448151183129 -1.4588001570043785,-0.2568570378324746 -1.4778445324579736,-0.1635555631651299 -1.4910565307049017,-0.0696086094114016 -1.4983840100240695,0.0246130577225248 -1.4997980522022114,0.1187375883622569 -1.4952930766608650,0.2123935159867673 -1.4848868624803639,0.3052112234370455 -1.4686204782339316,0.3968244016261621 -1.4465581199087849,0.4868714951938845 -1.4187868575539002,0.5749971294005050 -1.3854162916543096,0.6608535126285824 -1.3465781205880569,0.7441018089575662 -1.3024256208728686,0.8244134753943745 -1.2531330422537621,0.9014715584824917 -1.1988949200189096,0.9749719451724583 -1.1399253072577313,1.0446245630171418 -1.0764569300911420,1.1101545249551634 -1.0087402692078746,1.1713032141645456 -0.9370425713316364,5.1713032141645456 4.0629574286683638,5.1713032141645439 4.0629574286683621,5.2278293047051561 4.1383532054796159,5.2795097139219491 4.2171495085398671,5.3261404828502767 4.2990353648395407,5.3675375811487509 4.3836876083139131,5.4035376333829230 4.4707721552319217,5.4339985637934838 4.5599453226720268,5.4588001570043785 4.6508551848816859,5.4778445324579739 4.7431429621675241,5.4910565307049017 4.8364444368348689,5.4983840100240693 4.9303913905885972,5.4997980522022116 5.0246130577225232,5.4952930766608645 5.1187375883622552,5.4848868624803639 5.2123935159867658,5.4686204782339320 5.3052112234370439,5.4465581199087856 5.3968244016261604,5.4187868575539007 5.4868714951938822,5.3854162916543107 5.5749971294005025,5.3465781205880578 5.6608535126285799,5.3024256208728699 5.7441018089575637,5.2531330422537632 5.8244134753943726,5.1988949200189110 5.9014715584824895,5.1399253072577329 5.9749719451724559,5.0764569300911440 6.0446245630171394,5.0087402692078768 6.1101545249551616,4.9370425713316379 6.1713032141645439,4.8616467945203841 6.2278293047051561,4.7828504914601337 6.2795097139219482,4.7009646351604593 6.3261404828502759,4.6163123916860869 6.3675375811487509,4.5292278447680783 6.4035376333829230,4.4400546773279732 6.4339985637934838,4.3491448151183141 6.4588001570043785,4.2568570378324750 6.4778445324579739,4.1635555631651311 6.4910565307049017,4.0696086094114028 6.4983840100240693,3.9753869422774759 6.4997980522022116,3.8812624116377439 6.4952930766608645,3.7876064840132333 6.4848868624803639,3.6947887765629552 6.4686204782339320,3.6031755983738387 6.4465581199087847,3.5131285048061165 6.4187868575539007,3.4250028705994957 6.3854162916543098,3.3391464873714183 6.3465781205880578,3.2558981910424345 6.3024256208728691,3.1755865246056261 6.2531330422537623,3.0985284415175087 6.1988949200189101,3.0250280548275423 6.1399253072577320,2.9553754369828584 6.0764569300911422,2.8898454750448366 6.0087402692078751,2.8286967858354544 5.9370425713316362,-1.1713032141645456 0.9370425713316364,-1.1713032141645421 0.9370425713316406)))";

        OG open_mpgn = from_wkt<OG>(wkt);
        bg::reverse(open_mpgn);

        // polygon contains a spike
        test::apply("mpg20", open_mpgn, false);
    }

    // Interior ring touches exterior ring, which at that same point touches another exterior ring in (1 2)
    test::apply
        ("mpg21",
        "MULTIPOLYGON(((2 3,1 2,1 0,2 0,2 1,4 3),(2 2,1.5 1.5,1 2)),((0 3,0 2,1 2)))",
         true);

    // Two interior rings touch each other in (10 5)
    test::apply
        ("mpg22",
        "MULTIPOLYGON(((10 5,5 10,0 5,5 0),(10 5,5 4,4 6)),((10 5,10 0,20 0,20 10,10 10),(10 5,18 8,15 3)))",
         true);

    // Two polygons, one inside interior of other one, touching all at same point (0,0)
    test::apply
        ("mpg23",
        "MULTIPOLYGON(((0 0,10 0,10 10,0 10),(0 0,1 9,9 9,9 1)),((0 0,8 2,8 8,2 8)))",
         true);

    test::apply
        ("ticket_12503",
        "MULTIPOLYGON(((15 20,12 23,15 17,15 20)),((36 25,35 25,34 24,34 23,35 23,36 25)),((15 15,10 13,11 23,12 23,12 25,10 25,7 24,8 6,15 15)),((12 29,11 30,12 30,11 31,13 31,13 34,6 38,6 32,8 31,12 27,12 29)),((9 26,6 31,7 26,7 24,9 26)),((15 48,15 45,18 44,15 48)),((38 39,18 44,26 34,38 39)),((15 45,13 34,15 33,15 45)),((17 32,15 33,15 32,17 32)),((21 31,16 38,18 32,17 32,19 30,21 31)),((15 32,13 31,13 30,15 29,15 32)),((17 29,15 29,15 28,17 29)),((15 28,13 30,12 29,14 27,15 28)),((26 27,28 30,31 27,26 34,21 31,22 29,19 30,18 30,17 29,19 29,19 28,23 28,24 27,25 27,24 26,25 24,30 24,26 27)),((17 26,15 28,15 26,17 26)),((32 26,34 27,31 27,27 27,32 26)),((19 26,17 26,19 25,19 26)),((35 23,33 18,41 15,35 23)),((24 26,24 27,19 26,20 25,23 24,24 26)),((32 13,49 1,48 4,46 5,33 15,32 13)),((33 25,32 26,32 25,31 24,32 23,33 25)),((42 15,43 22,44 22,44 23,43 23,35 23,42 15)),((44 42,38 39,40 39,39 34,34 27,33 25,35 25,38 31,36 25,43 23,44 42)),((48 46,44 23,48 22,48 46)),((15 3,23 2,18 11,15 3)),((30 19,28 20,27 21,25 24,23 24,22 23,26 20,29 17,30 19)),((24 19,21 21,21 20,22 19,24 19)),((31 24,30 24,27 21,31 22,30 19,31 19,34 23,31 23,31 24)),((21 20,20 21,21 18,21 20)),((14 26,12 26,12 25,15 25,15 20,17 17,20 21,21 21,22 23,20 24,19 25,16 25,15 26,14 27,14 26),(17 24,20 22,20 21,17 24)),((23 18,22 19,22 17,23 18)),((28 13,31 10,32 13,30 15,28 13)),((18 17,17 17,16 16,18 17)),((16 16,15 17,15 15,16 16)),((30 17,29 17,29 16,30 15,30 17)),((33 18,31 19,30 17,33 15,33 18)),((42 13,47 7,48 4,48 22,44 22,43 14,42 13)),((42 15,41 15,42 14,43 14,42 15)),((24 2,49 1,27 11,23 13,25 6,27 10,24 2)),((29 16,24 19,23 18,28 13,29 16)),((17 13,16 15,15 15,15 11,17 13)),((20 14,23 13,22 17,20 15,21 17,21 18,18 17,19 15,17 13,18 11,20 14)),((5 3,15 3,15 11,8 5,8 6,5 3)))",
         true);

    // MySQL report 12.06.2015
    {
        std::string wkt = "MULTIPOLYGON("
            "((-40.314872143936725 -85.6567579487603,-53.1473643859603 -98.48925019078388,-41.29168745244485 -90.56754012573627,-40.314872143936725 -85.6567579487603)),"
            "((-186.91433298215597 -88.80210078879976,-192.54783347494038 -75.53420159905284,-192.7944062150986 -78.03769664281869,-186.91433298215597 -88.80210078879976)),"
            "((170.89089207158912 -44.35600339378721,191.8949969913326 -31.3460752560506,169.32805525181837 -43.07341636227523,170.89089207158912 -44.35600339378721)),"
            "((-2.6035109435630504 -13.058121512403435,26.839412016794036 43.97610638055074,26.974733141577826 43.72289807427111,26.97470368529088 43.722907009738066,-2.6035109435630504 -13.058121512403435))"
            ")";

        std::string msg;
        CG mpoly = from_wkt<CG>(wkt);
        BOOST_CHECK_MESSAGE(! bg::is_valid(mpoly, msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[0], msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[1], msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[2], msg), msg);
        BOOST_CHECK_MESSAGE(! bg::is_valid(mpoly[3], msg), msg);

        // The last Polygon has wrong orientation, so correct it
        bg::reverse(mpoly[3]);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly, msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[0], msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[1], msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[2], msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[3], msg), msg);
    }

    // MySQL report 30.06.2015
#ifdef BOOST_GEOMETRY_TEST_FAILURES
    {
        std::string wkt = "MULTIPOLYGON(((153.60036248974487 -86.4072234353084,134.8924761503673 -92.0821987136617,151.87887755950274 -92.0821987136617,153.60036248974487 -86.4072234353084)),((-162.3619975827558 -20.37135271519032,-170.42498886764488 -34.35970444494861,-168.21072335866987 -37.67358696575207,-162.3619975827558 -20.37135271519032)),((25.982352329146433 -1.793573272167862,25.81900562736861 -2.0992322130216032,24.216310496207715 -2.737558757030656,25.9822948153016 -1.7936204725604972,25.982352329146433 -1.793573272167862)))";

        std::string msg;
        CG mpoly = from_wkt<CG>(wkt);
        bg::correct(mpoly);

        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly, msg), msg);
        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly[2], msg), msg);
    }
    {
        std::string wkt = "MULTIPOLYGON(("
            "(176.37937452790632 33.81070321484654,165.31874654406195 30.429623478264823,164.22653456196667 40.57205841516082,169.96584109309163 43.760662373477935,169.96717084197783 43.76157019827226,169.96831223949295 43.76270579928938,169.96922682269394 43.76403090891733,169.96988377185227 43.76550087346144,169.97026094902066 43.767066157889666,169.97034564404004 43.768674015069664,169.97013500284808 43.77027026324768,169.96963612365553 43.77180111187144,169.9688658177501 43.773214974230235,169.9678500429876 43.77446420582984,169.96662302906125 43.775506709921984,169.96522612402566 43.77630735608547,169.96370640094523 43.77683916405498,169.96211507162087 43.777084212905194,169.96050576084875 43.777034244952155,163.96123663641703 43.03565247280777,161.69252016076334 64.10327452950632,161.69220086043657 64.10485513588213,161.69159759286657 64.10635057476519,161.6907307489143 64.1077102992874,161.68962962850455 64.10888834981994,161.68833145026844 64.10984490744038,161.6868800935278 64.11054763983988,161.68532461514377 64.11097279417753,161.68371759136113 64.11110599994278,161.68211334069463 64.1109427546887,161.68056608792585 64.11048857621772,161.67912813126807 64.10975881607614,127.83620579794024 42.81253476817878,91.03182023020634 47.701488147167275,136.39039242955064 83.14688773021612,136.39163693720113 83.14807413530517,136.39262607583518 83.14948053535137,136.39332183340986 83.15105288318968,136.39369747235173 83.15273075437446,136.3937385570669 83.15444966925705,136.39344350869288 83.15614357090246,136.39282366577314 83.15774736362043,136.39190284852347 83.15919941455623,136.39071644343446 83.1604439222067,136.38931004338824 83.16143306084076,136.38773769554993 83.16212881841544,136.38605982436513 83.16250445735731,136.38434090948257 83.16254554207251,136.38264700783714 83.16225049369845,136.38104321511918 83.1616306507787,136.37959116418338 83.16070983352907,91.00745759860087 47.70471273777429,-11.691678619246499 61.34689554994591,-33.79755445921219 85.09421525362518,-33.7987816906185 85.09530774427819,-33.80019114476049 85.09615218005024,-33.80173336063651 85.09671892773947,-33.80335421832898 85.0969880988603,-33.8049968381973 85.09695024757627,-33.80660357691587 85.09660670217643,-33.808118050311506 85.09596951846243,-33.80948711201435 85.09506105668265,-33.81066271848674 85.0939131968595,-33.81160361498191 85.09256622004571,-33.812276783268395 85.09106739476917,-33.81265860031601 85.08946931827111,-33.812735667282574 85.08782807074738,-32.830674277011205 64.15491927762633,-166.9120300852194 81.96578933819151,-166.91372994506622 81.96584876974394,-166.9154093128506 81.96557899307163,-166.91700503326308 81.96499015355748,-166.91845709668723 81.96410439545254,-166.91971089595725 81.96295502910593,-166.92071927994778 81.96158527827609,-166.92144432676722 81.96004665463211,-166.9218587698712 81.95839702057518,-166.92194702346433 81.95669841323047,-166.9217057686293 81.95501471144192,-135.70224400417752 -45.9322191166313,-138.57126185553517 -49.74180297177792,-161.44988958707904 -54.37476978072864,-161.45143808151082 -54.37523543008462,-161.45287478035848 -54.37597742789392,-161.45415085337137 -54.37697055527431,-161.4552229296195 -54.37818105799885,-161.45605457157663 -54.3795677937282,-161.45661751355206 -54.381083630348755,-161.4568926223796 -54.382677047890056,-161.4568705477121 -54.384293889575865,-161.45655203981923 -54.38587920249453,-161.4559479240874 -54.38737910532821,-161.45507873308873 -54.388742619660995,-161.45397400872366 -54.38992340262376,-161.4526712981565 -54.39088132198683,-161.45121487766946 -54.391583820166765,-161.44965424780858 -54.39200702078731,-161.44804245096856 -54.392136540185234,-141.89488089543744 -54.15502243957519,-153.27347396169714 -69.26392265746459,-153.2743589702193 -69.26537072588309,-153.2749485381606 -69.26696212522691,-153.2752205928412 -69.26863727551263,-153.27516494887718 -69.27033346122717,-153.27478368950827 -69.27198717931759,-153.27409108860425 -69.27353651666519,-153.27311307626903 -69.27492346803459,-153.2718862680504 -69.27609610771653,-153.27045659410012 -69.27701053356093,-153.268877579607 -69.27763251061793,-153.2672083408813 -69.27793875285136,-153.26551137211467 -69.27791779493838,-153.26385020567662 -69.27757042151681,-107.17294666067983 -54.86628398298008,-106.1185414985015 -57.820838687964276,-161.49299710582102 -75.80397143603332,-161.49452500826962 -75.80463385281091,-161.49590016714416 -75.80557311845463,-161.4970730084222 -75.80675537274882,-161.49800125156384 -75.8081379958174,-161.49865143371088 -75.80967114455622,-161.4990001160121 -75.81129954946056,-161.4990347285859 -75.81296450707245,-161.49875402366052 -75.81460599622092,-161.49816812055553 -75.81616484176531,-161.49729814088408 -75.81758484783957,-161.4961754471254 -75.81881482369457,-161.49484051201802 -75.81981042910776,-161.49334145953082 -75.82053577283341,-161.49173233000997 -75.82096470647024,-161.49007113204223 -75.82108176710256,-100.22898834625123 -74.30605444452195,-100.22734583448315 -74.30585777708335,-100.22576946346221 -74.30535620216963,-100.22431530761126 -74.30456756172502,-100.22303509393429 -74.30351990914377,-100.22197436199151 -74.30225051135928,-100.22117084397333 -74.30080452319054,-100.22065312249669 -74.299233381102,-100.22043961386883 -74.29759297351377,-100.2205379129852 -74.2959416527474,-100.220944523165 -74.29433815932634,-106.0959583598649 -57.83192845601232,-92.94623645293606 -53.56150642974892,0.5168968571681063 -52.42819495175221,42.552398940491486 -80.15392826171708,42.553823957605765 -80.15468988561345,42.55536450400553 -80.15517718585528,42.55696829838054 -80.15537362500162,42.55858091298275 -80.15527253652432,42.56014762073331 -80.15487735104905,42.56161525248782 -80.15420147993068,42.56293400143029 -80.15326786011391,42.56405911336014 -80.15210817572546,42.564952405509295 -80.15076178281409,42.56558356234557 -80.14927437372997,42.56593116438665 -80.14769642646958,42.56598341511011 -80.14608149161101,42.565738541290365 -80.14448437497535,42.5652048531764 -80.1429592776891,42.56440046246796 -80.14155795676808,22.856549024752262 -52.14358082479128,22.85544171929926 -52.14229333196901,22.854107064023683 -52.14124335176489,22.852595089554377 -52.14047024353326,22.850962473473096 -52.14000298785936,22.849270415709668 -52.13985910019877,0.5220842171561202 -52.41060241052812,-8.41778896463083 -46.51405003506674,2.082503817262605 -45.5265842190059,82.16974873985578 -55.61946094607285,82.17129108418781 -55.61951844683097,82.17281962817947 -55.61930470483393,82.17428704023979 -55.6188263386343,82.17564788173712 -55.61809816091227,82.17686001401307 -55.61714271979877,82.17788590321227 -55.61598960066932,87.42894995498656 -48.54817950299798,90.86025690382013 -48.842320647346625,90.86187779422266 -48.842309257891365,90.86346890919046 -48.841999792907956,90.8649759064788 -48.84140282172809,90.86634731676145 -48.84053873304434,90.86753630149138 -48.839437038563695,90.86850225260417 -48.838135365077605,90.86921217942799 -48.836678169372995,90.86964183543259 -48.83511521987464,90.8697765463347 -48.83349989687612,90.8696117112771 -48.831887369412634,90.86915295996427 -48.83033271104191,90.86841596038786 -48.82888901888599,90.8674258837091 -48.827605600175325,88.83971158165615 -46.64932716144001,92.40269805720794 -41.853630224309256,126.8456757419591 -63.47440298320586,126.84713519537095 -63.47513926788542,126.84870592800237 -63.475591947011644,126.85033338098 -63.4757452969263,126.85196102526822 -63.47559399107129,126.8535323251863 -63.47514328500498,126.85499270215635 -63.47440883385204,126.85629143047208 -63.47341614852797,126.85738339923923 -63.472199709625954,126.85823067928733 -63.47080176974498,126.85880384062692 -63.46927088585993,126.8590829746903 -63.46766023271123,126.85905838584898 -63.466025755798086,126.85873092818818 -63.46442422813017,126.85811197584037 -63.46291127823613,126.85722302790863 -63.461539457925234,126.85609496170247 -63.46035641691782,97.38483049679773 -37.827161184361024,99.87187230030216 -36.330501626203954,171.77091226660818 -29.569125763297997,171.77242885150693 -29.568846999536735,171.7738731802143 -29.568306949932698,171.77520061136264 -29.567522306359965,171.7763701166585 -29.566517320612498,171.777345548987 -29.565323054828713,171.77809675964784 -29.56397642142174,171.77860053019282 -29.562519042189095,171.77884129006245 -29.56099596186458,171.77881159784133 -29.559454255873884,165.32871350322802 30.33706888582541,176.38762884933539 33.794883447557986,176.38920864287232 33.79555326659214,176.3906277904606 33.79651785214075,176.39183197552205 33.79774028557272,176.39277510897307 33.799173779364644,176.39342109324193 33.80076346785056,176.3937452038706 33.80244850715521,176.39373503582075 33.80416440393784,176.3933909782652 33.80584548381587,176.39272619969265 33.80742740499197,176.39176614389606 33.80884962087758,176.39054755613435 33.810057697458305,176.38911707674143 33.811005396706115,176.3875294560099 33.81165644629765,176.38584545867357 33.81198592790426,176.38412953819284 33.81198123091831,176.3824473698573 33.81164253511265,176.38239992262746 33.811606862577534,176.38239393445264 33.81162601425782,176.37937452790632 33.81070321484654),"
            "(31.940896057402718 -11.516674682385004,31.735090184010808 -11.433768531594366,34.28637780994461 -10.636047228893936,31.940896057402718 -11.516674682385004),"
            "(30.28283959927952 -10.848748755534567,-3.4172969820295727 2.7269017301769836,-13.569154489871252 9.18584944133385,-12.274858999116972 9.628378040430327,21.92730276845213 2.385649455001494,38.84218041215776 -8.23228345814529,30.28283959927952 -10.848748755534567),"
            "(-3.506381078742539 2.7627880702187406,-16.750651901766318 8.098065902036172,-13.590409501346059 9.178582205591375,-3.506381078742539 2.7627880702187406),"
            "(31.709407405831065 -11.423422566994818,30.308715440626955 -10.859172493055892,38.861820878026606 -8.244612297573639,39.53325186684806 -8.666087271928937,34.579078645088416 -10.526151567651766,31.709407405831065 -11.423422566994818),"
            "(31.96495469130859 -11.526366382128769,34.5846215239078 -10.542808015164859,39.883544951634676 -8.885975497800693,42.787058225913796 -10.708574699971027,37.770908594399415 -13.865217464656103,31.96495469130859 -11.526366382128769),"
            "(48.77877229519879 -5.194821637047497,38.86437999888012 -8.225507106217272,21.97711163867511 2.3751018026608763,48.385785409894076 -3.2172657509225715,49.85165664674822 -4.7920077720156655,48.77877229519879 -5.194821637047497),"
            "(52.035281286546805 -3.9721613339210506,48.39482899889053 -3.201249949645277,24.68775090776513 22.266150556743217,27.256969910296846 23.144584341056124,56.359252044298216 -2.1676440520591687,55.64595593834196 -2.6165186152963713,52.035281286546805 -3.9721613339210506),"
            "(21.0523618078348 -15.604823171775074,20.044774220126357 -15.088996753249816,23.066991360577624 -14.144024584675869,23.763240995171685 -14.58700084866993,21.0523618078348 -15.604823171775074),"
            "(49.86941225561311 -4.785336300805852,48.41562983381898 -3.2235856816187862,52.00474175020747 -3.9836250990190427,49.86941225561311 -4.785336300805852),"
            "(56.52330868895917 -2.287118069864981,56.3885724075448 -2.1699191303695926,106.52503218338835 29.380766897799347,127.67871416131852 24.428460971650335,56.52330868895917 -2.287118069864981),"
            "(23.783784057718577 -14.579279673235199,23.088894501216046 -14.137168734379976,31.708657690867156 -11.442032284077861,31.91659602271336 -11.525797467311321,23.783784057718577 -14.579279673235199),"
            "(39.55390791690693 -8.65834200124904,38.88402046337834 -8.237835944785246,48.51602741760274 -5.293470218105426,39.55390791690693 -8.65834200124904),(56.50463609360271 -2.2941260112243,55.727564043439656 -2.5858892138401472,56.373071028355454 -2.1796740901332754,56.50463609360271 -2.2941260112243),(21.03014957321752 -15.613158799416311,10.569260752810493 -19.54075189827794,-2.9464639789869125 -22.277697151591028,20.02087042017853 -15.096466397529603,21.03014957321752 -15.613158799416311),"
            "(52.47436796255111 -4.065143439019163,52.0671665340097 -3.978913434571549,55.572196226218196 -2.6629353253941694,54.16542157313718 -3.5482133364501145,52.47436796255111 -4.065143439019163),"
            "(18.686422690099135 -14.3935995357972,-39.497151641780675 15.393047282556093,-15.306368432221959 10.270345844048258,-13.609365014952653 9.190640067582622,-16.778642869828406 8.10704200262427,-16.78013527405983 8.106370378455987,-16.78147692731812 8.10543319333665,-16.78262111907359 8.104263075974965,-16.78352801354702 8.10290076476696,-16.784166036618227 8.101393689462272,-16.78451297509954 8.099794319866595,-16.784556750102364 8.098158339071315,-16.784295837571545 8.09654270481052,-16.783739321346324 8.095003666440478,-16.782906576900587 8.093594806581768,-16.78182659677314 8.092365175605535,-16.780536981173654 8.091357583912956,-16.779082628906927 8.090607111463319,-3.4253378182903305 2.711229144049957,21.910296697427533 -13.408098481443034,18.686422690099135 -14.3935995357972),"
            "(49.92455499029387 -4.844573589537546,49.88232772450034 -4.7992107915072495,52.03662700043225 -3.990377200254404,52.43970998904547 -4.075735073446003,49.92455499029387 -4.844573589537546),"
            "(56.98298842344176 -2.6869329990186017,56.53834480149721 -2.3001960061283953,127.709423817281 24.421271512588014,132.92279324817048 23.20076594437462,122.22155101611108 17.25548653666155,56.98298842344176 -2.6869329990186017),"
            "(21.93236247547395 -13.401346106262679,-3.336253671995941 2.6753427840349797,30.256146438987056 -10.856907501734277,21.93236247547395 -13.401346106262679),"
            "(56.963208077106145 -2.6929774535282007,54.22943361944536 -3.5286570687708547,55.65380423765296 -2.6323059828796573,56.51967220394539 -2.3072039455782374,56.963208077106145 -2.6929774535282007),"
            "(49.905901657492095 -4.850280677454471,49.047142136266274 -5.112798684502387,49.86457211628575 -4.805882263415661,49.905901657492095 -4.850280677454471),"
            "(18.66231459031522 -14.40096459040753,-15.242813984693981 -24.76524114901455,-41.871025640976086 15.895732417477603,-39.56283173369286 15.406955864944898,18.66231459031522 -14.40096459040753),"
            "(54.16010295741377 -4.422118945281966,57.68433962274301 -3.3201949186482835,60.493211425450355 -5.763234012884148,54.16010295741377 -4.422118945281966),"
            "(50.57482090609503 -5.543124178834603,51.410616916152094 -5.2818015712411075,50.73017482548689 -5.7100012259218005,50.57482090609503 -5.543124178834603),"
            "(57.7040033134621 -3.3140490007641716,120.5880728282652 16.347976841903872,68.42035360394682 -12.634786014512635,63.952222521971656 -8.748552923366702,69.34888739490755 -7.655727627757562,69.35052190979452 -7.65522664740267,69.35202819378684 -7.654418112452161,69.35334902179036 -7.65333273988165,69.35443421435205 -7.652011763978858,69.3552425440229 -7.650505369815648,69.35574330162603 -7.648870786671658,69.35591746292623 -7.647170113842091,69.35575841137785 -7.645467961429212,69.35527218949336 -7.643828995745994,69.35447726928288 -7.642315482584282,69.35340385048569 -7.640984921680917,69.35209271325485 -7.639887862250621,69.35059366888214 -7.639065982575512,69.348963667422 -7.638550506609389,60.528528122163166 -5.770712765907017,57.7040033134621 -3.3140490007641716),"
            "(39.90557971777995 -8.879095683641047,50.55626153776256 -5.548932208433719,50.71506487635533 -5.719509860218734,42.803538516251564 -10.698203715094458,39.90557971777995 -8.879095683641047),"
            "(54.125906762225966 -4.432808367053374,60.520474886872016 -5.78693828698986,63.91384251004383 -8.738419975604259,55.19312990789962 -10.504359876990208,50.74233508921979 -5.723075090472591,51.47603824555935 -5.261358446995118,54.125906762225966 -4.432808367053374),"
            "(-39.55690832500143 15.42363240349549,-42.57305175099324 16.967718229775315,-54.529804271814804 35.22554209214185,-30.764086064128595 20.105022802970012,-30.710857834915476 18.970448449090075,-30.710636768905257 18.968861050497278,-30.71013054093483 18.96734038034394,-30.709356054150412 18.965937214390088,-30.70833916896296 18.964698404885425,-30.707113839559156 18.963665316154746,-30.70572098015804 18.962872443427244,-30.70420709886957 18.962346261027584,-30.70262274477097 18.962104338388023,-30.70102082005363 18.962154753398345,-29.249644048343924 19.141511936326765,-15.348629077210006 10.297225959678553,-39.55690832500143 15.42363240349549),"
            "(21.884178079339662 2.412712542801014,-12.242463797063095 9.639454184544832,2.3975468904970114 14.644975309050857,21.884178079339662 2.412712542801014),"
            "(-15.299650033147174 10.286854033424495,-29.22228439323044 19.144892967884317,-8.792850447899582 21.669506695125158,2.3784269745719975 14.656976960749958,-12.275968322130971 9.646537638677753,-15.299650033147174 10.286854033424495),"
            "(21.933986962464473 2.402164887728272,2.4189169499386907 14.652281880490598,24.669574614144935 22.25993595597394,48.36498457496562 -3.1949300189490657,21.933986962464473 2.402164887728272),"
            "(-39.62258841691361 15.437540985884295,-41.88466302451252 15.91655654561091,-42.55362818185854 16.938058662646,-39.62258841691361 15.437540985884295),"
            "(54.12505697116883 -4.414697518592483,53.149660304654546 -4.208154364528738,54.17278878474252 -3.5643034507215816,56.978802584973195 -2.7065410644301977,57.66879568091532 -3.3066752884918813,54.12505697116883 -4.414697518592483),"
            "(52.5097940270921 -4.072645352049953,54.108776779744645 -3.583859692404417,53.12503062771869 -4.202927454922124,52.5097940270921 -4.072645352049953),"
            "(43.28546691270277 -26.986883767640755,42.97679823939298 -26.79048854796605,56.466941170357856 -19.295797633524103,64.08900802970163 -24.08037950217483,45.2669261914462 -28.001268064806617,43.28546691270277 -26.986883767640755),"
            "(43.11812854048746 -26.90121622441705,42.940870464815944 -26.810470368231712,42.959363702902145 -26.800187467000804,43.11812854048746 -26.90121622441705),"
            "(20.022491497341747 -15.077589291279075,18.708847193281812 -14.40507958111042,21.92976181239232 -13.420482809530107,23.04762685262098 -14.13170426598196,20.022491497341747 -15.077589291279075),"
            "(21.073463079609297 -15.61562579933674,23.781749745311217 -14.598776706897553,26.48393818926334 -16.317995334865266,23.590120811873746 -16.904008626322792,21.073463079609297 -15.61562579933674),"
            "(9.350288473315587 38.74248379268954,-6.585012056625516 55.86103593692023,70.2680521039453 37.868910337863355,40.90146758060325 27.828271548502038,24.262734733742946 25.772111264832496,9.350288473315587 38.74248379268954),"
            "(73.09089007444824 -29.731095597924465,85.69919539743492 -37.64566996857491,80.88024981899486 -38.09884440280534,73.09089007444824 -29.731095597924465),"
            "(73.04450471090416 -29.681265947851927,69.3379197794863 -25.699470542214453,79.58321142449617 -22.367138852695543,96.03221445150848 -36.67396648552466,85.72790431968778 -37.64297969856965,73.04450471090416 -29.681265947851927),"
            "(9.46387079270287 38.62046756176968,24.239338321979417 25.769211949937343,21.716979498625832 25.45751975956489,9.46387079270287 38.62046756176968),"
            "(50.56156943063165 -5.528888731813538,49.937839204058804 -4.858844205801361,52.475136061040715 -4.083236988055326,53.10370484197959 -4.216347691874761,51.46866317625423 -5.24527327754466,50.56156943063165 -5.528888731813538),"
            "(9.224408992818482 38.851964794507175,-11.711265479809555 57.06114671556529,-6.615650355906411 55.86820869061986,9.224408992818482 38.851964794507175),"
            "(60.55579159384646 -5.794417042185758,69.30386052897589 -7.646930246489353,63.93552683381575 -8.734031540594103,60.55579159384646 -5.794417042185758),"
            "(57.688459366105995 -3.3005293657993064,56.99858293272367 -2.7004966111511806,122.14822367585627 17.214748146233596,120.66377698176358 16.39003571587255,57.688459366105995 -3.3005293657993064),"
            "(73.03324406984439 -29.694914662490135,80.85821729038376 -38.100921377000205,63.342896321792594 -39.748050882541506,50.75959180753584 -31.74216343806383,67.57943285376581 -26.271427096622276,73.03324406984439 -29.694914662490135),"
            "(39.86400285729009 -8.873708408777603,34.87732280230907 -10.432912187499966,39.551916707235804 -8.677803685461726,39.86400285729009 -8.873708408777603),"
            "(43.44410013402389 -27.087801782144677,45.239552804641725 -28.00697030328185,44.975318735147916 -28.06201375436441,43.44410013402389 -27.087801782144677),"
            "(-15.257389373944136 10.259973914784002,-12.30836353977779 9.635461489231913,-13.588110003575826 9.197907303291592,-15.257389373944136 10.259973914784002),"
            "(23.069529991534154 -14.124848414588413,21.951827586412705 -13.413730431788267,30.282022284527198 -10.86733124094458,31.682974917738175 -11.431686321512942,23.069529991534154 -14.124848414588413),"
            "(23.80229280523764 -14.591055529795597,31.94065465827722 -11.53548916772298,37.76851162339057 -13.883163606614682,37.770097848105024 -13.883634342409975,37.771744254525196 -13.883798801928782,37.77339225172808 -13.883651132524463,37.774983192179256 -13.883196589331225,37.77646045883184 -13.882451348248843,42.80353701408153 -10.718930933297813,45.567034652447205 -12.45365217013186,26.508719800762194 -16.312970859703256,23.80229280523764 -14.591055529795597),"
            "(43.276761579565346 -27.002134145623014,44.95069160862587 -28.067143910504445,38.11630436469161 -29.49083715589638,42.92240688088259 -26.820725087652352,43.276761579565346 -27.002134145623014),"
            "(126.75959213687518 -63.39967008039708,92.41319098322936 -41.83950698990248,93.78447601919933 -39.99379341951623,97.3690323884615 -37.83666821790139,126.75959213687518 -63.39967008039708),"
            "(72.98685869023261 -29.645084995156807,67.60116781362723 -26.264357693619786,69.31952721664192 -25.70545281408542,72.98685869023261 -29.645084995156807),"
            "(39.886037618944634 -8.866828591799006,39.57257275758669 -8.67005841496514,48.78439784949424 -5.211447043733159,49.91918587276122 -4.864551295334174,50.543010064128545 -5.534696763377852,39.886037618944634 -8.866828591799006),"
            "(9.337991159717035 38.729948727398885,21.695490271536684 25.454859141680437,-8.788846562680476 21.687676810037537,-31.489294946971892 35.93737116944357,-32.81216829744508 64.1347806843964,-11.700243714994912 61.33035113568702,-6.637054435703233 55.89123580798307,-11.741701242558069 57.08628824654891,-11.743347393206076 57.086512042263855,-11.745006213819227 57.08642142961937,-11.746618193091077 57.08601965940509,-11.748125500179835 57.08532114539797,-11.749474059428762 57.084350947258365,-11.75061549036676 57.0831438714978,-11.751508843390358 57.08174322277196,-11.752122068858302 57.08019925029701,-11.752433166894022 57.07856734512509,-11.752430976646009 57.076906052953085,-11.752115576690887 57.07527497375627,-11.751498282214417 57.07373262359911,-11.750601239071592 57.07233433533203,-11.749456629289176 57.07113027348812,9.337991159717035 38.729948727398885),"
            "(19.998587701357646 -15.085058937588059,-3.1132193245213102 -22.311465244718235,-15.089019545185295 -24.73657453178888,18.684739096779687 -14.412444637400831,19.998587701357646 -15.085058937588059),"
            "(21.051250846321395 -15.623961427658536,23.562530114275848 -16.909590792795676,10.677531366771717 -19.518827010871995,21.051250846321395 -15.623961427658536),"
            "(54.09086078489642 -4.42538694225184,51.53408460058219 -5.224830093565444,53.128334513377965 -4.221574604966094,54.09086078489642 -4.42538694225184),"
            "(173.75757156169772 32.99091541934996,165.3267971738516 30.354864191440612,165.32065635283269 30.411888723916427,173.75757156169772 32.99091541934996),"
            "(10.465747395220248 -19.57961137948609,-12.894911802992752 -28.350460831776864,-14.505090251111184 -25.891736822767676,-3.10897552967657 -22.328503814897203,10.465747395220248 -19.57961137948609),"
            "(-27.610965806972345 -33.8756481721416,-38.00614396293315 -37.7785591589894,-43.02265457370873 -37.34856959661485,-29.167463128681774 -32.849015266607424,-27.610965806972345 -33.8756481721416),"
            "(-38.04432250117674 -37.79289279162382,-43.34682819058996 -39.783739126387225,-48.515464191322366 -39.13238742407438,-43.065536969219835 -37.36249587240486,-38.04432250117674 -37.79289279162382),"
            "(-43.38210310004821 -39.796974297919405,-78.87353889947534 -53.12237586126752,-78.87504724430607 -53.12311387651708,-78.87638729989119 -53.124125816073466,-78.87750994786981 -53.125374588331766,-78.87837403873353 -53.126814420827664,-78.87894790011936 -53.128392537980105,-78.87921049772773 -53.130051095526014,-78.87915220631305 -53.1317293007423,-78.87877516248726 -53.13336564074071,-78.87809318640446 -53.13490013715957,-78.87713127519739 -53.136276544608954,-78.87592468673323 -53.13744441228774,-78.87451764727265 -53.13836093320586,-78.87296173040167 -53.138992513230576,-78.87131396665488 -53.13931600244487,-78.86963475311974 -53.1393195436839,-8.445695735047487 -46.516657330884335,0.49077562272833575 -52.4109659347568,-92.89011328794561 -53.543280146805785,-48.55468477896229 -39.14512450915579,-43.38210310004821 -39.796974297919405),"
            "(-27.590643470202913 -33.86803834336524,-29.146094667189693 -32.84209543970167,-19.625029335662298 -29.750071937783378,-19.62345393418427 -29.749383122488545,-19.62204318480157 -29.748400197004802,-19.620851301821823 -29.747160934607262,-19.61992408863952 -29.745712959454867,-19.619297177537277 -29.7441119164207,-19.61899466035717 -29.742419332690684,-19.619028162664783 -29.74070025330835,-19.61939639698522 -29.739020741530297,-19.620085212280053 -29.73744534005227,-19.621068137763796 -29.73603459066957,-19.622307400161336 -29.734842707689822,-19.62375537531373 -29.73391549450752,-19.6253564183479 -29.733288583405276,-19.627049002077914 -29.73298606622517,-19.628768081460247 -29.733019568532782,-19.6304475932383 -29.73338780285322,-29.164834804336436 -32.82973483928127,-31.586344616113706 -31.2325578835804,-14.522499048949383 -25.897172872851705,-12.911746604589299 -28.356773367533684,-27.590643470202913 -33.86803834336524),"
            "(-15.440268307843084 -24.82560063405746,-33.11063724365916 -30.227166517161177,-35.22563117058885 -28.832160994115174,-15.440268307843084 -24.82560063405746),"
            "(-110.06713343754421 -53.75156377977681,-133.70536993886762 -54.03818707453698,-134.92982691302475 -49.02232991848598,-110.98866705210969 -44.17423216097226,-107.81630674108442 -53.06352104066732,-110.06713343754421 -53.75156377977681),"
            "(-33.13242765496227 -30.23380803484376,-53.47598660261661 -36.452516973556065,-67.09295594277563 -35.28529916770173,-35.25002833910638 -28.837083149550402,-33.13242765496227 -30.23380803484376),"
            "(-53.520970953712265 -36.466267226310144,-58.2354762927896 -37.90741924457058,-71.67669094887043 -36.21351002448205,-67.15404194693704 -35.29766922081722,-53.520970953712265 -36.466267226310144),"
            "(-58.27645089887022 -37.919936120256196,-107.79952065899286 -53.05836684358259,-110.97130478081613 -44.1706925069293,-71.73052957001556 -36.22440573167339,-58.27645089887022 -37.919936120256196),"
            "(-110.1268958664014 -53.769831619846144,-132.12902100408343 -60.495532046101545,-133.70109935980145 -54.05568104535152,-110.1268958664014 -53.769831619846144),"
            "(-15.266747577272781 -24.772564602869423,-35.24640100751268 -28.818461646399513,-76.15530720021192 -1.8358005614142456,-86.14108983925057 19.847703882307872,-83.40496813166878 23.48084268412572,-64.0088883758489 6.425625058377867,-64.00760096344138 6.424685804640387,-64.00616485098435 6.4239950012666585,-64.00462744803231 6.423575453351113,-64.00303950798461 6.423441011187763,-64.00145345259266 6.42359611303842,-63.99992164139084 6.4240356386149005,-63.998494643181274 6.424745078112161,-63.99721956663504 6.4257010112121415,-63.996138505120754 6.42687188024526,-63.995287147100356 6.428219031985601,-63.994693597966496 6.429697993687634,-63.99437745221531 6.431259941239379,-63.99434914658464 6.432853310964729,-65.04832015933097 20.80381197052364,-41.89536979911647 15.900886582972497,-15.260289088462184 -24.770575950704515,-15.266747577272781 -24.772564602869423),"
            "(-103.34718678997828 -53.67007971050907,-54.4974791531819 -38.39617545176479,-48.5956002477636 -39.13996827390118,-92.94911174811018 -53.54399645486019,-103.34718678997828 -53.67007971050907),"
            "(-31.60797852622848 -31.239302626156118,-29.186203265828524 -32.836654666187016,-43.067588267658635 -37.344718300390326,-49.39887467327895 -36.801998383222546,-31.60797852622848 -31.239302626156118),"
            "(-49.44306980595738 -36.81581628610981,-43.11047066152018 -37.358644575644625,-48.55637966222821 -39.12723118950323,-54.45715723826972 -38.38357632163891,-49.44306980595738 -36.81581628610981),"
            "(-103.40555982520841 -53.68833070320558,-93.00523491310064 -53.562222737803324,-106.10185774557723 -57.815397753737585,-107.15619821197518 -54.861024333167755,-103.40555982520841 -53.68833070320558),"
            "(23.61518017648848 -16.91683756790308,26.50528396128093 -16.33157619705981,42.94251839317316 -26.78946994271238,42.92206386339187 -26.800842478829786,23.61518017648848 -16.91683756790308),"
            "(45.291790447718896 -28.01400696853263,64.11042508815196 -24.093836502795284,67.31540237012506 -26.10568053911328,51.48947303426359 -31.186856314772136,45.291790447718896 -28.01400696853263),"
            "(80.89532917461274 -38.11504345691095,85.72360464810268 -37.66099230023744,91.54967590863684 -41.31818491465435,86.63190829943045 -44.27761401903968,80.89532917461274 -38.11504345691095),"
            "(55.207151814658104 -10.519422962708234,63.930538194951616 -8.75294135555174,68.40404830643757 -12.643844730861389,61.009133000867436 -16.752229759599903,55.207151814658104 -10.519422962708234),"
            "(67.23698237538382 -23.44253327219278,78.19510992588658 -21.159810385859345,79.41506647143727 -22.220883708516908,69.16366426579913 -25.51226555715126,67.23698237538382 -23.44253327219278),"
            "(67.22302522687737 -23.42752227367621,61.02145616510926 -16.765450706696242,68.4181386751373 -12.656083874105,78.17850565684051 -21.14535081755846,67.22302522687737 -23.42752227367621),"
            "(96.07462679974704 -36.687606632181144,96.22981908999839 -36.67300010138628,96.16231350114971 -36.76386105410172,96.07462679974704 -36.687606632181144),"
            "(78.21666949675688 -21.155319242128968,91.07942965547718 -18.475847420826167,79.43458494021817 -22.214616982017457,78.21666949675688 -21.155319242128968),"
            "(78.20006522772039 -21.140859673826085,68.43444397509445 -12.647025156396275,120.67073097659974 16.373831825187075,138.40226861975972 21.917962128038926,139.5590597714433 21.64715021353345,115.5981191196391 -10.603717556458868,91.2383062237693 -18.424837476765795,78.20006522772039 -21.140859673826085),"
            "(96.05634598878443 -36.67170655960538,79.60266151961639 -22.360812613230724,92.37994996465073 -18.204936463794787,113.16809212086675 -13.874478509513061,96.24389265779745 -36.65405740641633,96.05634598878443 -36.67170655960538),"
            "(45.58859841303463 -12.467188312478434,56.44996967273963 -19.285159155930536,42.959952931426386 -26.779771024798904,26.53006557073263 -16.326551720595333,45.58859841303463 -12.467188312478434),"
            "(63.3670170846453 -39.76339728257143,80.8732966434185 -38.11712042833085,86.61655030813772 -44.286856197611016,80.54514164778 -47.94052691203084,75.54588157567393 -47.51198000904026,63.3670170846453 -39.76339728257143),"
            "(75.57785867637746 -47.53232486223543,80.5195359645405 -47.95593597773675,78.32209776055991 -49.27831700358555,75.57785867637746 -47.53232486223543),"
            "(44.99652318342139 -28.0755150444964,45.2644170651957 -28.01970920611602,51.48470250967003 -31.204123952853422,51.48629912522756 -31.20475275015989,51.48798761440433 -31.205058644898525,51.489703348260264 -31.20502992858282,51.49138065503266 -31.20466770036407,67.3348255051565 -26.117868279807055,67.56007186320271 -26.259277902547574,50.7404020749334 -31.72995854149191,44.99652318342139 -28.0755150444964),"
            "(63.338554000982406 -39.76607948751283,75.50811715621062 -47.508744406950505,26.068801349075883 -43.27092792395031,63.338554000982406 -39.76607948751283),"
            "(75.54009425476244 -47.5290892587767,78.31727689644644 -49.29602159154496,78.31871625003048 -49.29676048598416,78.32026683409454 -49.297222430561355,78.32187588641662 -49.29739170654002,78.32348865525648 -49.29726255391205,78.32505026241076 -49.29683936739541,78.32650757057118 -49.296136546893635,80.54932723087974 -47.95848139128968,87.408400175649 -48.54642481376636,82.1668571223638 -55.601419868886786,2.1628596589493014 -45.519030305358946,10.670127334892323 -44.719016219466,10.478355937853271 -44.825558445303535,10.477022945408265 -44.82646895355328,10.475879396234872 -44.82760833470957,10.474964026699563 -44.82893799359257,10.474307843855296 -44.830412889591,10.473933075114516 -44.83198306235927,10.473852415322028 -44.83359532416468,10.474068596732254 -44.835195061558494,10.474574296457355 -44.83672808534152,10.475352384521349 -44.838142466158686,10.47637650411764 -44.8393902935441,10.47761196441446 -44.84042929883103,10.47901691566548 -44.841224286952674,10.480543766820137 -44.8417483286331,10.482140797613786 -44.84198367258443,10.483753910530092 -44.84192234681058,10.485328463289965 -44.841566428649095,10.940150803529534 -44.693633216985006,25.970875452212482 -43.28014013939624,75.54009425476244 -47.5290892587767),"
            "(44.97189605739587 -28.080645200533002,50.71876939527397 -31.736994677353735,17.512462576017413 -42.53750991301251,15.017806026020082 -42.32364842620168,38.064721701858005 -29.51950096385712,44.97189605739587 -28.080645200533002),"
            "(63.31443324277529 -39.750733090438565,25.97083832848871 -43.262530741061205,17.557355750597907 -42.54135463818072,50.737959125026705 -31.74919957485254,63.31443324277529 -39.750733090438565),"
            "(85.75231357643165 -37.6583020340463,96.05049525434477 -36.68986655103241,96.15182157912969 -36.77798293714598,93.77244835997257 -39.980558168609235,91.56652005301999 -41.308048394369706,85.75231357643165 -37.6583020340463),"
            "(45.61355673842014 -12.462143693709407,55.186990966922515 -10.523510597075763,60.993339532379515 -16.76100411965765,56.46747536013181 -19.27543354028412,45.61355673842014 -12.462143693709407),"
            "(64.13520076257863 -24.088675402162217,67.2169128156077 -23.446714026907344,69.14521649458379 -25.51818851852844,67.33723331195878 -26.098671354979363,64.13520076257863 -24.088675402162217),"
            "(64.11378371141129 -24.075218400024625,56.48444685596549 -19.286072018869127,61.00566269662134 -16.774225066753985,67.20295566340234 -23.431703029161305,64.11378371141129 -24.075218400024625),"
            "(45.591992977695206 -12.448607551276503,42.82001730414031 -10.708559948596815,50.72722514027072 -5.7325837246547025,55.17296905968835 -10.508447510846764,45.591992977695206 -12.448607551276503),"
            "(90.83906500907895 -48.82288432501792,87.44124588861607 -48.531629462280534,88.82907600317388 -46.66364240274166,90.83906500907895 -48.82288432501792),"
            "(23.58758948005409 -16.922419734971612,42.90360027945853 -26.81109719825043,38.05849031533148 -29.502880586949516,-6.114132515913077 -38.70463297488126,-12.88506814740878 -28.365491980870296,10.5740180759243 -19.557686478564662,23.58758948005409 -16.922419734971612),"
            "(-3.2757307439501204 -22.362271881443974,-14.515075742325166 -25.876489092295138,-15.231034498305647 -24.783228289527237,-3.2757307439501204 -22.362271881443974),"
            "(-103.408268267121 -53.67082036628739,-107.56298077416507 -53.72117660087794,-107.79361648076426 -53.07491097499442,-58.233960524270685 -37.92529090694332,-54.5393407101279 -38.390899910893495,-103.408268267121 -53.67082036628739),"
            "(-31.626870337402607 -31.22684198473644,-49.444973181371616 -36.79804691662782,-53.42912203896892 -36.45653410481242,-33.113427002687885 -30.246340465564593,-31.626870337402607 -31.22684198473644),"
            "(-49.48916830524817 -36.811864820269555,-54.49901880228574 -38.378300779876625,-58.192985934280664 -37.912774029229915,-53.47410637084559 -36.47028435921391,-49.48916830524817 -36.811864820269555),"
            "(-31.605236430328155 -31.220097240155404,-33.0916365938955 -30.239698946225996,-15.262540010301606 -24.789610509222257,-15.249551258319446 -24.786972494086065,-14.532484538870866 -25.881925144352813,-31.605236430328155 -31.220097240155404),"
            "(-103.46664127579638 -53.6890713586619,-107.16209180873788 -54.84450985214174,-107.55674612108137 -53.73864675776175,-103.46664127579638 -53.6890713586619),"
            "(-110.00462741884331 -53.750805850432336,-107.81040256225506 -53.08006517376252,-107.58151701429983 -53.72142667015059,-110.00462741884331 -53.750805850432336),"
            "(-110.06438982504433 -53.76907369022695,-107.57528236121614 -53.738896827034395,-107.1788402567078 -54.84976950401287,-153.2430747370277 -69.25271687255216,-141.87272856231922 -54.15476717032229,-133.71910155050836 -54.055905624595034,-132.14385744166628 -60.508724794743074,-132.14334419803262 -60.51022379344769,-132.14256964852405 -60.511605997919794,-132.14155906878108 -60.51282630309763,-132.14034543674777 -60.513844887130325,-132.1389683565123 -60.514628510872285,-132.1374727659187 -60.51515160256623,-132.13590747012384 -60.51539709231865,-132.13432354895338 -60.51535696913649,-132.13277269002927 -60.51503254234744,-110.06438982504433 -53.76907369022695),"
            "(-27.592871920864 -33.887582518965544,-19.25708965943322 -39.38568922534542,-37.965533667372725 -37.78204018761589,-27.592871920864 -33.887582518965544),"
            "(-38.0037121954104 -37.796373821125144,-19.22641600937537 -39.405920919960636,-12.821155357361697 -43.63069616509796,-43.309481338204634 -39.78844570828592,-38.0037121954104 -37.796373821125144),"
            "(-43.34475623878629 -39.801680880936765,-12.788017983898143 -43.65255287986782,-8.469056186486533 -46.50124926830628,-78.80591177161446 -53.11570776137347,-43.34475623878629 -39.801680880936765),"
            "(-27.572549585136507 -33.87997268950195,-12.901902948535914 -28.37180451734392,-6.125539187747014 -38.71923420248274,-6.124615012752566 -38.72041215386896,-6.1235039573952275 -38.72141575980836,-6.12223839702193 -38.72221577595208,-6.120855209143663 -38.72278889041018,-6.119394698851558 -38.7231184030432,-6.11789942435578 -38.723194712092116,-6.116412956870168 -38.72301559396661,38.006907672851085 -29.531544390670412,14.986494714540797 -42.320976703350695,-19.220444896885056 -39.3888453254074,-27.572549585136507 -33.87997268950195),"
            "(163.85356981584565 38.01045640606598,150.78183751989167 33.1026264373484,160.43346758187292 38.46477202131451,163.85356981584565 38.01045640606598),"
            "(127.70830325425007 24.439550041637304,106.54590505913245 29.393902120191427,119.4559466211646 37.51814283753344,144.37633145980607 40.59773643801638,160.4042990486444 38.46863420514055,150.67036889056672 33.060765246572494,127.70830325425007 24.439550041637304),"
            "(137.87665775262624 22.04101325621122,122.3018982810747 17.280057664991475,132.94821653533805 23.194814080812744,137.87665775262624 22.04101325621122),"
            "(137.91007347628613 22.051206470998714,132.97106197457936 23.207481912287477,150.67791335246926 33.044889401030915,163.89246423334544 38.00632192281352,163.89395437188944 38.00704878327708,163.89528110506717 38.00804307603519,163.89639702613067 38.00926927308045,163.89726226105742 38.01068356001573,163.89784589332643 38.0122354016276,163.8981270686255 38.01386934760742,163.89809574001623 38.01552701389832,163.8977530269305 38.01714916887077,163.8971111751712 38.018677849783465,163.8961931193457 38.020058433904424,163.8950316633681 38.02124159028708,163.89366830831162 38.02218504246065,163.892151769495 38.02285507905031,163.8905362357896 38.02322775835002,160.45914599302486 38.479038148184,164.20988616119448 40.562819558817196,165.3016644430351 30.424412023824377,137.91007347628613 22.051206470998714),"
            "(138.43525780056368 21.928255201073114,165.3116438348011 30.33174198050335,171.7604175947424 -29.552483202131402,99.90657434057331 -36.30961852717998,149.90557033770168 -6.2210711003068075,149.90688874767233 -6.220099211525472,149.9080034680365 -6.2188991402094,149.90887562498716 -6.217512736621926,149.90947480366685 -6.215988349019058,149.9097801088294 -6.214379137593647,149.90978089352305 -6.212741220612166,149.90947713038307 -6.211131717394169,149.90887941258603 -6.209606756381856,149.9080085844325 -6.20821951776467,149.90689501444066 -6.207018378918731,149.90557753630043 -6.206045227335496,149.90410209462055 -6.205333999873108,149.90252014269552 -6.204909499271364,149.90088684816726 -6.204786529202118,149.89925916915584 -6.204969378018662,113.2096851912215 -13.847909418407404,115.57854716414471 -10.659476291875812,122.98695766373115 -8.24983286863276,122.99248836987037 -8.24805714415804,122.99248165360493 -8.248036225546489,122.99252002096307 -8.24804691580876,122.99409329720051 -8.247358404482963,122.99550225424824 -8.246376525670339,122.99669287690277 -8.24513892167087,122.99761952024896 -8.243693038524139,122.99824665954954 -8.242094307069925,122.99855025215751 -8.240404017895434,122.9985186592394 -8.238686971637309,122.99815309197326 -8.237008994718828,122.99746756511585 -8.235434415761524,122.99648835971863 -8.234023599418036,122.99525301559079 -8.232830632171684,122.99380889213523 -8.231901248822297,122.99221135273041 -8.231271079150648,122.99052164226393 -8.230964281979102,122.98880453918649 -8.230992618994556,122.98712587209964 -8.231355003840502,122.98709746667338 -8.231375211028098,122.98709422100694 -8.23136523218978,122.98445994390397 -8.232210944081352,115.62680134884883 -10.59450865345169,139.5811057192091 21.647409332212078,139.58192836390296 21.648754158529584,139.58249698017912 21.650224525030524,139.58279319818183 21.651772929697092,139.58280744822028 21.653349349391526,139.58253926992927 21.65490285591575,139.58199732714186 21.65638326131015,139.58119912799356 21.65774273923798,139.5801704593002 21.65893737007465,139.5789445534828 21.659928559785786,139.5775610149529 21.660684286755505,139.57606454064265 21.661180136284507,138.43525780056368 21.928255201073114),"
            "(-11.672901457713799 61.32672411402214,90.98812319335303 47.68960389094599,84.7626026713778 42.82467865599619,70.29932468799596 37.879602651101656,-6.606416144696235 55.884063056220455,-11.672901457713799 61.32672411402214),"
            "(21.731752401457072 25.441649913510176,24.257151920623585 25.753718243894582,27.241673724801043 23.15789335269138,24.67466474051967 22.280215188935983,21.731752401457072 25.441649913510176),"
            "(27.27614200608447 23.151139413488384,40.90533858676494 27.81105618886412,88.24260741703002 33.66087553733939,106.50102688198155 29.386386724583776,56.37475342083092 -2.1578890939673077,27.27614200608447 23.151139413488384),"
            "(24.280548333299286 25.756617557996357,40.820431510930504 27.800564754617064,27.26084581815643 23.16444842429204,24.280548333299286 25.756617557996357),"
            "(-8.816428652507156 21.684268290604805,-29.245606398615163 19.15968622376516,-30.746990152471433 20.114940195264964,-31.488293120007164 35.91601695199151,-8.816428652507156 21.684268290604805),"
            "(-29.272966059430175 19.15630519150303,-30.693779343676994 18.980737162403244,-30.745984888163573 20.09351270989812,-29.272966059430175 19.15630519150303),"
            "(-8.765268372199529 21.672915212812164,21.710263177057982 25.43898929273592,24.6564884472217 22.274000588276877,2.399797039244074 14.664283533978011,-8.765268372199529 21.672915212812164),"
            "(-11.719020876527594 61.35052257161079,-32.813002816538585 64.15256868853093,-33.794092220123524 85.06475931794377,-11.719020876527594 61.35052257161079),"
            "(138.36863398834245 21.92583635515323,120.74643517661659 16.41589072499998,122.22857097659741 17.23931929444039,137.91059951928324 22.033067125450867,138.36863398834245 21.92583635515323),"
            "(138.40162314694612 21.936129433384743,137.94401525032401 22.043260338510414,165.30357425200395 30.40667726763579,165.30972750566778 30.349537283861128,138.40162314694612 21.936129433384743),"
            "(127.7390129190668 24.432360580502113,150.56644469028546 33.00302819199967,132.94563868010746 23.21343377755938,127.7390129190668 24.432360580502113),"
            "(-161.35503387993074 -54.37346556730929,-138.5871756703948 -49.76293390065463,-141.88154531641396 -54.13731498386075,-161.35503387993074 -54.37346556730929),"
            "(-35.27079817688228 -28.823383801272726,-67.15508315200245 -35.279973754753655,-104.45438672212283 -32.08275635744238,-104.45608427388785 -32.08277574513043,-104.45774629244461 -32.083121797430444,-104.45931051252892 -32.08378154996377,-104.46071833276328 -32.084730285999285,-104.46191701107783 -32.08593246243208,-104.46286164062022 -32.08734304135757,-104.46351683212971 -32.088909177355255,-104.46385803974773 -32.09057219727058,-104.46387248059507 -32.0922697983248,-104.46355961366501 -32.09393838220386,-104.46293116009147 -32.09551543768267,-104.46201066403262 -32.096941882523176,-104.46083261062104 -32.09816427690997,-104.4594411340252 -32.0991368255003,-104.45788836402274 -32.099823093084304,-104.45623247302943 -32.10019736958055,-71.78503500133692 -36.217536765212955,-110.97726480920838 -44.153991877456086,-118.48907838644061 -23.105096307062404,-86.1530711337859 19.831784241114434,-76.17018886097277 -1.845422238170359,-76.16937610532348 -1.846827817505229,-76.16831819663724 -1.848059505056384,-76.1670513874113 -1.8490750932589226,-35.27079817688228 -28.823383801272726),"
            "(-104.01746970265641 -32.1378111596404,-67.21616917021902 -35.29234380666436,-71.7311963690772 -36.20664105942231,-104.01746970265641 -32.1378111596404),"
            "(-19.189771245865394 -39.40907702065701,14.959045315399937 -42.33622673608511,10.708277693154 -44.69782106654718,2.082853933046394 -45.50894770194591,-12.781743251109447 -43.63567754687929,-19.189771245865394 -39.40907702065701),"
            "(25.87291249723667 -43.271742962131206,11.016303004296148 -44.66886433771501,17.514523291044114 -42.55528610711852,25.87291249723667 -43.271742962131206),"
            "(-12.74860587593394 -43.65753426277832,2.0024981350794633 -45.51650162110258,-8.441149421139997 -46.49864196914455,-12.74860587593394 -43.65753426277832),"
            "(0.5482054515958907 -52.42783142752352,22.844877876175584 -52.15747591132524,42.52338780560707 -80.11376900277862,0.5482054515958907 -52.42783142752352),"
            "(70.30021875120347 37.86136945403442,88.19316357634735 33.672450869505475,40.98637445712528 27.838762914602786,70.30021875120347 37.86136945403442),"
            "(70.33149133386337 37.8720617667972,84.7697194975705 42.80859204783613,84.77105909891975 42.80917893165259,84.77228255716334 42.80998025200993,91.0124858249585 47.68637930033897,127.81301147359562 42.79793866689235,119.44990642462011 37.5350680374038,88.24355226977912 33.67867053045407,70.33149133386337 37.8720617667972),"
            "(144.3073988767456 40.60689326310742,119.4909083116505 37.540144098182196,127.84021527260577 42.79433164264914,144.3073988767456 40.60689326310742),"
            "(88.29299612070747 33.66709519588936,119.4149447434929 37.5130667826444,106.52189974628405 29.399521939775685,88.29299612070747 33.66709519588936),"
            "(144.44527145744826 40.60625585489642,163.9457022420348 43.01606879557336,164.20784624646586 40.58176249549163,160.4299774597963 38.482900332010054,144.44527145744826 40.60625585489642),"
            "(144.37633892649745 40.61541268642703,127.86340960426224 42.808927748536924,161.67658544228874 64.08740831841152,163.94382395897412 43.0335107977197,144.37633892649745 40.61541268642703),"
            "(-41.90900718265291 15.92171071110581,-65.04965636135107 20.822031166079725,-65.63127790159731 28.752476130223627,-42.58516701826829 16.9541990192793,-41.90900718265291 15.92171071110581),"
            "(-42.60459058740299 16.983858586408623,-65.63278027468877 28.772961078412454,-66.67261586220584 42.95118235795973,-54.565741638677004 35.24839895429071,-42.60459058740299 16.983858586408623),"
            "(-54.58907517986818 35.28402900387246,-66.6742146811002 42.97298235051845,-66.98910868886206 47.26628926429717,-62.12011091968531 46.783833855102905,-54.58907517986818 35.28402900387246),"
            "(-54.55313781876444 35.261172150516714,-62.097702142309025 46.78161343185131,-46.30482922656958 45.2167417013382,-31.506387108397742 35.92737383993618,-30.76509132865387 20.12645029297113,-54.55313781876444 35.261172150516714),"
            "(-97.83416987317577 45.238498178710174,-65.64955249400556 28.76182418273821,-65.06752232404301 20.825807527637693,-82.6206997737451 24.54292980246981,-82.62234422414674 24.543118386846405,-82.62399482199257 24.54299466157833,-82.62559278256681 24.54256303304157,-82.62708119578785 24.541838873339582,-82.62840705301502 24.540847972838158,-82.62952313490874 24.539625621662307,-83.40757403122092 23.506493075556897,-90.83289438322406 30.035664942125123,-97.83416987317577 45.238498178710174),"
            "(-82.61888709171882 24.52458915191082,-65.06618612106601 20.80758831903435,-64.01341032562904 6.452926519952804,-83.39437198775232 23.49488435218103,-82.61888709171882 24.52458915191082),"
            "(-65.65105486819753 28.782309145932423,-97.84873506427972 45.26564377964443,-97.85028505214999 45.266259170730656,-97.8519235011197 45.266570062288324,-97.85359117767403 45.26656521490745,-97.85522779165703 45.26624480383146,-97.85677417589265 45.26562041262175,-97.85817442520765 45.26471461438658,-97.85937791752582 45.263560155713826,-97.86034114396688 45.26219877281023,-97.86102928178767 45.260679682646156,-97.86141745330046 45.259057803654485,-97.86149162525584 45.25739177030908,-97.86124911617533 45.255741813360295,-97.86069869329278 45.254167582361575,-90.86536170723733 30.064229436755205,-122.87031432291771 58.20657846684874,-122.87160397462488 58.20751906037725,-122.87304273757687 58.20821034821862,-122.87458295968815 58.20862943479625,-122.87617362852775 58.20876243988797,-122.87776206086001 58.208604958341404,-122.87929564752282 58.20816220597348,-122.88072359585243 58.20744884682156,-122.88199861194552 58.20648850746798,-122.88307846704171 58.20531299452339,-122.88392739614764 58.203961241186136,-122.88451728257994 58.20247801776782,-122.88482858919475 58.20091244889254,-128.50870223166575 4.921332760433136,-128.50874214078485 4.920024935850124,-128.5085870504444 4.918725726527491,-128.508240412044 4.917464045229747,-118.50876745117507 -23.102116039964123,-135.68859242632564 -45.914092066443295,-166.90162006241783 81.94670439205376,-32.82983975785978 64.13713127225718,-31.507388935807747 35.94872806687944,-46.297243998766895 45.232694686742164,-46.298438120776574 45.23332182175007,-46.29971427817714 45.23375845429706,-46.30104229243783 45.233994258910144,-62.110046067823596 46.80046435131626,-67.56717234515712 55.133453500144235,-67.56820100882734 55.134741527678834,-67.56945250271089 55.1358143403073,-67.5708826237989 55.13663404607767,-67.5724408599347 55.13717169282173,-67.5740721739131 55.13740829075129,-67.57571894740512 55.137335483181175,-67.57732301604885 55.136955841689016,-67.57882772382588 55.13628277528659,-67.5801799241631 55.13534005681094,-67.58133185708003 55.13416098326309,-67.58224283608062 55.13278719975146,-67.58288068520834 55.1312672285785,-67.58322287550715 55.12965475542346,-67.58325732074876 55.12800673315368,-67.00809756718309 47.28579861360277,-74.64384103040652 48.042403137098944,-74.64551355318005 48.04240861420995,-74.64715670691223 48.04209651127949,-74.6487107421129 48.04147817720973,-74.6501191498815 48.04057609629091,-74.65133071672265 48.039423070612024,-74.65230138680663 48.03806102728863,-74.6529958639579 48.03653949388014,-74.65338889511892 48.03491379743455,-74.65346618861936 48.03324305264807,-74.6532249338596 48.0315880122958,-74.65267390351174 48.03000885809735,-74.65183313452161 48.028563012347625,-74.65073319951154 48.02730304988773,-74.64941409507755 48.02627478634224,-66.69106576546662 42.962920784577285,-65.65105486819753 28.782309145932423),"
            "(-74.60894342391143 48.02131743628841,-67.00680717272054 47.268042957701205,-66.69266458534088 42.98472079049692,-74.60894342391143 48.02131743628841),"
            "(-138.56127670492663 -49.7577037198288,-134.94703094520807 -49.02582406883465,-133.72337212957453 -54.038411653780486,-141.85939298329575 -54.13705971460783,-138.56127670492663 -49.7577037198288),"
            "(-134.93399269617888 -49.00526523292734,-135.68321498734667 -45.93611109773349,-118.50171193441933 -23.121886343614484,-110.99462708067594 -44.15753153101153,-134.93399269617888 -49.00526523292734),"
            "(-138.54536289076012 -49.736572791872405,-135.69686656519858 -45.95423814792149,-134.95119672848287 -49.00875938278158,-138.54536289076012 -49.736572791872405),"
            "(88.82755353245742 -46.63627739859524,86.64413783102847 -44.29072778657896,91.56634116238342 -41.32862933986829,92.38779164183632 -41.84427969841862,88.82755353245742 -46.63627739859524),"
            "(96.17559112847069 -36.775403890830276,96.2533145203432 -36.670790012864686,99.8315395547437 -36.33429989598129,97.37090375596762 -37.815068768540016,96.17559112847069 -36.775403890830276),"
            "(92.39828457197696 -41.83015645846746,91.58318530508117 -41.31849282059788,93.74495703397014 -40.01757527428385,92.39828457197696 -41.83015645846746),"
            "(96.16509920779882 -36.78952577205994,97.35510564997305 -37.824575800671205,93.81196732309566 -39.95677632714469,96.16509920779882 -36.78952577205994),"
            "(88.81691795134695 -46.65059264343438,87.42069611419876 -48.52987476642635,80.5749329147718 -47.94307232519108,86.62877983973574 -44.29996996515029,88.81691795134695 -46.65059264343438),"
            "(69.32473564139825 -25.685312296376537,69.17686596344828 -25.526450792983923,79.43054197209301 -22.234338902371594,79.56777154519035 -22.35371429782551,69.32473564139825 -25.685312296376537),"
            "(79.58722163711042 -22.347388059401556,79.45006045094098 -22.22807217263994,91.24299727724733 -18.441755195353803,92.22218393874856 -18.237804190782946,79.58722163711042 -22.347388059401556),"
            "(67.58180682559251 -26.252208498722716,67.35665645137476 -26.110859094265415,69.15841818541341 -25.532373756550623,69.30634307855388 -25.691294568247503,67.58180682559251 -26.252208498722716),"
            "(10.713631867610786 -44.714913767695485,10.86035670084666 -44.70114019099129,10.566326319189315 -44.7967750242813,10.713631867610786 -44.714913767695485),"
            "(14.990356626143694 -42.338898459344726,17.469630118109404 -42.551441381415,10.93650885434335 -44.676371327096064,10.75178223878234 -44.69371860760433,14.990356626143694 -42.338898459344726),"
            "(113.19393813942808 -13.869104583916076,149.85225961536605 -6.232679296303189,99.86624158413852 -36.31341680350249,96.26738808398915 -36.65184732348474,113.19393813942808 -13.869104583916076),"
            "(113.18383917989723 -13.853283334263445,92.53313651295461 -18.155111782662388,115.54974201699517 -10.668833135736556,113.18383917989723 -13.853283334263445),"
            "(163.96311491920022 43.01821047323797,169.91789145903348 43.754090361605556,164.22449464723803 40.59100135183526,163.96311491920022 43.01821047323797),"
            "(92.37537042521268 -18.187979529764235,91.40187377110232 -18.3907452751927,115.58013059889898 -10.627916926612096,115.56781674564326 -10.64450497131272,92.37537042521268 -18.187979529764235),"
            "(-86.16139825386244 19.849866063211905,-118.49613390319635 -23.085326003412035,-128.4910459077906 4.921493651135869,-122.86916414947727 58.18220258298618,-90.8472856693717 30.0249784474685,-86.16139825386244 19.849866063211905),"
            "(-86.14941695432584 19.865785693545394,-90.81481832588861 29.996413910560918,-83.41817017513736 23.49245140750158,-86.14941695432584 19.865785693545394),"
            "(-106.11264211278916 -57.83736939023901,-100.24152662041178 -74.28881727173125,-161.4303237477923 -75.80205692190154,-106.11264211278916 -57.83736939023901),"
            "(-67.56328532753739 55.095514517777545,-62.132454845199874 46.802684774567844,-66.99039908239888 47.28404492010701,-67.56328532753739 55.095514517777545),"
            "(115.59662188220229 -10.635148141706566,115.6088128566935 -10.618708014427298,118.58211270640945 -9.664081031738045,115.59662188220229 -10.635148141706566)))";
        std::string msg;
        CG mpoly = from_wkt<CG>(wkt);
        bg::correct(mpoly);

        BOOST_CHECK_MESSAGE(bg::is_valid(mpoly, msg), msg);
    }
#endif // BOOST_GEOMETRY_TEST_FAILURES
}

BOOST_AUTO_TEST_CASE( test_is_valid_multipolygon )
{
    test_open_multipolygons<point_type, true>();
    test_open_multipolygons<point_type, false>();
}


template <typename CoordinateType, typename Geometry>
inline void check_one(Geometry const& geometry)
{
    bool const is_fp = boost::is_floating_point<CoordinateType>::value;

    bg::validity_failure_type failure;
    bool validity = bg::is_valid(geometry, failure);
    if (BOOST_GEOMETRY_CONDITION(is_fp))
    {
        BOOST_CHECK(! validity);
        BOOST_CHECK(failure == bg::failure_invalid_coordinate);
    }
    else
    {
        BOOST_CHECK(failure == bg::no_failure
                    || failure != bg::failure_invalid_coordinate);
    }
}

template <typename P, typename CoordinateType>
inline void test_with_invalid_coordinate(CoordinateType invalid_value)
{
    typedef bg::model::segment<P> segment_t;
    typedef bg::model::box<P> box_t;
    typedef bg::model::linestring<P> linestring_t;
    typedef bg::model::ring<P> ring_t; // cw, closed
    typedef bg::model::polygon<P> polygon_t; // cw, closed
    typedef bg::model::multi_point<P> multi_point_t;
    typedef bg::model::multi_linestring<linestring_t> multi_linestring_t;
    typedef bg::model::multi_polygon<polygon_t> multi_polygon_t;

    typedef typename bg::coordinate_type<P>::type coord_t;

    std::string wkt_point = "POINT(1 1)";
    std::string wkt_segment = "LINESTRING(1 1,10 20)";
    std::string wkt_box = "BOX(1 1,10 20)";
    std::string wkt_linestring = "LINESTRING(1 1,2 3,4 -5)";
    std::string wkt_ring = "POLYGON((1 1,2 2,2 1,1 1))";
    std::string wkt_polygon = "POLYGON((1 1,1 200,200 200,200 1,1 1),(50 50,50 51,49 50,50 50))";
    std::string wkt_multipoint = "MULTIPOINT(1 1,2 3,4 -5,-5 2)";
    std::string wkt_multilinestring =
        "MULTILINESTRING((1 1,2 3,4 -5),(-4 -2,-8 9))";
    std::string wkt_multipolygon = "MULTIPOLYGON(((1 1,1 200,200 200,200 1,1 1),(50 50,50 51,49 50,50 50)),((500 500,550 550,550 500,500 500)))";

    {
        P p;
        bg::read_wkt(wkt_point, p);
        BOOST_CHECK(bg::is_valid(p));
        bg::set<1>(p, invalid_value);
        check_one<coord_t>(p);
    }
    {
        segment_t s;
        bg::read_wkt(wkt_segment, s);
        BOOST_CHECK(bg::is_valid(s));
        bg::set<1, 1>(s, invalid_value);
        check_one<coord_t>(s);
    }
    {
        box_t b;
        bg::read_wkt(wkt_box, b);
        BOOST_CHECK(bg::is_valid(b));
        bg::set<1, 0>(b, invalid_value);
        check_one<coord_t>(b);
    }
    {
        linestring_t ls;
        bg::read_wkt(wkt_linestring, ls);
        BOOST_CHECK(bg::is_valid(ls));
        bg::set<1>(ls[1], invalid_value);
        check_one<coord_t>(ls);
    }
    {
        ring_t r;
        bg::read_wkt(wkt_ring, r);
        BOOST_CHECK(bg::is_valid(r));
        bg::set<0>(r[1], invalid_value);
        check_one<coord_t>(r);
    }
    {
        polygon_t pgn;
        bg::read_wkt(wkt_polygon, pgn);
        BOOST_CHECK(bg::is_valid(pgn));
        bg::set<0>(bg::interior_rings(pgn)[0][1], invalid_value);
        check_one<coord_t>(pgn);
    }
    {
        multi_point_t mp;
        bg::read_wkt(wkt_multipoint, mp);
        BOOST_CHECK(bg::is_valid(mp));
        bg::set<0>(mp[2], invalid_value);
        check_one<coord_t>(mp);
    }
    {
        multi_linestring_t mls;
        bg::read_wkt(wkt_multilinestring, mls);
        BOOST_CHECK(bg::is_valid(mls));
        bg::set<0>(mls[1][1], invalid_value);
        check_one<coord_t>(mls);
    }
    {
        multi_polygon_t mpgn;
        bg::read_wkt(wkt_multipolygon, mpgn);
        BOOST_CHECK(bg::is_valid(mpgn));
        bg::set<0>(bg::exterior_ring(mpgn[1])[1], invalid_value);
        check_one<coord_t>(mpgn);
    }
}

template <typename P>
inline void test_with_invalid_coordinate()
{
    typedef typename bg::coordinate_type<P>::type coord_t;

    coord_t const q_nan = std::numeric_limits<coord_t>::quiet_NaN();
    coord_t const inf = std::numeric_limits<coord_t>::infinity();

    test_with_invalid_coordinate<P, coord_t>(q_nan);
    test_with_invalid_coordinate<P, coord_t>(inf);
}

BOOST_AUTO_TEST_CASE( test_geometries_with_invalid_coordinates )
{
    typedef point_type fp_point_type;
    typedef bg::model::point<int, 2, bg::cs::cartesian> int_point_type;

    test_with_invalid_coordinate<fp_point_type>();
    test_with_invalid_coordinate<int_point_type>();
}

BOOST_AUTO_TEST_CASE( test_with_NaN_coordinates )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: geometry with NaN coordinates" << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    linestring_type ls1, ls2;
    bg::read_wkt("LINESTRING(1 1,1.115235e+308 1.738137e+308)", ls1);
    bg::read_wkt("LINESTRING(-1 1,1.115235e+308 1.738137e+308)", ls2);

    // the intersection of the two linestrings is a new linestring
    // (multilinestring with a single element) that has NaN coordinates
    multi_linestring_type mls;
    bg::intersection(ls1, ls2, mls);

    typedef validity_tester_linear<true> tester_allow_spikes;
    typedef validity_tester_linear<false> tester_disallow_spikes;

    test_valid
        <
            tester_allow_spikes, multi_linestring_type
        >::apply("mls-NaN", mls, false);

    test_valid
        <
            tester_disallow_spikes, multi_linestring_type
        >::apply("mls-NaN", mls, false);
}

BOOST_AUTO_TEST_CASE( test_is_valid_variant )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: variant support" << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<point_type> polygon_type; // cw, closed

    typedef boost::variant
        <
            linestring_type, multi_linestring_type, polygon_type
        > variant_geometry;
    typedef test_valid_variant<variant_geometry> test;

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
    test::apply("v01", vg, true);
    vg = invalid_multi_linestring;
    test::apply("v02", vg, false);
    vg = valid_polygon;
    test::apply("v03", vg, true);
    vg = invalid_polygon;
    test::apply("v04", vg, false);
}
