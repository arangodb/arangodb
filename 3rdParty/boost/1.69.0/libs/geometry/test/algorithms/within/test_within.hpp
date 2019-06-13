// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014, 2015, 2017.
// Modifications copyright (c) 2014-2017 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_WITHIN_HPP
#define BOOST_GEOMETRY_TEST_WITHIN_HPP


#include <boost/variant/variant.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/strategies/strategies.hpp>

struct no_strategy {};

template <typename Geometry1, typename Geometry2, typename Strategy>
bool call_within(Geometry1 const& geometry1,
                 Geometry2 const& geometry2,
                 Strategy const& strategy)
{
    return bg::within(geometry1, geometry2, strategy);
}

template <typename Geometry1, typename Geometry2>
bool call_within(Geometry1 const& geometry1,
                 Geometry2 const& geometry2,
                 no_strategy)
{
    return bg::within(geometry1, geometry2);
}

template <typename Geometry1, typename Geometry2, typename Strategy>
void check_geometry(Geometry1 const& geometry1,
                    Geometry2 const& geometry2,
                    std::string const& wkt1,
                    std::string const& wkt2,
                    bool expected,
                    Strategy const& strategy)
{
    bool detected = call_within(geometry1, geometry2, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
        "within: " << wkt1
        << " in " << wkt2
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
    boost::variant<Geometry1> v1(geometry1);
    boost::variant<Geometry2> v2(geometry2);

    typedef typename bg::strategy::within::services::default_strategy
        <
            Geometry1, Geometry2
        >::type strategy_type;

    check_geometry(geometry1, geometry2, wkt1, wkt2, expected, strategy_type());
    check_geometry(geometry1, geometry2, wkt1, wkt2, expected, no_strategy());
    check_geometry(v1, geometry2, wkt1, wkt2, expected, no_strategy());
    check_geometry(geometry1, v2, wkt1, wkt2, expected, no_strategy());
    check_geometry(v1, v2, wkt1, wkt2, expected, no_strategy());
}

template <typename Point, bool Clockwise, bool Closed>
void test_ordered_ring(std::string const& wkt_point,
        std::string const& wkt_geometry, bool expected, bool on_border)
{
    typedef bg::model::ring<Point, Clockwise, Closed> ring_type;
    ring_type ring;
    Point point;

    bg::read_wkt(wkt_geometry, ring);
    if ( BOOST_GEOMETRY_CONDITION(! Clockwise) )
    {
        std::reverse(boost::begin(ring), boost::end(ring));
    }

    bg::read_wkt(wkt_point, point);

    bool detected = bg::within(point, ring);

    BOOST_CHECK_MESSAGE(detected == expected,
        "within: " << wkt_point
        << " in " << wkt_geometry
        << " -> Expected: " << expected
        << " detected: " << detected
        << " clockwise: " << int(Clockwise)
        << " closed: " << int(Closed)
        );

    // other strategy (note that this one cannot detect OnBorder
    // (without modifications)

    bg::strategy::within::franklin<Point> franklin;
    detected = bg::within(point, ring, franklin);
    if (! on_border)
    {
        BOOST_CHECK_MESSAGE(detected == expected,
            "within: " << wkt_point
            << " in " << wkt_geometry
            << " -> Expected: " << expected
            << " detected: " << detected
            << " clockwise: " << int(Clockwise)
            << " closed: " << int(Closed)
            );
    }


    bg::strategy::within::crossings_multiply<Point> cm;
    detected = bg::within(point, ring, cm);
    if (! on_border)
    {
        BOOST_CHECK_MESSAGE(detected == expected,
            "within: " << wkt_point
            << " in " << wkt_geometry
            << " -> Expected: " << expected
            << " detected: " << detected
            << " clockwise: " << int(Clockwise)
            << " closed: " << int(Closed)
            );
    }
}

template <typename Point>
void test_ring(std::string const& wkt_point,
        std::string const& wkt_geometry,
        bool expected, bool on_border)
{
    test_ordered_ring<Point, true, true>(wkt_point, wkt_geometry, expected, on_border);
    test_ordered_ring<Point, false, true>(wkt_point, wkt_geometry, expected, on_border);
    test_ordered_ring<Point, true, false>(wkt_point, wkt_geometry, expected, on_border);
    test_ordered_ring<Point, false, false>(wkt_point, wkt_geometry, expected, on_border);
    test_geometry<Point, bg::model::polygon<Point> >(wkt_point, wkt_geometry, expected);
}

#endif
