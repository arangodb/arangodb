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

// pointlike-linear geometries
template <typename P>
inline void test_point_segment()
{
    typedef test_disjoint tester;
    typedef bg::model::segment<P> S;

    tester::apply("p-s-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  false);

    tester::apply("p-s-02",
                  from_wkt<P>("POINT(2 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  false);

    tester::apply("p-s-03",
                  from_wkt<P>("POINT(1 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  false);

    tester::apply("p-s-04",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  true);

    tester::apply("p-s-05",
                  from_wkt<P>("POINT(3 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  true);

    tester::apply("p-s-06",
                  from_wkt<P>("POINT(-1 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  true);

    // degenerate segment
    tester::apply("p-s-07",
                  from_wkt<P>("POINT(-1 0)"),
                  from_wkt<S>("SEGMENT(2 0,2 0)"),
                  true);

    // degenerate segment
    tester::apply("p-s-08",
                  from_wkt<P>("POINT(2 0)"),
                  from_wkt<S>("SEGMENT(2 0,2 0)"),
                  false);

    // degenerate segment
    tester::apply("p-s-09",
                  from_wkt<P>("POINT(3 0)"),
                  from_wkt<S>("SEGMENT(2 0,2 0)"),
                  true);

    // degenerate segment
    tester::apply("p-s-10",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<S>("SEGMENT(2 0,2 0)"),
                  true);
}

template <typename P>
inline void test_point_linestring()
{
    typedef bg::model::linestring<P> L;
    
    typedef test_disjoint tester;

    tester::apply("p-l-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  false);

    tester::apply("p-l-02",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  false);

    tester::apply("p-l-03",
                  from_wkt<P>("POINT(3 3)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  false);

    tester::apply("p-l-04",
                  from_wkt<P>("POINT(1 0)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  true);

    tester::apply("p-l-05",
                  from_wkt<P>("POINT(5 5)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  true);

    tester::apply("p-l-06",
                  from_wkt<P>("POINT(5 5)"),
                  from_wkt<L>("LINESTRING(0 0,2 2)"),
                  true);
}

template <typename P>
inline void test_point_multilinestring()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    
    typedef test_disjoint tester;

    tester::apply("p-ml-01",
                  from_wkt<P>("POINT(0 1)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  true);

    tester::apply("p-ml-02",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("p-ml-03",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("p-ml-04",
                  from_wkt<P>("POINT(1 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("p-ml-05",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 4),(3 0,4 0))"),
                  true);

    tester::apply("p-ml-06",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 4),(0 0,4 0))"),
                  false);

    tester::apply("p-ml-07",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 4),(-1 0,4 0))"),
                  false);
}

template <typename P>
inline void test_multipoint_segment()
{
    typedef test_disjoint tester;
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::segment<P> S;

    tester::apply("mp-s-01",
                  from_wkt<MP>("MULTIPOINT(0 0,1 1)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  false);

    tester::apply("mp-s-02",
                  from_wkt<MP>("MULTIPOINT(1 0,1 1)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  false);

    tester::apply("mp-s-03",
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  true);

    tester::apply("mp-s-04",
                  from_wkt<MP>("MULTIPOINT()"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  true);

    tester::apply("mp-s-05",
                  from_wkt<MP>("MULTIPOINT(3 0,4 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  true);

    tester::apply("mp-s-06",
                  from_wkt<MP>("MULTIPOINT(1 0,4 0)"),
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  false);

    // segments that degenerate to a point
    tester::apply("mp-s-07",
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  from_wkt<S>("SEGMENT(0 0,0 0)"),
                  true);

    tester::apply("mp-s-08",
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  from_wkt<S>("SEGMENT(1 1,1 1)"),
                  false);
}

template <typename P>
inline void test_multipoint_linestring()
{
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::linestring<P> L;
    
    typedef test_disjoint tester;

    tester::apply("mp-l-01",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  false);

    tester::apply("mp-l-02",
                  from_wkt<MP>("MULTIPOINT(1 0,1 1)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  false);

    tester::apply("mp-l-03",
                  from_wkt<MP>("MULTIPOINT(1 0,3 3)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  false);

    tester::apply("mp-l-04",
                  from_wkt<MP>("MULTIPOINT(1 0,2 0)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  true);

    tester::apply("mp-l-05",
                  from_wkt<MP>("MULTIPOINT(-1 -1,2 0)"),
                  from_wkt<L>("LINESTRING(0 0,2 2,4 4)"),
                  true);

    tester::apply("mp-l-06",
                  from_wkt<MP>("MULTIPOINT(-1 -1,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,3 0)"),
                  false);

    tester::apply("mp-l-07",
                  from_wkt<MP>("MULTIPOINT(-1 -1,2 0,-1 -1,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,3 0)"),
                  false);

    tester::apply("mp-l-08",
                  from_wkt<MP>("MULTIPOINT(2 0)"),
                  from_wkt<L>("LINESTRING(1 0)"),
                  true);

    tester::apply("mp-l-09",
                  from_wkt<MP>("MULTIPOINT(3 0,0 0,3 0)"),
                  from_wkt<L>("LINESTRING(1 0,2 0)"),
                  true);
}

template <typename P>
inline void test_multipoint_multilinestring()
{
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    
    typedef test_disjoint tester;

    tester::apply("mp-ml-01",
                  from_wkt<MP>("MULTIPOINT(0 1,0 2)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  true);

    tester::apply("mp-ml-02",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("mp-ml-03",
                  from_wkt<MP>("MULTIPOINT(0 1,1 1)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("mp-ml-04",
                  from_wkt<MP>("MULTIPOINT(0 1,1 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("mp-ml-05",
                  from_wkt<MP>("MULTIPOINT(0 0,10 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);

    tester::apply("mp-ml-06",
                  from_wkt<MP>("MULTIPOINT(-1 0,3 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,2 2,4 4),(0 0,2 0,4 0))"),
                  false);
}

//============================================================================

template <typename CoordinateType>
inline void test_pointlike_linear()
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;

    test_point_linestring<point_type>();
    test_point_multilinestring<point_type>();
    test_point_segment<point_type>();

    test_multipoint_linestring<point_type>();
    test_multipoint_multilinestring<point_type>();
    test_multipoint_segment<point_type>();
}

//============================================================================

BOOST_AUTO_TEST_CASE( test_pointlike_linear_all )
{
    test_pointlike_linear<double>();
    test_pointlike_linear<int>();
}
