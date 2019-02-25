// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014-2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2016-2017.
// Modifications copyright (c) 2016-2017 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_EQUALS_HPP
#define BOOST_GEOMETRY_TEST_EQUALS_HPP


#include <geometry_test_common.hpp>

#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/variant/variant.hpp>


struct no_strategy {};

template <typename Geometry1, typename Geometry2, typename Strategy>
bool call_equals(Geometry1 const& geometry1,
                 Geometry2 const& geometry2,
                 Strategy const& strategy)
{
    return bg::equals(geometry1, geometry2, strategy);
}

template <typename Geometry1, typename Geometry2>
bool call_equals(Geometry1 const& geometry1,
                 Geometry2 const& geometry2,
                 no_strategy)
{
    return bg::equals(geometry1, geometry2);
}

template <typename Geometry1, typename Geometry2, typename Strategy>
void check_geometry(Geometry1 const& geometry1,
                    Geometry2 const& geometry2,
                    std::string const& caseid,
                    std::string const& wkt1,
                    std::string const& wkt2,
                    bool expected,
                    Strategy const& strategy)
{
    bool detected = call_equals(geometry1, geometry2, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
        "case: " << caseid
        << " equals: " << wkt1
        << " to " << wkt2
        << " -> Expected: " << expected
        << " detected: " << detected);

    detected = call_equals(geometry2, geometry1, strategy);

    BOOST_CHECK_MESSAGE(detected == expected,
        "case: " << caseid
        << " equals: " << wkt2
        << " to " << wkt1
        << " -> Expected: " << expected
        << " detected: " << detected);
}


template <typename Geometry1, typename Geometry2>
void test_geometry(std::string const& caseid,
            std::string const& wkt1,
            std::string const& wkt2,
            bool expected,
            bool correct_geometries = false)
{
    Geometry1 geometry1;
    Geometry2 geometry2;

    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);

    if (correct_geometries)
    {
        bg::correct(geometry1);
        bg::correct(geometry2);
    }

    typedef typename bg::strategy::relate::services::default_strategy
        <
            Geometry1, Geometry2
        >::type strategy_type;

    check_geometry(geometry1, geometry2, caseid, wkt1, wkt2, expected, no_strategy());
    check_geometry(geometry1, geometry2, caseid, wkt1, wkt2, expected, strategy_type());
    check_geometry(boost::variant<Geometry1>(geometry1), geometry2, caseid, wkt1, wkt2, expected, no_strategy());
    check_geometry(geometry1, boost::variant<Geometry2>(geometry2), caseid, wkt1, wkt2, expected, no_strategy());
    check_geometry(boost::variant<Geometry1>(geometry1), boost::variant<Geometry2>(geometry2), caseid, wkt1, wkt2, expected, no_strategy());
}

template <typename Geometry1, typename Geometry2>
void test_geometry(std::string const& wkt1,
                   std::string const& wkt2,
                   bool expected)
{
    test_geometry<Geometry1, Geometry2>("", wkt1, wkt2, expected);
}

#endif
