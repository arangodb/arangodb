// Generic Geometry2 Library
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2015, 2017.
// Modifications copyright (c) 2015-2017 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_OVERLAPS_HPP
#define BOOST_GEOMETRY_TEST_OVERLAPS_HPP


#include <geometry_test_common.hpp>

#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/algorithms/overlaps.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/geometry/io/wkt/read.hpp>


struct no_strategy {};

template <typename Geometry1, typename Geometry2, typename Strategy>
bool call_overlaps(Geometry1 const& geometry1,
                   Geometry2 const& geometry2,
                   Strategy const& strategy)
{
    return bg::overlaps(geometry1, geometry2, strategy);
}

template <typename Geometry1, typename Geometry2>
bool call_overlaps(Geometry1 const& geometry1,
                   Geometry2 const& geometry2,
                   no_strategy)
{
    return bg::overlaps(geometry1, geometry2);
}

template <typename Geometry1, typename Geometry2, typename Strategy>
void test_geometry(Geometry1 const& geometry1,
                   Geometry2 const& geometry2,
                   std::string const& wkt1,
                   std::string const& wkt2,
                   bool expected,
                   Strategy const& strategy)
{
    bool detected = call_overlaps(geometry1, geometry2, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
        "overlaps: " << wkt1
        << " with " << wkt2
        << " -> Expected: " << expected
        << " detected: " << detected);

    detected = call_overlaps(geometry2, geometry1, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
        "overlaps: " << wkt2
        << " with " << wkt1
        << " -> Expected: " << expected
        << " detected: " << detected);
}

template <typename Geometry1, typename Geometry2>
void test_geometry(std::string const& wkt1,
                   std::string const& wkt2,
                   bool expected)
{
    Geometry1 geometry1;
    Geometry2 geometry2;

    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);

    test_geometry(geometry1, geometry2, wkt1, wkt2, expected, no_strategy());

    typedef typename bg::strategy::relate::services::default_strategy
        <
            Geometry1, Geometry2
        >::type strategy_type;

    test_geometry(geometry1, geometry2, wkt1, wkt2, expected, strategy_type());
}


#endif
