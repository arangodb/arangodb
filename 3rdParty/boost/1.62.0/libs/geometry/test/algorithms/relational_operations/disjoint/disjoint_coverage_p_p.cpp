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

// pointlike-pointlike geometries
template <typename P>
inline void test_point_point()
{
    typedef test_disjoint tester;

    tester::apply("p-p-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<P>("POINT(0 0)"),
                  false);

    tester::apply("p-p-02",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<P>("POINT(1 1)"),
                  true);
}

template <typename P>
inline void test_point_multipoint()
{
    typedef bg::model::multi_point<P> MP;
    
    typedef test_disjoint tester;

    tester::apply("p-mp-01",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<MP>("MULTIPOINT(0 0,1 1)"),
                  false);

    tester::apply("p-mp-02",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  true);

    tester::apply("p-mp-03",
                  from_wkt<P>("POINT(0 0)"),
                  from_wkt<MP>("MULTIPOINT()"),
                  true);
}

template <typename P>
inline void test_multipoint_multipoint()
{
    typedef bg::model::multi_point<P> MP;
    
    typedef test_disjoint tester;

    tester::apply("mp-mp-01",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<MP>("MULTIPOINT(0 0,1 1)"),
                  false);

    tester::apply("mp-mp-02",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  true);

    tester::apply("mp-mp-03",
                  from_wkt<MP>("MULTIPOINT()"),
                  from_wkt<MP>("MULTIPOINT(1 1,2 2)"),
                  true);

    tester::apply("mp-mp-04",
                  from_wkt<MP>("MULTIPOINT(0 0,1 0)"),
                  from_wkt<MP>("MULTIPOINT()"),
                  true);
}

//============================================================================

template <typename CoordinateType>
inline void test_pointlike_pointlike()
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;

    test_point_point<point_type>();
    test_point_multipoint<point_type>();

    test_multipoint_multipoint<point_type>();
}

//============================================================================

BOOST_AUTO_TEST_CASE( test_pointlike_pointlike_all )
{
    test_pointlike_pointlike<double>();
    test_pointlike_pointlike<int>();
#ifdef HAVE_TTMATH
    test_pointlike_pointlike<ttmath_big>();
#endif
}
