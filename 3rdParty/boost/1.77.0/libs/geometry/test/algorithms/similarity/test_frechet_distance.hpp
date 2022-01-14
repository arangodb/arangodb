// Boost.Geometry

// Copyright (c) 2018 Yaghyavardhan Singh Khangarot, Hyderabad, India.

// Contributed and/or modified by Yaghyavardhan Singh Khangarot, as part of Google Summer of Code 2018 program.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_FRECHET_DISTANCE_HPP
#define BOOST_GEOMETRY_TEST_FRECHET_DISTANCE_HPP

#include <geometry_test_common.hpp>
#include <boost/geometry/algorithms/discrete_frechet_distance.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/variant/variant.hpp>

template <typename Geometry1,typename Geometry2>
void test_frechet_distance(Geometry1 const& geometry1,Geometry2 const& geometry2,
    typename bg::distance_result
        <
            typename bg::point_type<Geometry1>::type,
            typename bg::point_type<Geometry2>::type
        >::type expected_frechet_distance )
{
    using namespace bg;
    typedef typename distance_result
        <
            typename point_type<Geometry1>::type,
            typename point_type<Geometry2>::type
        >::type result_type;
    result_type h_distance = bg::discrete_frechet_distance(geometry1,geometry2);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::ostringstream out;
    out << typeid(typename bg::coordinate_type<Geometry1>::type).name()
        << std::endl
        << typeid(typename bg::coordinate_type<Geometry2>::type).name()
        << std::endl
        << typeid(h_distance).name()
        << std::endl
        << "frechet_distance : " << bg::discrete_frechet_distance(geometry1,geometry2)
        << std::endl;
    std::cout << out.str();
#endif

    BOOST_CHECK_CLOSE(h_distance, expected_frechet_distance, 0.001);
}



template <typename Geometry1,typename Geometry2>
void test_geometry(std::string const& wkt1,std::string const& wkt2,
    typename bg::distance_result
        <
            typename bg::point_type<Geometry1>::type,
            typename bg::point_type<Geometry2>::type
        >::type expected_frechet_distance)
{
    Geometry1 geometry1;
    bg::read_wkt(wkt1, geometry1);
    Geometry2 geometry2;
    bg::read_wkt(wkt2, geometry2);
    test_frechet_distance(geometry1,geometry2,expected_frechet_distance);
#if defined(BOOST_GEOMETRY_TEST_DEBUG)
    test_frechet_distance(boost::variant<Geometry1>(geometry1),boost::variant<Geometry2>(geometry2), expected_frechet_distance);
#endif
}

template <typename Geometry1,typename Geometry2 ,typename Strategy>
void test_frechet_distance(Geometry1 const& geometry1,Geometry2 const& geometry2,Strategy strategy,
    typename bg::distance_result
        <
            typename bg::point_type<Geometry1>::type,
            typename bg::point_type<Geometry2>::type,
            Strategy
        >::type expected_frechet_distance )
{
    using namespace bg;
    typedef typename distance_result
        <
            typename point_type<Geometry1>::type,
            typename point_type<Geometry2>::type,
            Strategy
        >::type result_type;
    result_type h_distance = bg::discrete_frechet_distance(geometry1,geometry2,strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::ostringstream out;
    out << typeid(typename bg::coordinate_type<Geometry1>::type).name()
        << std::endl
        << typeid(typename bg::coordinate_type<Geometry2>::type).name()
        << std::endl
        << typeid(h_distance).name()
        << std::endl
        << "frechet_distance : " << bg::discrete_frechet_distance(geometry1,geometry2,strategy)
        << std::endl;
    std::cout << out.str();
#endif

    BOOST_CHECK_CLOSE(h_distance, expected_frechet_distance, 0.001);
}



template <typename Geometry1,typename Geometry2,typename Strategy>
void test_geometry(std::string const& wkt1,std::string const& wkt2,Strategy strategy,
    typename bg::distance_result
        <
            typename bg::point_type<Geometry1>::type,
            typename bg::point_type<Geometry2>::type,
            Strategy
        >::type expected_frechet_distance)
{
    Geometry1 geometry1;
    bg::read_wkt(wkt1, geometry1);
    Geometry2 geometry2;
    bg::read_wkt(wkt2, geometry2);
    test_frechet_distance(geometry1,geometry2,strategy,expected_frechet_distance);
#if defined(BOOST_GEOMETRY_TEST_DEBUG)
    test_frechet_distance(boost::variant<Geometry1>(geometry1),boost::variant<Geometry2>(geometry2),strategy, expected_frechet_distance);
#endif
}


template <typename Geometry1,typename Geometry2>
void test_empty_input(Geometry1 const& geometry1,Geometry2 const& geometry2)
{
    try
    {
        bg::discrete_frechet_distance(geometry1,geometry2);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );
}


#endif

