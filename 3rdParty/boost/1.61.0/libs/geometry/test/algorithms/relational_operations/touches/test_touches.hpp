// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013, 2015.
// Modifications copyright (c) 2013, 2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_TOUCHES_HPP
#define BOOST_GEOMETRY_TEST_TOUCHES_HPP


#include <geometry_test_common.hpp>

#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/algorithms/touches.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/geometry/io/wkt/read.hpp>
#include <boost/variant/variant.hpp>

template <typename Geometry1, typename Geometry2>
void check_touches(Geometry1 const& geometry1,
                   Geometry2 const& geometry2,
                   std::string const& wkt1,
                   std::string const& wkt2,
                   bool expected)
{
    bool detected = bg::touches(geometry1, geometry2);

    BOOST_CHECK_MESSAGE(detected == expected,
        "touches: " << wkt1
        << " with " << wkt2
        << " -> Expected: " << expected
        << " detected: " << detected);

    detected = bg::touches(geometry2, geometry1);

    BOOST_CHECK_MESSAGE(detected == expected,
        "touches: " << wkt1
        << " with " << wkt2
        << " -> Expected: " << expected
        << " detected: " << detected);
}

template <typename Geometry1, typename Geometry2>
void test_touches(std::string const& wkt1,
        std::string const& wkt2, bool expected)
{
    Geometry1 geometry1;
    Geometry2 geometry2;

    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);

    boost::variant<Geometry1> v1(geometry1);
    boost::variant<Geometry2> v2(geometry2);

    check_touches(geometry1, geometry2, wkt1, wkt2, expected);
    check_touches(v1, geometry2, wkt1, wkt2, expected);
    check_touches(geometry1, v2, wkt1, wkt2, expected);
    check_touches(v1, v2, wkt1, wkt2, expected);
}


template <typename Geometry>
void check_self_touches(Geometry const& geometry,
                        std::string const& wkt,
                        bool expected)
{
    bool detected = bg::touches(geometry);

    BOOST_CHECK_MESSAGE(detected == expected,
        "touches: " << wkt
        << " -> Expected: " << expected
        << " detected: " << detected);
}

template <typename Geometry>
void test_self_touches(std::string const& wkt, bool expected)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> v(geometry);

    check_self_touches(geometry, wkt, expected);
    check_self_touches(v, wkt, expected);
}



#endif
