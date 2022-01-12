// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014-2020.
// Modifications copyright (c) 2014-2020 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_STRATEGIES_TEST_WITHIN_HPP
#define BOOST_GEOMETRY_TEST_STRATEGIES_TEST_WITHIN_HPP


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/within.hpp>

#include <boost/geometry/strategy/cartesian/side_robust.hpp>

#include <boost/geometry/strategies/cartesian/point_in_poly_franklin.hpp>
#include <boost/geometry/strategies/cartesian/point_in_poly_crossings_multiply.hpp>
#include <boost/geometry/strategies/agnostic/point_in_poly_winding.hpp>
#include <boost/geometry/strategies/cartesian/point_in_box.hpp>
#include <boost/geometry/strategies/cartesian/box_in_box.hpp>
#include <boost/geometry/strategies/agnostic/point_in_box_by_side.hpp>

#include <boost/geometry/strategies/spherical/ssf.hpp>

// TEMP
#include <boost/geometry/strategies/relate/cartesian.hpp>
#include <boost/geometry/strategies/relate/geographic.hpp>
#include <boost/geometry/strategies/relate/spherical.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>


template <typename Strategy>
inline const char * strategy_name(Strategy const&)
{
    return typeid(Strategy).name();
}

template <typename P, typename PoS, typename CT>
inline const char * strategy_name(bg::strategy::within::crossings_multiply<P, PoS, CT> const&)
{
    return "crossings_multiply";
}

template <typename P, typename PoS, typename CT>
inline const char * strategy_name(bg::strategy::within::franklin<P, PoS, CT> const&)
{
    return "franklin";
}

template <typename P, typename PoS, typename CT>
inline const char * strategy_name(bg::strategy::within::winding<P, PoS, CT> const&)
{
    return "winding";
}


template <typename Point, typename Polygon, typename Strategy>
void test_point_in_polygon(std::string const& case_id,
                           Point const& point,
                           Polygon const& polygon,
                           Strategy const& strategy,
                           bool expected,
                           bool use_within = true)
{
    BOOST_CONCEPT_ASSERT( (bg::concepts::WithinStrategyPolygonal<Point, Polygon, Strategy>) );
    bool detected = use_within ?
                    bg::within(point, polygon, strategy) :
                    bg::covered_by(point, polygon, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
            (use_within ? "within: " : "covered_by: ") << case_id
            << " strategy: " << strategy_name(strategy)
            << " output expected: " << int(expected)
            << " detected: " << int(detected)
            );
}


template <typename Point, typename Polygon, typename Strategy>
void test_geometry(std::string const& case_id,
                   std::string const& wkt_point,
                   std::string const& wkt_polygon,
                   Strategy const& strategy,
                   bool expected,
                   bool use_within = true)
{
    Point point;
    Polygon polygon;
    bg::read_wkt(wkt_point, point);
    bg::read_wkt(wkt_polygon, polygon);

    test_point_in_polygon(case_id, point, polygon, strategy, expected, use_within);
}


#endif // BOOST_GEOMETRY_TEST_STRATEGIES_TEST_WITHIN_HPP
