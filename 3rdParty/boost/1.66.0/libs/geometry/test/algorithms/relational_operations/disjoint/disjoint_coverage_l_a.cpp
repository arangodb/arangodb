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

// linear-areal geometries
template <typename P>
inline void test_segment_box()
{
    typedef bg::model::segment<P> S;
    typedef bg::model::box<P> B;
    
    typedef test_disjoint tester;

    tester::apply("s-b-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-02",
                  from_wkt<S>("SEGMENT(1 1,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-03",
                  from_wkt<S>("SEGMENT(2 2,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-04",
                  from_wkt<S>("SEGMENT(4 4,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);

    tester::apply("s-b-05",
                  from_wkt<S>("SEGMENT(0 4,4 4)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);

    tester::apply("s-b-06",
                  from_wkt<S>("SEGMENT(4 0,4 4)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);

    tester::apply("s-b-07",
                  from_wkt<S>("SEGMENT(0 -2,0 -1)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);

    tester::apply("s-b-08",
                  from_wkt<S>("SEGMENT(-2 -2,-2 -1)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);

    tester::apply("s-b-09",
                  from_wkt<S>("SEGMENT(-2 -2,-2 -2)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);

    tester::apply("s-b-10",
                  from_wkt<S>("SEGMENT(-2 0,-2 0)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);

    tester::apply("s-b-11",
                  from_wkt<S>("SEGMENT(0 -2,0 -2)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);

    tester::apply("s-b-12",
                  from_wkt<S>("SEGMENT(-2 0,-1 0)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);

    // segment degenerates to a point
    tester::apply("s-b-13",
                  from_wkt<S>("SEGMENT(0 0,0 0)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  false);

    tester::apply("s-b-14",
                  from_wkt<S>("SEGMENT(1 1,1 1)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-15",
                  from_wkt<S>("SEGMENT(2 2,2 2)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-16",
                  from_wkt<S>("SEGMENT(2 0,2 0)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-17",
                  from_wkt<S>("SEGMENT(0 2,0 2)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("s-b-18",
                  from_wkt<S>("SEGMENT(2 2,2 2)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  true);
}

template <typename P>
inline void test_segment_ring()
{
    typedef bg::model::segment<P> S;
    typedef bg::model::ring<P, false, false> R; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("s-r-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("s-r-02",
                  from_wkt<S>("SEGMENT(1 0,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("s-r-03",
                  from_wkt<S>("SEGMENT(1 1,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("s-r-04",
                  from_wkt<S>("SEGMENT(2 2,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  true);
}

template <typename P>
inline void test_segment_polygon()
{
    typedef bg::model::segment<P> S;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("s-pg-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("s-pg-02",
                  from_wkt<S>("SEGMENT(1 0,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("s-pg-03",
                  from_wkt<S>("SEGMENT(1 1,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("s-pg-04",
                  from_wkt<S>("SEGMENT(2 2,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  true);
}

template <typename P>
inline void test_segment_multipolygon()
{
    typedef bg::model::segment<P> S;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;
    
    typedef test_disjoint tester;

    tester::apply("s-mpg-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("s-mpg-02",
                  from_wkt<S>("SEGMENT(1 0,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("s-mpg-03",
                  from_wkt<S>("SEGMENT(1 1,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("s-mpg-04",
                  from_wkt<S>("SEGMENT(2 2,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  true);
}

template <typename P>
inline void test_linestring_box()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::box<P> B;
    
    typedef test_disjoint tester;

    tester::apply("l-b-01",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("l-b-02",
                  from_wkt<L>("LINESTRING(1 1,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("l-b-03",
                  from_wkt<L>("LINESTRING(2 2,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("l-b-04",
                  from_wkt<L>("LINESTRING(4 4,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);
}

template <typename P>
inline void test_linestring_ring()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::ring<P, false, false> R; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("l-r-01",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("l-r-02",
                  from_wkt<L>("LINESTRING(1 0,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("l-r-03",
                  from_wkt<L>("LINESTRING(1 1,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("l-r-04",
                  from_wkt<L>("LINESTRING(2 2,3 3)"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  true);
}

template <typename P>
inline void test_linestring_polygon()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("l-pg-01",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("l-pg-02",
                  from_wkt<L>("LINESTRING(1 0,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("l-pg-03",
                  from_wkt<L>("LINESTRING(1 1,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("l-pg-04",
                  from_wkt<L>("LINESTRING(2 2,3 3)"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  true);
}

template <typename P>
inline void test_linestring_multipolygon()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;

    typedef test_disjoint tester;

    tester::apply("l-mpg-01",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("l-mpg-02",
                  from_wkt<L>("LINESTRING(1 0,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("l-mpg-03",
                  from_wkt<L>("LINESTRING(1 1,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("l-mpg-04",
                  from_wkt<L>("LINESTRING(2 2,3 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  true);
}

template <typename P>
inline void test_multilinestring_box()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    typedef bg::model::box<P> B;

    typedef test_disjoint tester;

    tester::apply("ml-b-01",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("ml-b-02",
                  from_wkt<ML>("MULTILINESTRING((1 1,3 3))"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("ml-b-03",
                  from_wkt<ML>("MULTILINESTRING((2 2,3 3))"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("ml-b-04",
                  from_wkt<ML>("MULTILINESTRING((4 4,3 3))"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);
}

template <typename P>
inline void test_multilinestring_ring()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    typedef bg::model::ring<P, false, false> R; // ccw, open

    typedef test_disjoint tester;

    tester::apply("ml-r-01",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("ml-r-02",
                  from_wkt<ML>("MULTILINESTRING((1 0,3 3))"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("ml-r-03",
                  from_wkt<ML>("MULTILINESTRING((1 1,3 3))"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("ml-r-04",
                  from_wkt<ML>("MULTILINESTRING((2 2,3 3))"),
                  from_wkt<R>("POLYGON((0 0,2 0,0 2))"),
                  true);
}

template <typename P>
inline void test_multilinestring_polygon()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open

    typedef test_disjoint tester;

    tester::apply("ml-pg-01",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("ml-pg-02",
                  from_wkt<ML>("MULTILINESTRING((1 0,3 3))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("ml-pg-03",
                  from_wkt<ML>("MULTILINESTRING((1 1,3 3))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  false);

    tester::apply("ml-pg-04",
                  from_wkt<ML>("MULTILINESTRING((2 2,3 3))"),
                  from_wkt<PL>("POLYGON((0 0,2 0,0 2))"),
                  true);
}

template <typename P>
inline void test_multilinestring_multipolygon()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;

    typedef test_disjoint tester;

    tester::apply("ml-mpg-01",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("ml-mpg-02",
                  from_wkt<ML>("MULTILINESTRING((1 0,3 3))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("ml-mpg-03",
                  from_wkt<ML>("MULTILINESTRING((1 1,3 3))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  false);

    tester::apply("ml-mpg-04",
                  from_wkt<ML>("MULTILINESTRING((2 2,3 3))"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,2 0,0 2)))"),
                  true);
}

//============================================================================

template <typename CoordinateType>
inline void test_linear_areal()
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;

    test_segment_polygon<point_type>();
    test_segment_multipolygon<point_type>();
    test_segment_ring<point_type>();
    test_segment_box<point_type>();

    test_linestring_polygon<point_type>();
    test_linestring_multipolygon<point_type>();
    test_linestring_ring<point_type>();
    test_linestring_box<point_type>();

    test_multilinestring_polygon<point_type>();
    test_multilinestring_multipolygon<point_type>();
    test_multilinestring_ring<point_type>();
    test_multilinestring_box<point_type>();
}

//============================================================================

BOOST_AUTO_TEST_CASE( test_linear_areal_all )
{
    test_linear_areal<double>();
    test_linear_areal<int>();
#ifdef HAVE_TTMATH
    test_linear_areal<ttmath_big>();
#endif
}
