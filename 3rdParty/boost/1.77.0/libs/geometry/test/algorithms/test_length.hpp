// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// Copyright (c) 2016-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_LENGTH_HPP
#define BOOST_GEOMETRY_TEST_LENGTH_HPP

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/length.hpp>

#ifndef BOOST_GEOMETRY_TEST_DEBUG
#include <boost/geometry/geometries/adapted/boost_variant.hpp>
#include <boost/geometry/geometries/adapted/boost_variant2.hpp>
#include <boost/geometry/geometries/geometry_collection.hpp>
#endif

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/strategies.hpp>

template <typename Geometry>
void test_length(Geometry const& geometry, long double expected_length)
{
    typename bg::default_length_result<Geometry>::type
        length = bg::length(geometry);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::ostringstream out;
    out << typeid(typename bg::coordinate_type<Geometry>::type).name()
        << std::endl
        << typeid(typename bg::default_length_result<Geometry>::type).name()
        << std::endl
        << "length : " << bg::length(geometry)
        << std::endl;
    std::cout << out.str();
#endif

    BOOST_CHECK_CLOSE(length, expected_length, 0.0001);
}

template <typename Geometry, typename Strategy>
void test_length(Geometry const& geometry, long double expected_length, Strategy strategy)
{
    typename bg::default_length_result<Geometry>::type
        length = bg::length(geometry, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::ostringstream out;
    out << typeid(typename bg::coordinate_type<Geometry>::type).name()
        << std::endl
        << typeid(typename bg::default_length_result<Geometry>::type).name()
        << std::endl
        << "length : " << bg::length(geometry, strategy)
        << std::endl;
    std::cout << out.str();
#endif

    BOOST_CHECK_CLOSE(length, expected_length, 0.0001);
}

template <typename Geometry>
void test_geometry(std::string const& wkt, double expected_length)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    test_length(geometry, expected_length);

#ifndef BOOST_GEOMETRY_TEST_DEBUG
    using variant_t = boost::variant<Geometry>;
    using variant2_t = boost::variant2::variant<Geometry>;
    using gc_t = bg::model::geometry_collection<variant2_t>;
    test_length(variant_t(geometry), expected_length);
    test_length(variant2_t(geometry), expected_length);
    test_length(gc_t{variant2_t(geometry), variant2_t(geometry)}, expected_length * 2);
#endif
}

template <typename Geometry, typename Strategy>
void test_geometry(std::string const& wkt, double expected_length, Strategy strategy)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    test_length(geometry, expected_length, strategy);

#ifndef BOOST_GEOMETRY_TEST_DEBUG
    using variant_t = boost::variant<Geometry>;
    using variant2_t = boost::variant2::variant<Geometry>;
    using gc_t = bg::model::geometry_collection<variant2_t>;
    test_length(variant_t(geometry), expected_length, strategy);
    test_length(variant2_t(geometry), expected_length, strategy);
    test_length(gc_t{variant2_t(geometry), variant2_t(geometry)}, expected_length * 2, strategy);
#endif
}

template <typename Geometry>
void test_empty_input(Geometry const& geometry)
{
    try
    {
        bg::length(geometry);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );
}

#endif
