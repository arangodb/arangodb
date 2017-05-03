// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_PERIMETER_HPP
#define BOOST_GEOMETRY_TEST_PERIMETER_HPP


#include <boost/variant/variant.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/perimeter.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/io/wkt/read.hpp>


template <typename Geometry>
void test_perimeter(Geometry const& geometry, long double expected_perimeter)
{
    typename bg::default_length_result<Geometry>::type
        perimeter = bg::perimeter(geometry);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::ostringstream out;
    out << typeid(typename bg::coordinate_type<Geometry>::type).name()
        << std::endl
        << typeid(typename bg::default_perimeter_result<Geometry>::type).name()
        << std::endl
        << "perimeter : " << bg::perimeter(geometry)
        << std::endl;
    std::cout << out.str();
#endif

    BOOST_CHECK_CLOSE(perimeter, expected_perimeter, 0.0001);
}


template <typename Geometry, typename Strategy>
void test_perimeter(Geometry const& geometry, long double expected_perimeter, Strategy strategy)
{
    typename bg::default_length_result<Geometry>::type
        perimeter = bg::perimeter(geometry, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::ostringstream out;
    out << typeid(typename bg::coordinate_type<Geometry>::type).name()
        << std::endl
        << typeid(typename bg::default_perimeter_result<Geometry>::type).name()
        << std::endl
        << "perimeter : " << bg::perimeter(geometry, strategy)
        << std::endl;
    std::cout << out.str();
#endif

    BOOST_CHECK_CLOSE(perimeter, expected_perimeter, 0.0001);
}

template <typename Geometry>
void test_geometry(std::string const& wkt, double expected_perimeter)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> v(geometry);

    test_perimeter(geometry, expected_perimeter);
#if !defined(BOOST_GEOMETRY_TEST_DEBUG)
    test_perimeter(v, expected_perimeter);
#endif
}

template <typename Geometry, typename Strategy>
void test_geometry(std::string const& wkt, double expected_perimeter, Strategy strategy)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> v(geometry);

    test_perimeter(geometry, expected_perimeter, strategy);
#if !defined(BOOST_GEOMETRY_TEST_DEBUG)
    test_perimeter(v, expected_perimeter, strategy);
#endif
}

template <typename Geometry>
void test_empty_input(Geometry const& geometry)
{
    try
    {
        bg::perimeter(geometry);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );
}

#endif
