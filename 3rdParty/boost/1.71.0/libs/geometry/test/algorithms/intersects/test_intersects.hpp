// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2017.
// Modifications copyright (c) 2017 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_INTERSECTS_HPP
#define BOOST_GEOMETRY_TEST_INTERSECTS_HPP


#include <geometry_test_common.hpp>


#include <boost/geometry/core/geometry_id.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/read.hpp>


struct no_strategy {};

template <typename Geometry1, typename Geometry2, typename Strategy>
bool call_intersects(Geometry1 const& geometry1,
                     Geometry2 const& geometry2,
                     Strategy const& strategy)
{
    return bg::intersects(geometry1, geometry2, strategy);
}

template <typename Geometry1, typename Geometry2>
bool call_intersects(Geometry1 const& geometry1,
                     Geometry2 const& geometry2,
                     no_strategy)
{
    return bg::intersects(geometry1, geometry2);
}

template <typename G1, typename G2, typename Strategy>
void check_intersects(std::string const& wkt1,
                      std::string const& wkt2,
                      G1 const& g1,
                      G2 const& g2,
                      bool expected,
                      Strategy const& strategy)
{
    bool detected = call_intersects(g1, g2, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
        "intersects: " << wkt1
        << " with " << wkt2
        << " -> Expected: " << expected
        << " detected: " << detected);
}

template <typename Geometry1, typename Geometry2>
void test_geometry(std::string const& wkt1,
        std::string const& wkt2, bool expected)
{
    Geometry1 geometry1;
    Geometry2 geometry2;

    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);

    check_intersects(wkt1, wkt2, geometry1, geometry2, expected, no_strategy());
    check_intersects(wkt2, wkt1, geometry2, geometry1, expected, no_strategy());

    typedef typename bg::strategy::disjoint::services::default_strategy
        <
            Geometry1, Geometry2
        >::type strategy12_type;
    typedef typename bg::strategy::disjoint::services::default_strategy
        <
            Geometry2, Geometry1
        >::type strategy21_type;

    check_intersects(wkt1, wkt2, geometry1, geometry2, expected, strategy12_type());
    check_intersects(wkt2, wkt1, geometry2, geometry1, expected, strategy21_type());
}


template <typename Geometry>
void test_self_intersects(std::string const& wkt, bool expected)
{
    Geometry geometry;

    bg::read_wkt(wkt, geometry);

    bool detected = bg::intersects(geometry);

    BOOST_CHECK_MESSAGE(detected == expected,
        "intersects: " << wkt
        << " -> Expected: " << expected
        << " detected: " << detected);
}



#endif
