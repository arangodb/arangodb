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

// linear-linear geometries
template <typename P>
inline void test_segment_segment()
{
    typedef bg::model::segment<P> S;
    
    typedef test_disjoint tester;

    tester::apply("s-s-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<S>("SEGMENT(0 0,0 2)"),
                  false);

    tester::apply("s-s-02",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<S>("SEGMENT(2 0,3 0)"),
                  false);

    tester::apply("s-s-03",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<S>("SEGMENT(1 0,3 0)"),
                  false);

    tester::apply("s-s-04",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<S>("SEGMENT(1 0,1 1)"),
                  false);

    tester::apply("s-s-05",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<S>("SEGMENT(1 1,2 2)"),
                  true);

    tester::apply("s-s-06",
                  from_wkt<S>("SEGMENT(0 0,1 1)"),
                  from_wkt<S>("SEGMENT(1 1,1 1)"),
                  false);

    tester::apply("s-s-07",
                  from_wkt<S>("SEGMENT(0 0,1 1)"),
                  from_wkt<S>("SEGMENT(2 2,2 2)"),
                  true);

    tester::apply("s-s-08",
                  from_wkt<S>("SEGMENT(0 0,1 1)"),
                  from_wkt<S>("SEGMENT(2 2,3 3)"),
                  true);
}

template <typename P>
inline void test_linestring_segment()
{
    typedef bg::model::segment<P> S;
    typedef bg::model::linestring<P> L;
    
    typedef test_disjoint tester;

    tester::apply("l-s-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(0 0,0 2)"),
                  false);

    tester::apply("l-s-02",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(2 0,3 0)"),
                  false);

    tester::apply("l-s-03",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,3 0)"),
                  false);

    tester::apply("l-s-04",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,1 1)"),
                  false);

    tester::apply("l-s-05",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 1,2 2)"),
                  true);

    tester::apply("l-s-06",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 1,1 1,2 2)"),
                  true);

    tester::apply("l-s-07",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,1 0,1 1,2 2)"),
                  false);

    tester::apply("l-s-08",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,1 0,3 0)"),
                  false);

    tester::apply("l-s-09",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(3 0,3 0,4 0)"),
                  true);

    tester::apply("l-s-10",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(3 0,3 0)"),
                  true);

    tester::apply("l-s-11",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(-1 0,-1 0)"),
                  true);

    tester::apply("l-s-12",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,1 0)"),
                  false);

    tester::apply("l-s-13",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 1,1 1)"),
                  true);
}

template <typename P>
inline void test_multilinestring_segment()
{
    typedef bg::model::segment<P> S;
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    
    typedef test_disjoint tester;

    tester::apply("s-ml-01",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,0 2))"),
                  false);

    tester::apply("s-ml-02",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((2 0,3 0))"),
                  false);

    tester::apply("s-ml-03",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 0,3 0))"),
                  false);

    tester::apply("s-ml-04",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 0,1 1))"),
                  false);

    tester::apply("s-ml-05",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2))"),
                  true);

    tester::apply("s-ml-06",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2),(3 3,3 3))"),
                  true);

    tester::apply("s-ml-07",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2),(1 0,1 0))"),
                  false);

    tester::apply("s-ml-08",
                  from_wkt<S>("SEGMENT(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2),(3 0,3 0))"),
                  true);
}

template <typename P>
inline void test_linestring_linestring()
{
    typedef bg::model::linestring<P> L;
    
    typedef test_disjoint tester;

    tester::apply("l-l-01",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(0 0,0 2)"),
                  false);

    tester::apply("l-l-02",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(2 0,3 0)"),
                  false);

    tester::apply("l-l-03",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,3 0)"),
                  false);

    tester::apply("l-l-04",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 0,1 1)"),
                  false);

    tester::apply("l-l-05",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<L>("LINESTRING(1 1,2 2)"),
                  true);
}

template <typename P>
inline void test_linestring_multilinestring()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    
    typedef test_disjoint tester;

    tester::apply("l-ml-01",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((0 0,0 2))"),
                  false);

    tester::apply("l-ml-02",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((2 0,3 0))"),
                  false);

    tester::apply("l-ml-03",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 0,3 0))"),
                  false);

    tester::apply("l-ml-04",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 0,1 1))"),
                  false);

    tester::apply("l-ml-05",
                  from_wkt<L>("LINESTRING(0 0,2 0)"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2))"),
                  true);
}

template <typename P>
inline void test_multilinestring_multilinestring()
{
    typedef bg::model::linestring<P> L;
    typedef bg::model::multi_linestring<L> ML;
    
    typedef test_disjoint tester;

    tester::apply("ml-ml-01",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<ML>("MULTILINESTRING((0 0,0 2))"),
                  false);

    tester::apply("ml-ml-02",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<ML>("MULTILINESTRING((2 0,3 0))"),
                  false);

    tester::apply("ml-ml-03",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<ML>("MULTILINESTRING((1 0,3 0))"),
                  false);

    tester::apply("ml-ml-04",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<ML>("MULTILINESTRING((1 0,1 1))"),
                  false);

    tester::apply("ml-ml-05",
                  from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
                  from_wkt<ML>("MULTILINESTRING((1 1,2 2))"),
                  true);
}

//============================================================================

template <typename CoordinateType>
inline void test_linear_linear()
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;

    test_linestring_linestring<point_type>();
    test_linestring_multilinestring<point_type>();
    test_linestring_segment<point_type>();

    test_multilinestring_multilinestring<point_type>();
    test_multilinestring_segment<point_type>();

    test_segment_segment<point_type>();
}

//============================================================================

BOOST_AUTO_TEST_CASE( test_linear_linear_all )
{
    test_linear_linear<double>();
    test_linear_linear<int>();
#ifdef HAVE_TTMATH
    test_linear_linear<ttmath_big>();
#endif
}
