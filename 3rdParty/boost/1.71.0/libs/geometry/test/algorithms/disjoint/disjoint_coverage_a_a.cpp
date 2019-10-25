// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_disjoint_coverage
#endif

// unit test to test disjoint for all geometry combinations

#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/algorithms/disjoint.hpp>

#include <from_wkt.hpp>


#ifdef HAVE_TTMATH
#include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif

namespace bg = ::boost::geometry;

//============================================================================

struct test_disjoint
{
    template <typename Geometry1, typename Geometry2>
    static inline void apply(std::string const& case_id,
                             Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             bool expected_result)
    {
        bool result = bg::disjoint(geometry1, geometry2);
        BOOST_CHECK_MESSAGE(result == expected_result,
            "case ID: " << case_id << ", G1: " << bg::wkt(geometry1)
            << ", G2: " << bg::wkt(geometry2) << " -> Expected: "
            << expected_result << ", detected: " << result);

        result = bg::disjoint(geometry2, geometry1);
        BOOST_CHECK_MESSAGE(result == expected_result,
            "case ID: " << case_id << ", G1: " << bg::wkt(geometry2)
            << ", G2: " << bg::wkt(geometry1) << " -> Expected: "
            << expected_result << ", detected: " << result);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "case ID: " << case_id << "; G1 - G2: ";
        std::cout << bg::wkt(geometry1) << " - ";
        std::cout << bg::wkt(geometry2) << std::endl;
        std::cout << std::boolalpha;
        std::cout << "expected/computed result: "
                  << expected_result << " / " << result << std::endl;
        std::cout << std::endl;
        std::cout << std::noboolalpha;
#endif
    }
};

//============================================================================

// areal-areal geometries
template <typename P>
inline void test_box_box()
{
    typedef bg::model::box<P> B;

    typedef test_disjoint tester;

    tester::apply("b-b-01",
                  from_wkt<B>("BOX(2 2,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("b-b-02",
                  from_wkt<B>("BOX(1 1,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("b-b-03",
                  from_wkt<B>("BOX(3 3,4 4)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);
}

template <typename P>
inline void test_ring_box()
{
    typedef bg::model::box<P> B;
    typedef bg::model::ring<P, false, false> R; // ccw, open

    typedef test_disjoint tester;

    tester::apply("r-b-01",
                  from_wkt<B>("BOX(2 2,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("r-b-02",
                  from_wkt<B>("BOX(1 1,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("r-b-03",
                  from_wkt<B>("BOX(3 3,4 4)"),
                  from_wkt<R>("POLYGON((0 0,2 0,2 2,0 2))"),
                  true);
}

template <typename P>
inline void test_polygon_box()
{
    typedef bg::model::box<P> B;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open

    typedef test_disjoint tester;

    tester::apply("pg-b-01",
                  from_wkt<B>("BOX(2 2,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("pg-b-02",
                  from_wkt<B>("BOX(1 1,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("pg-b-03",
                  from_wkt<B>("BOX(3 3,4 4)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  true);
}

template <typename P>
inline void test_multipolygon_box()
{
    typedef bg::model::box<P> B;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;
    
    typedef test_disjoint tester;

    tester::apply("mpg-b-01",
                  from_wkt<B>("BOX(2 2,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("mpg-b-02",
                  from_wkt<B>("BOX(1 1,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("mpg-b-03",
                  from_wkt<B>("BOX(3 3,4 4)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  true);
}

template <typename P>
inline void test_ring_ring()
{
    typedef bg::model::ring<P, false, false> R; // ccw, open

    typedef test_disjoint tester;

    tester::apply("r-r-01",
                  from_wkt<R>("POLYGON((2 2,2 3,3 3,3 2))"),
                  from_wkt<R>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("r-r-02",
                  from_wkt<R>("POLYGON((1 1,1 3,3 3,3 1))"),
                  from_wkt<R>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("r-r-03",
                  from_wkt<R>("POLYGON((3 3,3 4,4 4,4 3))"),
                  from_wkt<R>("POLYGON((0 0,2 0,2 2,0 2))"),
                  true);
}

template <typename P>
inline void test_polygon_ring()
{
    typedef bg::model::ring<P, false, false> R; // ccw, open
    typedef bg::model::polygon<P, false, false> PL; // ccw, open

    typedef test_disjoint tester;

    tester::apply("pg-r-01",
                  from_wkt<R>("POLYGON((2 2,2 3,3 3,3 2))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("pg-r-02",
                  from_wkt<R>("POLYGON((1 1,1 3,3 3,3 1))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("pg-r-03",
                  from_wkt<R>("POLYGON((3 3,3 4,4 4,4 3))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  true);
}

template <typename P>
inline void test_multipolygon_ring()
{
    typedef bg::model::ring<P, false, false> R; // ccw, open
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;

    typedef test_disjoint tester;

    tester::apply("mpg-r-01",
                  from_wkt<R>("POLYGON((2 2,2 3,3 3,3 2))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("mpg-r-02",
                  from_wkt<R>("POLYGON((1 1,1 3,3 3,3 1))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("mpg-r-03",
                  from_wkt<R>("POLYGON((3 3,3 4,4 4,4 3))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  true);
}

template <typename P>
inline void test_polygon_polygon()
{
    typedef bg::model::polygon<P, false, false> PL; // ccw, open

    typedef test_disjoint tester;

    tester::apply("pg-pg-01",
                  from_wkt<PL>("POLYGON((2 2,2 3,3 3,3 2))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("pg-pg-02",
                  from_wkt<PL>("POLYGON((1 1,1 3,3 3,3 1))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  false);

    tester::apply("pg-pg-03",
                  from_wkt<PL>("POLYGON((3 3,3 4,4 4,4 3))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,2 2,0 2))"),
                  true);

    tester::apply("pg-pg-04",
                  from_wkt<PL>("POLYGON((0 0,9 0,9 9,0 9))"),
                  from_wkt<PL>("POLYGON((3 3,6 3,6 6,3 6))"),
                  false);
    // polygon with a hole which entirely contains the other polygon
    tester::apply("pg-pg-05",
                  from_wkt<PL>("POLYGON((0 0,9 0,9 9,0 9),(2 2,2 7,7 7,7 2))"),
                  from_wkt<PL>("POLYGON((3 3,6 3,6 6,3 6))"),
                  true);
    // polygon with a hole, but the inner ring intersects the other polygon
    tester::apply("pg-pg-06",
                  from_wkt<PL>("POLYGON((0 0,9 0,9 9,0 9),(3 2,3 7,7 7,7 2))"),
                  from_wkt<PL>("POLYGON((2 3,6 3,6 6,2 6))"),
                  false);
    // polygon with a hole, but the other polygon is entirely contained
    // between the inner and outer rings.
    tester::apply("pg-pg-07",
                  from_wkt<PL>("POLYGON((0 0,9 0,9 9,0 9),(6 2,6 7,7 7,7 2))"),
                  from_wkt<PL>("POLYGON((3 3,5 3,5 6,3 6))"),
                  false);
    // polygon with a hole and the outer ring of the other polygon lies
    // between the inner and outer, but without touching either.
    tester::apply("pg-pg-08",
                  from_wkt<PL>("POLYGON((0 0,9 0,9 9,0 9),(3 3,3 6,6 6,6 3))"),
                  from_wkt<PL>("POLYGON((2 2,7 2,7 7,2 7))"),
                  false);

    {
        typedef bg::model::polygon<P> PL; // cw, closed

        // https://svn.boost.org/trac/boost/ticket/10647
        tester::apply("ticket-10647",
            from_wkt<PL>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0)(1 1, 4 1, 4 4, 1 4, 1 1))"),
            from_wkt<PL>("POLYGON((2 2, 2 3, 3 3, 3 2, 2 2))"),
            true);
    }
}

template <typename P>
inline void test_polygon_multipolygon()
{
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;

    typedef test_disjoint tester;

    tester::apply("pg-mpg-01",
                  from_wkt<PL>("POLYGON((2 2,2 3,3 3,3 2))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("pg-mpg-02",
                  from_wkt<PL>("POLYGON((1 1,1 3,3 3,3 1))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("pg-mpg-03",
                  from_wkt<PL>("POLYGON((3 3,3 4,4 4,4 3))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  true);
}

template <typename P>
inline void test_multipolygon_multipolygon()
{
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;

    typedef test_disjoint tester;

    tester::apply("mpg-mpg-01",
                  from_wkt<MPL>("MULTIPOLYGON(((2 2,2 3,3 3,3 2)))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("mpg-mpg-02",
                  from_wkt<MPL>("MULTIPOLYGON(((1 1,1 3,3 3,3 1)))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  false);

    tester::apply("mpg-mpg-03",
                  from_wkt<MPL>("MULTIPOLYGON(((3 3,3 4,4 4,4 3)))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,2 2,0 2)))"),
                  true);
}

//============================================================================

template <typename CoordinateType>
inline void test_areal_areal()
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;

    test_polygon_polygon<point_type>();
    test_polygon_multipolygon<point_type>();
    test_polygon_ring<point_type>();
    test_polygon_box<point_type>();

    test_multipolygon_multipolygon<point_type>();
    test_multipolygon_ring<point_type>();
    test_multipolygon_box<point_type>();

    test_ring_ring<point_type>();
    test_ring_box<point_type>();

    test_box_box<point_type>();
}

//============================================================================

BOOST_AUTO_TEST_CASE( test_areal_areal_all )
{
    test_areal_areal<double>();
    test_areal_areal<int>();
#ifdef HAVE_TTMATH
    test_areal_areal<ttmath_big>();
#endif
}
