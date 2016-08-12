// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_DISTANCE_SE_COMMON_HPP
#define BOOST_GEOMETRY_TEST_DISTANCE_SE_COMMON_HPP

#include <iostream>
#include <string>

#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/write.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <boost/geometry/algorithms/num_interior_rings.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/comparable_distance.hpp>

#include <from_wkt.hpp>
#include <string_from_type.hpp>

#include "distance_brute_force.hpp"

namespace bg = ::boost::geometry;

static const double earth_radius_km = 6371.0;
static const double earth_radius_miles = 3959.0;


//========================================================================


template <typename T>
struct check_equal
{
    template <typename Value, typename = void>
    struct equal_to
    {
        static inline void apply(Value const& x, Value const& y)
        {
            BOOST_CHECK(x == y);
        }
    };

    template <typename Dummy>
    struct equal_to<double, Dummy>
    {
        static inline void apply(double x, double y)
        {
            BOOST_CHECK_CLOSE(x, y, 0.001);
        }
    };

    template <typename Geometry1, typename Geometry2>
    static inline void apply(std::string const& /*case_id*/,
                             std::string const& /*subcase_id*/,
                             Geometry1 const& /*geometry1*/,
                             Geometry2 const& /*geometry2*/,
                             T const& detected,
                             T const& expected)
    {
        equal_to<T>::apply(expected, detected);
        /*
          TODO:
          Ideally we would want the following, but it does not work well
          approximate equality test.

        BOOST_CHECK_MESSAGE(equal_to<T>::apply(expected, detected),
             "case ID: " << case_id << "-" << subcase_id << "; "
             << "G1: " << bg::wkt(geometry1)
             << " - "
             << "G2: " << bg::wkt(geometry2)
             << " -> Detected: " << detected
             << "; Expected: " << expected);
        */
    }
};

//========================================================================

template
<
    typename Geometry1, typename Geometry2,
    int id1 = bg::geometry_id<Geometry1>::value,
    int id2 = bg::geometry_id<Geometry2>::value
>
struct test_distance_of_geometries
    : public test_distance_of_geometries<Geometry1, Geometry2, 0, 0>
{};


template <typename Geometry1, typename Geometry2>
struct test_distance_of_geometries<Geometry1, Geometry2, 0, 0>
{
    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& case_id,
               std::string const& wkt1,
               std::string const& wkt2,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool test_reversed = true)
    {
        Geometry1 geometry1 = from_wkt<Geometry1>(wkt1);
        Geometry2 geometry2 = from_wkt<Geometry2>(wkt2);

        apply(case_id, geometry1, geometry2,
              expected_distance, expected_comparable_distance,
              strategy, test_reversed);
    }

    template <typename DistanceType, typename Strategy>
    static inline
    void apply(std::string const& case_id,
               std::string const& wkt1,
               std::string const& wkt2,
               DistanceType const& expected_distance,
               Strategy const& strategy,
               bool test_reversed = true)
    {
        Geometry1 geometry1 = from_wkt<Geometry1>(wkt1);
        Geometry2 geometry2 = from_wkt<Geometry2>(wkt2);

        apply(case_id, geometry1, geometry2,
              expected_distance, expected_distance,
              strategy, test_reversed);
    }


    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& case_id,
               Geometry1 const& geometry1,
               Geometry2 const& geometry2,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool test_reversed = true)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "case ID: " << case_id << "; "
                  << "G1: " << bg::wkt(geometry1)
                  << " - "
                  << "G2: " << bg::wkt(geometry2)
                  << std::endl;
#endif
        namespace services = bg::strategy::distance::services;

        using bg::unit_test::distance_brute_force;

        typedef typename bg::default_distance_result
            <
                Geometry1, Geometry2
            >::type default_distance_result;

        typedef typename services::return_type
            <
                Strategy, Geometry1, Geometry2
            >::type distance_result_from_strategy;

        static const bool same_regular = boost::is_same
            <
                default_distance_result,
                distance_result_from_strategy
            >::type::value;

        BOOST_CHECK(same_regular);

        typedef typename bg::default_comparable_distance_result
            <
                Geometry1, Geometry2
            >::type default_comparable_distance_result;

        typedef typename services::return_type
            <
                typename services::comparable_type<Strategy>::type,
                Geometry1,
                Geometry2
            >::type comparable_distance_result_from_strategy;

        static const bool same_comparable = boost::is_same
            <
                default_comparable_distance_result,
                comparable_distance_result_from_strategy
            >::type::value;
        
        BOOST_CHECK( same_comparable );

        // check distance with passed strategy
        distance_result_from_strategy dist =
            bg::distance(geometry1, geometry2, strategy);

        check_equal
            <
                distance_result_from_strategy
            >::apply(case_id, "a", geometry1, geometry2,
                     dist, expected_distance);

        // check against the comparable distance computed in a
        // brute-force manner
        default_distance_result dist_brute_force
            = distance_brute_force(geometry1, geometry2, strategy);

        check_equal
            <
                default_distance_result
            >::apply(case_id, "b", geometry1, geometry2,
                     dist_brute_force, expected_distance);

        // check comparable distance with passed strategy
        comparable_distance_result_from_strategy cdist =
            bg::comparable_distance(geometry1, geometry2, strategy);

        check_equal
            <
                default_comparable_distance_result
            >::apply(case_id, "c", geometry1, geometry2,
                     cdist, expected_comparable_distance);

        // check against the comparable distance computed in a
        // brute-force manner
        default_comparable_distance_result cdist_brute_force
            = distance_brute_force(geometry1,
                                   geometry2,
                                   services::get_comparable
                                       <
                                           Strategy
                                       >::apply(strategy));

        check_equal
            <
                default_comparable_distance_result
            >::apply(case_id, "d", geometry1, geometry2,
                     cdist_brute_force, expected_comparable_distance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << string_from_type<typename bg::coordinate_type<Geometry1>::type>::name()
                  << string_from_type<typename bg::coordinate_type<Geometry2>::type>::name()
                  << " -> "
                  << string_from_type<default_distance_result>::name()
                  << string_from_type<default_comparable_distance_result>::name()
                  << std::endl;
        std::cout << "strategy radius: " << strategy.radius() << std::endl;
        std::cout << "expected distance = "
                  << expected_distance << " ; "
                  << "expected comp. distance = "
                  << expected_comparable_distance
                  << std::endl;
        std::cout << "distance = "
                  << dist << " ; " 
                  << "comp. distance = "
                  << cdist
                  << std::endl;

        if ( !test_reversed )
        {
            std::cout << std::endl;
        }
#endif

        if ( test_reversed )
        {
            // check distance with given strategy
            dist = bg::distance(geometry2, geometry1, strategy);

            check_equal
                <
                    default_distance_result
                >::apply(case_id, "ra", geometry2, geometry1,
                         dist, expected_distance);

            // check comparable distance with given strategy
            cdist = bg::comparable_distance(geometry2, geometry1, strategy);

            check_equal
                <
                    default_comparable_distance_result
                >::apply(case_id, "rc", geometry2, geometry1,
                         cdist, expected_comparable_distance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "distance[reversed args] = "
                      << dist << " ; "
                      << "comp. distance[reversed args] = "
                      << cdist
                      << std::endl;
            std::cout << std::endl;
#endif
        }
    }
};


//========================================================================


template <typename Geometry1, typename Geometry2, typename Strategy>
void test_empty_input(Geometry1 const& geometry1,
                      Geometry2 const& geometry2,
                      Strategy const& strategy)
{
    try
    {
        bg::distance(geometry1, geometry2);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false,
                        "A empty_input_exception should have been thrown");

    try
    {
        bg::distance(geometry2, geometry1);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false,
                        "A empty_input_exception should have been thrown");

    try
    {
        bg::distance(geometry1, geometry2, strategy);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false,
                        "A empty_input_exception should have been thrown");

    try
    {
        bg::distance(geometry2, geometry1, strategy);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false,
                        "A empty_input_exception should have been thrown");
}

#endif // BOOST_GEOMETRY_TEST_DISTANCE_SE_COMMON_HPP
