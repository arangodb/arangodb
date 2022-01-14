// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


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

// pointlike-areal geometries
template <typename P>
inline void test_point_box()
{
    typedef test_disjoint tester;
    typedef bg::model::box<P> B;

    tester::apply("p-b-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<B>("BOX(0 0,1 1)"),
                  false);

    tester::apply("p-b-02",
                  from_wkt<P>("POINT(2 2)"),
                  from_wkt<B>("BOX(0 0,1 0)"),
                  true);
}

template <typename P>
inline void test_point_ring()
{
    typedef bg::model::ring<P, false, false> R; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("p-r-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<R>("POLYGON((0 0,1 0,0 1))"),
                  false);

    tester::apply("p-r-02",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<R>("POLYGON((0 0,1 0,0 1))"),
                  true);
}

template <typename P>
inline void test_point_polygon()
{
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("p-pg-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<PL>("POLYGON((0 0,1 0,0 1))"),
                  false);

    tester::apply("p-pg-02",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<PL>("POLYGON((0 0,1 0,0 1))"),
                  true);
}

template <typename P>
inline void test_point_multipolygon()
{
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;
    
    typedef test_disjoint tester;

    tester::apply("p-mpg-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,1 0,0 1)),((2 0,3 0,2 1)))"),
                  false);

    tester::apply("p-mpg-02",
                  from_wkt<P>("POINT(1 1)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,1 0,0 1)),((2 0,3 0,2 1)))"),
                  true);
}

template <typename P>
inline void test_multipoint_box()
{
    typedef test_disjoint tester;
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::box<P> B;

    tester::apply("mp-b-01",
                  from_wkt<MP>("MULTIPOINT(0 0,1 1)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("mp-b-02",
                  from_wkt<MP>("MULTIPOINT(1 1,3 3)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  false);

    tester::apply("mp-b-03",
                  from_wkt<MP>("MULTIPOINT(3 3,4 4)"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);

    tester::apply("mp-b-04",
                  from_wkt<MP>("MULTIPOINT()"),
                  from_wkt<B>("BOX(0 0,2 2)"),
                  true);
}

template <typename P>
inline void test_multipoint_ring()
{
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::ring<P, false, false> R; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("mp-r-01",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<R>("POLYGON((0 0,1 0,0 1))"),
                  false);

    tester::apply("mp-r-02",
                  from_wkt<MP>("MULTIPOINT(1 0,1 1)"),
                  from_wkt<R>("POLYGON((0 0,1 0,0 1))"),
                  false);

    tester::apply("mp-r-03",
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  from_wkt<R>("POLYGON((0 0,1 0,0 1))"),
                  true);
}

template <typename P>
inline void test_multipoint_polygon()
{
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    
    typedef test_disjoint tester;

    tester::apply("mp-pg-01",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<PL>("POLYGON((0 0,1 0,0 1))"),
                  false);

    tester::apply("mp-pg-02",
                  from_wkt<MP>("MULTIPOINT(0 0,2 0)"),
                  from_wkt<PL>("POLYGON((0 0,1 0,0 1))"),
                  false);

    tester::apply("mp-pg-03",
                  from_wkt<MP>("MULTIPOINT(1 1,2 0)"),
                  from_wkt<PL>("POLYGON((0 0,1 0,0 1))"),
                  true);

    tester::apply("mp-pg-04",
                  from_wkt<MP>("MULTIPOINT(1 1,2 3)"),
                  from_wkt<PL>("POLYGON((0 0,1 0,0 1))"),
                  true);
}

template <typename P>
inline void test_multipoint_multipolygon()
{
    typedef bg::model::multi_point<P> MP;
    typedef bg::model::polygon<P, false, false> PL; // ccw, open
    typedef bg::model::multi_polygon<PL> MPL;
    
    typedef test_disjoint tester;

    tester::apply("mp-mp-01",
                  from_wkt<MP>("MULTIPOINT(0 0,2 0)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,1 0,0 1)),((2 0,3 0,2 1)))"),
                  false);

    tester::apply("mp-mp-02",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,1 0,0 1)),((2 0,3 0,2 1)))"),
                  false);

    tester::apply("mp-mp-03",
                  from_wkt<MP>("MULTIPOINT(1 1,2 0)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,1 0,0 1)),((2 0,3 0,2 1)))"),
                  false);

    tester::apply("mp-mp-04",
                  from_wkt<MP>("MULTIPOINT(1 1,2 3)"),
                  from_wkt<MPL>("MULTIPOLYGON(((0 0,1 0,0 1)),((2 0,3 0,2 1)))"),
                  true);
}

//============================================================================

template <typename CoordinateType>
inline void test_pointlike_areal()
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;

    test_point_polygon<point_type>();
    test_point_multipolygon<point_type>();
    test_point_ring<point_type>();
    test_point_box<point_type>();

    test_multipoint_polygon<point_type>();
    test_multipoint_multipolygon<point_type>();
    test_multipoint_ring<point_type>();

    test_multipoint_box<point_type>();
}

//============================================================================

BOOST_AUTO_TEST_CASE( test_pointlike_areal_all )
{
    test_pointlike_areal<double>();
    test_pointlike_areal<int>();
}
