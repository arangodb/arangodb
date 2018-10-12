// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2017.
// Modifications copyright (c) 2017 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_DISJOINT_HPP
#define BOOST_GEOMETRY_TEST_DISJOINT_HPP

#include <iostream>
#include <string>
#include <boost/variant/variant.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/io/wkt/read.hpp>


struct no_strategy {};

template <typename Geometry1, typename Geometry2, typename Strategy>
bool call_disjoint(Geometry1 const& geometry1,
                   Geometry2 const& geometry2,
                   Strategy const& strategy)
{
    return bg::disjoint(geometry1, geometry2, strategy);
}

template <typename Geometry1, typename Geometry2>
bool call_disjoint(Geometry1 const& geometry1,
                   Geometry2 const& geometry2,
                   no_strategy)
{
    return bg::disjoint(geometry1, geometry2);
}

template <typename G1, typename G2, typename Strategy>
void check_disjoint(std::string const& id,
                    std::string const& wkt1,
                    std::string const& wkt2,
                    G1 const& g1,
                    G2 const& g2,
                    bool expected,
                    Strategy const& strategy)
{
    bool detected = call_disjoint(g1, g2, strategy);
    if (id.empty())
    {
        BOOST_CHECK_MESSAGE(detected == expected,
            "disjoint: " << wkt1 << " and " << wkt2
            << " -> Expected: " << expected
            << " detected: " << detected);
    }
    else
    {
        BOOST_CHECK_MESSAGE(detected == expected,
            "disjoint: " << id
            << " -> Expected: " << expected
            << " detected: " << detected);
    }
}

template <typename G1, typename G2>
void test_disjoint(std::string const& id,
                   std::string const& wkt1,
                   std::string const& wkt2,
                   bool expected)
{
    G1 g1;
    bg::read_wkt(wkt1, g1);

    G2 g2;
    bg::read_wkt(wkt2, g2);

    boost::variant<G1> v1(g1);
    boost::variant<G2> v2(g2);

    typedef typename bg::strategy::disjoint::services::default_strategy
        <
            G1, G2
        >::type strategy_type;

    check_disjoint(id, wkt1, wkt2, g1, g2, expected, no_strategy());
    check_disjoint(id, wkt1, wkt2, g1, g2, expected, strategy_type());
    check_disjoint(id, wkt1, wkt2, g1, g2, expected, no_strategy());
    check_disjoint(id, wkt1, wkt2, v1, g2, expected, no_strategy());
    check_disjoint(id, wkt1, wkt2, g1, v2, expected, no_strategy());
    check_disjoint(id, wkt1, wkt2, v1, v2, expected, no_strategy());
}

template <typename G1, typename G2>
void test_geometry(std::string const& wkt1,
                   std::string const& wkt2,
                   bool expected)
{
    test_disjoint<G1, G2>("", wkt1, wkt2, expected);
}

#endif // BOOST_GEOMETRY_TEST_DISJOINT_HPP
