// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2014 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2014 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_STRATEGIES_TEST_PROJECTED_POINT_HPP
#define BOOST_GEOMETRY_TEST_STRATEGIES_TEST_PROJECTED_POINT_HPP

#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>

#include <boost/geometry/strategies/cartesian/distance_projected_point.hpp>
#include <boost/geometry/strategies/cartesian/distance_projected_point_ax.hpp>
#include <boost/geometry/strategies/concepts/distance_concept.hpp>

#include <boost/geometry/io/wkt/read.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <test_common/test_point.hpp>

#ifdef HAVE_TTMATH
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)


template <typename P, typename PS, typename CalculationType>
void test_services()
{
    PS p1, p2;
    bg::assign_values(p1, 0, 0);
    bg::assign_values(p2, 0, 4);

    P p;
    bg::assign_values(p, 2, 0);

    CalculationType const sqr_expected = 4;
    CalculationType const expected = 2;


    namespace bgsd = bg::strategy::distance;
    namespace services = bg::strategy::distance::services;

    {
        // compile-check if there is a strategy for this type
        typedef typename services::default_strategy
            <
                bg::point_tag, bg::segment_tag, P, PS
            >::type projected_point_strategy_type;

        typedef typename services::default_strategy
            <
                bg::segment_tag, bg::point_tag, PS, P
            >::type reversed_tags_projected_point_strategy_type;

        boost::ignore_unused<projected_point_strategy_type,
                             reversed_tags_projected_point_strategy_type>();
    }

    // 1: normal, calculate distance:

    typedef bgsd::projected_point<CalculationType> strategy_type;

    BOOST_CONCEPT_ASSERT( (bg::concepts::PointSegmentDistanceStrategy<strategy_type, P, PS>) );

    typedef typename services::return_type<strategy_type, P, PS>::type return_type;

    strategy_type strategy;
    return_type result = strategy.apply(p, p1, p2);
    BOOST_CHECK_CLOSE(result, return_type(expected), 0.001);

    // 2: the strategy should return the same result if we reverse parameters
    result = strategy.apply(p, p2, p1);
    BOOST_CHECK_CLOSE(result, return_type(expected), 0.001);


    // 3: "comparable" to construct a "comparable strategy" for P1/P2
    //    a "comparable strategy" is a strategy which does not calculate the exact distance, but
    //    which returns results which can be mutually compared (e.g. avoid sqrt)

    // 3a: "comparable_type"
    typedef typename services::comparable_type<strategy_type>::type comparable_type;

    // 3b: "get_comparable"
    comparable_type comparable = bgsd::services::get_comparable<strategy_type>::apply(strategy);

    return_type c_result = comparable.apply(p, p1, p2);
    BOOST_CHECK_CLOSE(c_result, return_type(sqr_expected), 0.001);
}

template <typename T1, typename T2>
void test_check_close(T1 const& v1, T2 const& v2, double f)
{
    BOOST_CHECK_CLOSE(v1, v2, f);
}

template <typename T1, typename T2>
void test_check_close(bg::strategy::distance::detail::projected_point_ax_result<T1> const& v1,
                      bg::strategy::distance::detail::projected_point_ax_result<T2> const& v2,
                      double f)
{
    BOOST_CHECK_CLOSE(v1.atd, v2.atd, f);
    BOOST_CHECK_CLOSE(v1.xtd, v2.xtd, f);
}

template <typename P1, typename P2, typename T, typename Strategy, typename ComparableStrategy>
void test_2d(std::string const& wkt_p,
             std::string const& wkt_sp1,
             std::string const& wkt_sp2,
             T expected_distance,
             T expected_comparable_distance,
             Strategy strategy,
             ComparableStrategy comparable_strategy)
{
    P1 p;
    P2 sp1, sp2;
    bg::read_wkt(wkt_p, p);
    bg::read_wkt(wkt_sp1, sp1);
    bg::read_wkt(wkt_sp2, sp2);

    BOOST_CONCEPT_ASSERT
        (
            (bg::concepts::PointSegmentDistanceStrategy<Strategy, P1, P2>)
        );
    BOOST_CONCEPT_ASSERT
        (
            (bg::concepts::PointSegmentDistanceStrategy<ComparableStrategy, P1, P2>)
        );

    {
        typedef typename bg::strategy::distance::services::return_type<Strategy, P1, P2>::type return_type;
        return_type d = strategy.apply(p, sp1, sp2);
        test_check_close(d, expected_distance, 0.001);
    }

    // Test combination with the comparable strategy
    {
        typedef typename bg::strategy::distance::services::return_type<ComparableStrategy, P1, P2>::type return_type;
        return_type d = comparable_strategy.apply(p, sp1, sp2);
        test_check_close(d, expected_comparable_distance, 0.01);
    }

}

template <typename P1, typename P2, typename T>
void test_2d(std::string const& wkt_p,
             std::string const& wkt_sp1,
             std::string const& wkt_sp2,
             T expected_distance)
{
    typedef bg::strategy::distance::projected_point<> strategy_type;
    typedef bg::strategy::distance::projected_point
        <
            void,
            bg::strategy::distance::comparable::pythagoras<>
        > comparable_strategy_type;

    strategy_type strategy;
    comparable_strategy_type comparable_strategy;

    T expected_squared_distance = expected_distance * expected_distance;
    test_2d<P1, P2>(wkt_p, wkt_sp1, wkt_sp2, expected_distance, expected_squared_distance, strategy, comparable_strategy);
}

#endif // BOOST_GEOMETRY_TEST_STRATEGIES_TEST_PROJECTED_POINT_HPP
