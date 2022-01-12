// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <iostream>
#include <string>

#include <boost/assert.hpp>
#include <boost/variant/variant.hpp>

#include <boost/test/included/unit_test.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/is_simple.hpp>

#include <from_wkt.hpp>

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#include "pretty_print_geometry.hpp"
#endif

namespace bg = ::boost::geometry;

template <typename Geometry, typename Strategy>
void test_simple_s(Geometry const& geometry,
                   Strategy const& strategy,
                   bool expected_result,
                   bool check_validity = true)
{
    bool simple = bg::is_simple(geometry, strategy);
    bool valid = ! check_validity || bg::is_valid(geometry, strategy);

    BOOST_CHECK_MESSAGE( valid == true,
        "Expected valid geometry, "
        << " wkt: " << bg::wkt(geometry) );

    BOOST_CHECK_MESSAGE( simple == expected_result,
        "Expected: " << expected_result
        << " detected: " << simple
        << " wkt: " << bg::wkt(geometry) );
}

template <typename CSTag, typename Geometry>
void test_simple(Geometry const& geometry, bool expected_result,
                 bool check_validity = true)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "=======" << std::endl;
#endif

    bool simple = bg::is_simple(geometry);
    bool valid = ! check_validity || bg::is_valid(geometry);

    BOOST_CHECK_MESSAGE( valid == true,
        "Expected valid geometry, "
        << " wkt: " << bg::wkt(geometry) );

    BOOST_CHECK_MESSAGE( simple == expected_result,
        "Expected: " << expected_result
        << " detected: " << simple
        << " wkt: " << bg::wkt(geometry) );

    typedef typename bg::strategy::intersection::services::default_strategy
        <
            CSTag
        >::type strategy_type;

    test_simple_s(geometry, strategy_type(), expected_result, check_validity);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "Geometry: ";
    pretty_print_geometry<Geometry>::apply(std::cout, geometry);
    std::cout << std::endl;
    std::cout << std::boolalpha;
    std::cout << "is simple: " << simple << std::endl;
    std::cout << "expected result: " << expected_result << std::endl;
    std::cout << "=======" << std::endl;
    std::cout << std::endl << std::endl;
    std::cout << std::noboolalpha;
#endif
}

template <typename Geometry>
void test_simple(Geometry const& geometry,
                 bool expected_result,
                 bool check_validity = true)
{
    typedef typename bg::cs_tag<Geometry>::type cs_tag;
    test_simple<cs_tag>(geometry, expected_result, check_validity);
}

template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
void test_simple(boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> const& variant_geometry,
                 bool expected_result,
                 bool check_validity = true)
{
    typedef typename bg::cs_tag<T0>::type cs_tag;
    test_simple<cs_tag>(variant_geometry, expected_result, check_validity);
}
