// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_DISTANCE_GEO_COMMON_HPP
#define BOOST_GEOMETRY_TEST_DISTANCE_GEO_COMMON_HPP

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

#include <boost/geometry/strategies/strategies.hpp>

#include <from_wkt.hpp>
#include <string_from_type.hpp>

#include "distance_brute_force.hpp"

namespace bg = ::boost::geometry;

typedef bg::srs::spheroid<double> stype;

// Spherical strategy  for point-point distance

typedef bg::strategy::distance::haversine<double> spherical_pp;

// Geo strategies for point-point distance

typedef bg::strategy::distance::andoyer<stype> andoyer_pp;
typedef bg::strategy::distance::thomas<stype> thomas_pp;
typedef bg::strategy::distance::vincenty<stype> vincenty_pp;

// Spherical strategy  for point-segment distance

typedef bg::strategy::distance::cross_track<> spherical_ps;

// Geo strategies for point-segment distance

typedef bg::strategy::distance::geographic_cross_track<bg::strategy::andoyer, stype, double>
        andoyer_ps;

typedef bg::strategy::distance::geographic_cross_track<bg::strategy::thomas, stype, double>
        thomas_ps;

typedef bg::strategy::distance::geographic_cross_track<bg::strategy::vincenty, stype, double>
        vincenty_ps;

typedef bg::strategy::distance::detail::geographic_cross_track<bg::strategy::vincenty, stype, double, true>
        vincenty_ps_bisection;

// Spherical strategy  for point-box distance

typedef bg::strategy::distance::cross_track_point_box<> spherical_pb;

// Geo strategies for point-box distance

typedef bg::strategy::distance::geographic_cross_track_point_box
<
    bg::strategy::andoyer,
    stype,
    double
> andoyer_pb;

typedef bg::strategy::distance::geographic_cross_track_point_box
<
    bg::strategy::thomas,
    stype,
    double
> thomas_pb;

typedef bg::strategy::distance::geographic_cross_track_point_box
<
    bg::strategy::vincenty,
    stype,
    double
> vincenty_pb;

// Spherical strategy  for segment-box distance

typedef bg::strategy::distance::spherical_segment_box<> spherical_sb;

// Geo strategies for segment-box distance

typedef bg::strategy::distance::geographic_segment_box<bg::strategy::andoyer, stype, double>
        andoyer_sb;

typedef bg::strategy::distance::geographic_segment_box<bg::strategy::thomas, stype, double>
        thomas_sb;

typedef bg::strategy::distance::geographic_segment_box<bg::strategy::vincenty, stype, double>
        vincenty_sb;

// Strategies for box-box distance

typedef bg::strategy::distance::cross_track_box_box<> spherical_bb;

typedef bg::strategy::distance::geographic_cross_track_box_box
        <
            bg::strategy::andoyer,
            stype,
            double
        > andoyer_bb;

typedef bg::strategy::distance::geographic_cross_track_box_box
        <
            bg::strategy::thomas,
            stype,
            double
        > thomas_bb;

typedef bg::strategy::distance::geographic_cross_track_box_box
        <
            bg::strategy::vincenty,
            stype,
            double
        > vincenty_bb;

//===========================================================================

template <typename Point, typename Strategy>
inline typename bg::default_distance_result<Point>::type
pp_distance(std::string const& wkt1,
            std::string const& wkt2,
            Strategy const& strategy)
{
    Point p1, p2;
    bg::read_wkt(wkt1, p1);
    bg::read_wkt(wkt2, p2);
    return bg::distance(p1, p2, strategy);
}

//===========================================================================

template <typename Point, typename Strategy>
inline typename bg::default_distance_result<Point>::type
ps_distance(std::string const& wkt1,
            std::string const& wkt2,
            Strategy const& strategy)
{
    Point p;
    typedef bg::model::segment<Point> segment_type;
    segment_type s;
    bg::read_wkt(wkt1, p);
    bg::read_wkt(wkt2, s);
    return bg::distance(p, s, strategy);
}

//===========================================================================

template <typename Point, typename Strategy>
inline typename bg::default_distance_result<Point>::type
sb_distance(std::string const& wkt1,
            std::string const& wkt2,
            Strategy const& strategy)
{
    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::box<Point> box_type;
    segment_type s;
    box_type b;
    bg::read_wkt(wkt1, s);
    bg::read_wkt(wkt2, b);
    return bg::distance(s, b, strategy);
}

//===================================================================

template <typename Tag> struct dispatch
{
    //tag dispatching for swaping arguments in segments
    template <typename T>
    static inline T swap(T const& t)
    {
        return t;
    }

    // mirror geometry w.r.t. equator
    template <typename T>
    static inline T mirror(T const& t)
    {
        return t;
    }
};

// Specialization for segments
template <> struct dispatch<boost::geometry::segment_tag>
{
    template <typename Segment>
    static inline Segment swap(Segment const& s)
    {
        Segment s_swaped;

        bg::set<0, 0>(s_swaped, bg::get<1, 0>(s));
        bg::set<0, 1>(s_swaped, bg::get<1, 1>(s));
        bg::set<1, 0>(s_swaped, bg::get<0, 0>(s));
        bg::set<1, 1>(s_swaped, bg::get<0, 1>(s));

        return s_swaped;
    }

    template <typename Segment>
    static inline Segment mirror(Segment const& s)
    {
        Segment s_mirror;

        bg::set<0, 0>(s_mirror, bg::get<0, 0>(s));
        bg::set<0, 1>(s_mirror, bg::get<0, 1>(s) * -1);
        bg::set<1, 0>(s_mirror, bg::get<1, 0>(s));
        bg::set<1, 1>(s_mirror, bg::get<1, 1>(s) * -1);

        return s_mirror;
    }
};

// Specialization for boxes
template <> struct dispatch<boost::geometry::box_tag>
{
    template <typename T>
    static inline T swap(T const& t)
    {
        return t;
    }

    template <typename Box>
    static inline Box mirror(Box const& b)
    {
        Box b_mirror;

        bg::set<0, 0>(b_mirror, bg::get<bg::min_corner, 0>(b));
        bg::set<0, 1>(b_mirror, bg::get<bg::max_corner, 1>(b) * -1);
        bg::set<1, 0>(b_mirror, bg::get<bg::max_corner, 0>(b));
        bg::set<1, 1>(b_mirror, bg::get<bg::min_corner, 1>(b) * -1);

        return b_mirror;
    }
};


// Specialization for points
template <> struct dispatch<boost::geometry::point_tag>
{
    template <typename T>
    static inline T swap(T const& t)
    {
        return t;
    }

    template <typename Point>
    static inline Point mirror(Point const& p)
    {
        Point p_mirror;

        bg::set<0>(p_mirror, bg::get<0>(p));
        bg::set<1>(p_mirror, bg::get<1>(p) * -1);

        return p_mirror;
    }
};

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
    template <typename DistanceType, typename Strategy>
    static inline
    void apply(std::string const& case_id,
               std::string const& wkt1,
               std::string const& wkt2,
               DistanceType const& expected_distance,
               Strategy const& strategy,
               bool test_reversed = true,
               bool swap_geometry_args = false,
               bool mirror_geometry = true)
    {
        Geometry1 geometry1 = from_wkt<Geometry1>(wkt1);
        Geometry2 geometry2 = from_wkt<Geometry2>(wkt2);

        apply(case_id, geometry1, geometry2,
              expected_distance,
              strategy, test_reversed, swap_geometry_args,
              mirror_geometry);
    }


    template
    <
        typename DistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& case_id,
               Geometry1 const& geometry1,
               Geometry2 const& geometry2,
               DistanceType const& expected_distance,
               Strategy const& strategy,
               bool test_reversed = true,
               bool swap_geometry_args = false,
               bool mirror_geometry = true)
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

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << string_from_type<typename bg::coordinate_type<Geometry1>::type>::name()
                  << string_from_type<typename bg::coordinate_type<Geometry2>::type>::name()
                  << " -> "
                  << string_from_type<default_distance_result>::name()
                  << std::endl;
        std::cout << "expected distance = " << std::setprecision(10)
                  << expected_distance << " ; "
                  << std::endl;
        std::cout << "distance = " << std::setprecision(10)
                  << dist << " ; "
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

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "distance[reversed args] = "  << std::setprecision(10)
                      << dist
                      << std::endl;
#endif
        }

        if (swap_geometry_args)
        {
            Geometry1 g1 = dispatch
                <
                    typename boost::geometry::tag<Geometry1>::type
                >::swap(geometry1);

            Geometry2 g2 = dispatch
                <
                    typename boost::geometry::tag<Geometry2>::type
                >::swap(geometry2);

            // check distance with given strategy
            dist = bg::distance(g1, g2, strategy);

            check_equal
                <
                    default_distance_result
                >::apply(case_id, "swap", g1, g2,
                         dist, expected_distance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "distance[swap geometry args] = "  << std::setprecision(10)
                      << dist
                      << std::endl;
            std::cout << std::endl;
#endif
        }
        if (mirror_geometry)
        {
            Geometry1 g1 = dispatch
                <
                    typename boost::geometry::tag<Geometry1>::type
                >::mirror(geometry1);

            Geometry2 g2 = dispatch
                <
                    typename boost::geometry::tag<Geometry2>::type
                >::mirror(geometry2);

            // check distance with given strategy
            dist = bg::distance(g1, g2, strategy);

            check_equal
                <
                    default_distance_result
                >::apply(case_id, "mirror", g1, g2,
                         dist, expected_distance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "distance[mirror geometries] = "  << std::setprecision(10)
                      << dist
                      << std::endl;
            std::cout << std::endl;
#endif
        }
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << std::endl;
#endif

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

#endif // BOOST_GEOMETRY_TEST_DISTANCE_GEO_COMMON_HPP
