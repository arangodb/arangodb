// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_SIMPLIFY_HPP
#define BOOST_GEOMETRY_TEST_SIMPLIFY_HPP

// Test-functionality, shared between single and multi tests

#include <iomanip>
#include <sstream>
#include <geometry_test_common.hpp>
#include <boost/geometry/algorithms/simplify.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/variant/variant.hpp>

template <typename Tag, typename Geometry>
struct test_inserter
{
    static void apply(Geometry& , std::string const& , double )
    {}
};

template <typename Geometry>
struct test_inserter<bg::linestring_tag, Geometry>
{
    template <typename DistanceMeasure>
    static void apply(Geometry& geometry,
            std::string const& expected,
            DistanceMeasure const& distance)
    {
        {
            Geometry simplified;
            bg::detail::simplify::simplify_insert(geometry,
                std::back_inserter(simplified), distance);

            std::ostringstream out;
            // TODO: instead of comparing the full string (with more or less decimal digits),
            // we should call something more robust to check the test for example geometry::equals
            out << std::setprecision(12) << bg::wkt(simplified);
            BOOST_CHECK_EQUAL(out.str(), expected);
        }

#ifdef TEST_PULL89
        {
            typedef typename bg::point_type<Geometry>::type point_type;
            typedef typename bg::strategy::distance::detail::projected_point_ax<>::template result_type<point_type, point_type>::type distance_type;
            typedef bg::strategy::distance::detail::projected_point_ax_less<distance_type> less_comparator;

            distance_type max_distance(distance);
            less_comparator less(max_distance);

            bg::strategy::simplify::detail::douglas_peucker
                <
                    point_type,
                    bg::strategy::distance::detail::projected_point_ax<>,
                    less_comparator
                > strategy(less);

            Geometry simplified;
            bg::detail::simplify::simplify_insert(geometry,
                std::back_inserter(simplified), max_distance, strategy);

            std::ostringstream out;
            // TODO: instead of comparing the full string (with more or less decimal digits),
            // we should call something more robust to check the test for example geometry::equals
            out << std::setprecision(12) << bg::wkt(simplified);
            BOOST_CHECK_EQUAL(out.str(), expected);
        }
#endif
    }
};


template <typename Geometry, typename DistanceMeasure>
void check_geometry(Geometry const& geometry,
                    std::string const& expected,
                    DistanceMeasure const&  distance)
{
    Geometry simplified;
    bg::simplify(geometry, simplified, distance);

    std::ostringstream out;
    out << std::setprecision(12) << bg::wkt(simplified);
    BOOST_CHECK_EQUAL(out.str(), expected);
}

template <typename Geometry, typename Strategy, typename DistanceMeasure>
void check_geometry(Geometry const& geometry,
                    std::string const& expected,
                    DistanceMeasure const& distance,
                    Strategy const& strategy)
{
    Geometry simplified;
    bg::simplify(geometry, simplified, distance, strategy);

    std::ostringstream out;
    out << std::setprecision(12) << bg::wkt(simplified);
    BOOST_CHECK_EQUAL(out.str(), expected);
}


template <typename Geometry, typename DistanceMeasure>
void test_geometry(std::string const& wkt,
        std::string const& expected,
        DistanceMeasure distance)
{
    typedef typename bg::point_type<Geometry>::type point_type;

    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> v(geometry);

    // Define default strategy for testing
    typedef bg::strategy::simplify::douglas_peucker
        <
            typename bg::point_type<Geometry>::type,
            bg::strategy::distance::projected_point<double>
        > dp;

    check_geometry(geometry, expected, distance);
    check_geometry(v, expected, distance);


    BOOST_CONCEPT_ASSERT( (bg::concepts::SimplifyStrategy<dp, point_type>) );

    check_geometry(geometry, expected, distance, dp());
    check_geometry(v, expected, distance, dp());

    // Check inserter (if applicable)
    test_inserter
        <
            typename bg::tag<Geometry>::type,
            Geometry
        >::apply(geometry, expected, distance);

#ifdef TEST_PULL89
    // Check using non-default less comparator in douglass_peucker
    typedef typename bg::strategy::distance::detail::projected_point_ax<>::template result_type<point_type, point_type>::type distance_type;
    typedef bg::strategy::distance::detail::projected_point_ax_less<distance_type> less_comparator;

    distance_type const max_distance(distance);
    less_comparator const less(max_distance);

    typedef bg::strategy::simplify::detail::douglas_peucker
        <
            point_type,
            bg::strategy::distance::detail::projected_point_ax<>,
            less_comparator
        > douglass_peucker_with_less;

    BOOST_CONCEPT_ASSERT( (bg::concepts::SimplifyStrategy<douglass_peucker_with_less, point_type>) );

    check_geometry(geometry, expected, distance, douglass_peucker_with_less(less));
    check_geometry(v, expected, distance, douglass_peucker_with_less(less));
#endif
}

template <typename Geometry, typename Strategy, typename DistanceMeasure>
void test_geometry(std::string const& wkt,
        std::string const& expected,
        DistanceMeasure const& distance,
        Strategy const& strategy)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> v(geometry);

    BOOST_CONCEPT_ASSERT( (bg::concepts::SimplifyStrategy<Strategy,
                           typename bg::point_type<Geometry>::type>) );

    check_geometry(geometry, expected, distance, strategy);
    check_geometry(v, expected, distance, strategy);
}

#endif
