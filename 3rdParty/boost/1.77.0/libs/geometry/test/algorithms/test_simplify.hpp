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
#include <boost/geometry/algorithms/correct_closure.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/simplify.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/variant/variant.hpp>


template
<
    typename GeometryForTag,
    typename Tag = typename bg::tag<GeometryForTag>::type
>
struct test_equality
{
    template <typename Geometry, typename Expected>
    static void apply(Geometry const& geometry, Expected const& expected)
    {
        // Verify both spatially equal AND number of points, because several
        // of the tests only check explicitly on collinear points being
        // simplified away
        bool const result
                = bg::equals(geometry, expected)
                && bg::num_points(geometry) == bg::num_points(expected);

        BOOST_CHECK_MESSAGE(result,
                            " result: " << bg::wkt(geometry) << " " << bg::area(geometry)
                            << " expected: " << bg::wkt(expected) << " " << bg::area(expected));

    }
};

// Linestring does NOT yet have "geometry::equals" implemented
// Until then, WKT's are compared (which is acceptable for linestrings, but not
// for polygons, because simplify might rotate them)
template <typename GeometryForTag>
struct test_equality<GeometryForTag, bg::linestring_tag>
{
    template <typename Geometry, typename Expected>
    static void apply(Geometry const& geometry, Expected const& expected)
    {
        std::ostringstream out1, out2;
        out1 << bg::wkt(geometry);
        out2 << bg::wkt(expected);
        BOOST_CHECK_EQUAL(out1.str(), out2.str());
    }
};


template <typename Tag>
struct test_inserter
{
    template <typename Geometry, typename Expected>
    static void apply(Geometry& , Expected const& , double )
    {}
};

template <>
struct test_inserter<bg::linestring_tag>
{
    template <typename Geometry, typename Expected, typename DistanceMeasure>
    static void apply(Geometry& geometry,
            Expected const& expected,
            DistanceMeasure const& distance)
    {
        {
            Geometry simplified;
            bg::detail::simplify::simplify_insert(geometry,
                std::back_inserter(simplified), distance);

            test_equality<Geometry>::apply(simplified, expected);
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

            test_equality<Geometry>::apply(simplified, expected);
        }
#endif
    }
};

template <typename Geometry, typename Expected, typename DistanceMeasure>
void check_geometry(Geometry const& geometry,
                    Expected const& expected,
                    DistanceMeasure const& distance)
{
    Geometry simplified;
    bg::simplify(geometry, simplified, distance);
    test_equality<Expected>::apply(simplified, expected);
}

template <typename Geometry, typename Expected, typename Strategy, typename DistanceMeasure>
void check_geometry(Geometry const& geometry,
                    Expected const& expected,
                    DistanceMeasure const& distance,
                    Strategy const& strategy)
{
    Geometry simplified;
    bg::simplify(geometry, simplified, distance, strategy);
    test_equality<Expected>::apply(simplified, expected);
}

template <typename Geometry, typename DistanceMeasure>
void check_geometry_with_area(Geometry const& geometry,
                    double expected_area,
                    DistanceMeasure const& distance)
{
    Geometry simplified;
    bg::simplify(geometry, simplified, distance);
    BOOST_CHECK_CLOSE(bg::area(simplified), expected_area, 0.01);
}


template <typename Geometry, typename DistanceMeasure>
void test_geometry(std::string const& wkt,
        std::string const& expected_wkt,
        DistanceMeasure distance)
{
    typedef typename bg::point_type<Geometry>::type point_type;

    Geometry geometry, expected;

    bg::read_wkt(wkt, geometry);
    bg::read_wkt(expected_wkt, expected);

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
            typename bg::tag<Geometry>::type
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
        std::string const& expected_wkt,
        DistanceMeasure const& distance,
        Strategy const& strategy)
{
    Geometry geometry, expected;

    bg::read_wkt(wkt, geometry);
    bg::read_wkt(expected_wkt, expected);
    bg::correct_closure(geometry);
    bg::correct_closure(expected);

    boost::variant<Geometry> v(geometry);

    BOOST_CONCEPT_ASSERT( (bg::concepts::SimplifyStrategy<Strategy,
                           typename bg::point_type<Geometry>::type>) );

    check_geometry(geometry, expected, distance, strategy);
    check_geometry(v, expected, distance, strategy);
}

template <typename Geometry, typename DistanceMeasure>
void test_geometry(std::string const& wkt,
        double expected_area,
        DistanceMeasure const& distance)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    bg::correct_closure(geometry);

    check_geometry_with_area(geometry, expected_area, distance);
}

#endif
