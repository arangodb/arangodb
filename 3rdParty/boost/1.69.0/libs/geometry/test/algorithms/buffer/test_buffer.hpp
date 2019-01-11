// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test Helper

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2016-2017.
// Modifications copyright (c) 2016-2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_TEST_BUFFER_HPP
#define BOOST_GEOMETRY_TEST_BUFFER_HPP

#if defined(TEST_WITH_SVG)
    // Define before including any buffer headerfile
    #define BOOST_GEOMETRY_BUFFER_USE_HELPER_POINTS
#endif

#include <iostream>
#include <fstream>
#include <iomanip>

#include <boost/foreach.hpp>
#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/num_interior_rings.hpp>
#include <boost/geometry/algorithms/union.hpp>

#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/self_turn_points.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/strategies/buffer.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/util/condition.hpp>

const double same_distance = -999;

#if defined(TEST_WITH_SVG)
#  include <test_buffer_svg.hpp>
#  include <test_buffer_svg_per_turn.hpp>
#endif

//-----------------------------------------------------------------------------
template <typename JoinStrategy>
struct JoinTestProperties
{
    static std::string name() { return "joinunknown"; }
};

template<> struct JoinTestProperties<boost::geometry::strategy::buffer::join_round>
{ 
    static std::string name() { return "round"; }
};

template<> struct JoinTestProperties<boost::geometry::strategy::buffer::join_miter>
{ 
    static std::string name() { return "miter"; }
};

template<> struct JoinTestProperties<boost::geometry::strategy::buffer::join_round_by_divide>
{ 
    static std::string name() { return "divide"; }
};


//-----------------------------------------------------------------------------
template <typename EndStrategy>
struct EndTestProperties { };

template<> struct EndTestProperties<boost::geometry::strategy::buffer::end_round>
{ 
    static std::string name() { return "round"; }
};

template<> struct EndTestProperties<boost::geometry::strategy::buffer::end_flat>
{ 
    static std::string name() { return "flat"; }
};

struct ut_settings
{
    double tolerance;
    bool test_validity;
    bool test_area;

    explicit ut_settings(double tol = 0.01, bool val = true)
        : tolerance(tol)
        , test_validity(val)
        , test_area(true)
    {}

    static inline ut_settings ignore_validity()
    {
        ut_settings result;
        result.test_validity = false;
        return result;
    }

    static inline ut_settings assertions_only()
    {
        ut_settings result;
        result.test_validity = false;
        result.test_area = false;
        return result;
    }

    static inline double ignore_area() { return 9999.9; }
};

template
<
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy,
    typename DistanceStrategy,
    typename SideStrategy,
    typename PointStrategy,
    typename Geometry
>
void test_buffer(std::string const& caseid, Geometry const& geometry,
            JoinStrategy const& join_strategy,
            EndStrategy const& end_strategy,
            DistanceStrategy const& distance_strategy,
            SideStrategy const& side_strategy,
            PointStrategy const& point_strategy,
            int expected_count,
            int expected_holes_count,
            double expected_area,
            ut_settings const& settings)
{
    namespace bg = boost::geometry;

    typedef typename bg::coordinate_type<Geometry>::type coordinate_type;
    typedef typename bg::point_type<Geometry>::type point_type;

    typedef typename bg::tag<Geometry>::type tag;
    // TODO use something different here:
    std::string type = boost::is_same<tag, bg::polygon_tag>::value ? "poly"
        : boost::is_same<tag, bg::linestring_tag>::value ? "line"
        : boost::is_same<tag, bg::point_tag>::value ? "point"
        : boost::is_same<tag, bg::multi_polygon_tag>::value ? "multipoly"
        : boost::is_same<tag, bg::multi_linestring_tag>::value ? "multiline"
        : boost::is_same<tag, bg::multi_point_tag>::value ? "multipoint"
        : ""
        ;

    bg::model::box<point_type> envelope;
    if (bg::is_empty(geometry))
    {
        bg::assign_values(envelope, 0, 0, 1,  1);
    }
    else
    {
        bg::envelope(geometry, envelope);
    }

    std::string join_name = JoinTestProperties<JoinStrategy>::name();
    std::string end_name = EndTestProperties<EndStrategy>::name();

    if ( BOOST_GEOMETRY_CONDITION((
            boost::is_same<tag, bg::point_tag>::value 
         || boost::is_same<tag, bg::multi_point_tag>::value )) )
    {
        join_name.clear();
    }

    std::ostringstream complete;
    complete
        << type << "_"
        << caseid << "_"
        << string_from_type<coordinate_type>::name()
        << "_" << join_name
        << (end_name.empty() ? "" : "_") << end_name
        << (distance_strategy.negative() ? "_deflate" : "")
        << (bg::point_order<GeometryOut>::value == bg::counterclockwise ? "_ccw" : "")
         // << "_" << point_buffer_count
        ;

    //std::cout << complete.str() << std::endl;

#if defined(TEST_WITH_SVG_PER_TURN)
    save_turns_visitor<point_type> visitor;
#elif defined(TEST_WITH_SVG)

    buffer_svg_mapper<point_type> buffer_mapper(complete.str());

    std::ostringstream filename;
    filename << "buffer_" << complete.str() << ".svg";
    std::ofstream svg(filename.str().c_str());
    typedef bg::svg_mapper<point_type> mapper_type;
    mapper_type mapper(svg, 1000, 800);

    svg_visitor<mapper_type, bg::model::box<point_type> > visitor(mapper);

    buffer_mapper.prepare(mapper, visitor, envelope,
            distance_strategy.negative()
            ? 1.0
            : 1.1 * distance_strategy.max_distance(join_strategy, end_strategy)
        );
#else
    bg::detail::buffer::visit_pieces_default_policy visitor;
#endif

    typedef typename bg::point_type<Geometry>::type point_type;
    typedef typename bg::rescale_policy_type<point_type>::type
        rescale_policy_type;
    typedef typename bg::strategy::intersection::services::default_strategy
        <
            typename bg::cs_tag<Geometry>::type
        >::type strategy_type;

    // Enlarge the box to get a proper rescale policy
    bg::buffer(envelope, envelope, distance_strategy.max_distance(join_strategy, end_strategy));

    strategy_type strategy;
    rescale_policy_type rescale_policy
            = bg::get_rescale_policy<rescale_policy_type>(envelope);

    bg::model::multi_polygon<GeometryOut> buffered;

    bg::detail::buffer::buffer_inserter<GeometryOut>(geometry,
                        std::back_inserter(buffered),
                        distance_strategy,
                        side_strategy,
                        join_strategy,
                        end_strategy,
                        point_strategy,
                        strategy,
                        rescale_policy,
                        visitor);

#if defined(TEST_WITH_SVG)
    buffer_mapper.map_input_output(mapper, geometry, buffered, distance_strategy.negative());
#endif

    //Uncomment to create simple CSV to compare/use in tests - adapt precision if necessary
    //std::cout << complete.str() << "," << std::fixed << std::setprecision(0) << area << std::endl;
    //return;

    if (bg::is_empty(buffered) && bg::math::equals(expected_area, 0.0))
    {
        // As expected - don't get rescale policy for output (will be invalid)
        return;
    }

    if (settings.test_area)
    {
        BOOST_CHECK_MESSAGE
            (
                ! bg::is_empty(buffered),
                complete.str() << " output is empty (unexpected)."
            );
    }

    bg::model::box<point_type> envelope_output;
    bg::assign_values(envelope_output, 0, 0, 1,  1);
    bg::envelope(buffered, envelope_output);

    //    std::cout << caseid << std::endl;
    //    std::cout << "INPUT: " << bg::wkt(geometry) << std::endl;
    //    std::cout << "OUTPUT: " << area << std::endl;
    //    std::cout << "OUTPUT env: " << bg::wkt(envelope_output) << std::endl;
    //    std::cout << bg::wkt(buffered) << std::endl;

    if (expected_count >= 0)
    {
        BOOST_CHECK_MESSAGE
            (
                int(buffered.size()) == expected_count,
                "#outputs not as expected."
                << " Expected: " << expected_count
                << " Detected: " << buffered.size()
            );
    }

    if (expected_holes_count >= 0)
    {
        std::size_t nholes = bg::num_interior_rings(buffered);
        BOOST_CHECK_MESSAGE
        (
            int(nholes) == expected_holes_count,
            complete.str() << " #holes not as expected."
            << " Expected: " << expected_holes_count
            << " Detected: " << nholes
        );
    }

    if (settings.test_area)
    {
        typename bg::default_area_result<GeometryOut>::type area = bg::area(buffered);
        double const difference = area - expected_area;
        BOOST_CHECK_MESSAGE
            (
                bg::math::abs(difference) < settings.tolerance,
                complete.str() << " not as expected. " 
                << std::setprecision(18)
                << " Expected: " << expected_area
                << " Detected: " << area
                << " Diff: " << difference
                << " Tol: " << settings.tolerance
                << std::setprecision(3)
                << " , " << 100.0 * (difference / expected_area) << "%"
            );
    }

    if (settings.test_validity && ! bg::is_valid(buffered))
    {
        BOOST_CHECK_MESSAGE(bg::is_valid(buffered), complete.str() <<  " is not valid");
    }

#if defined(TEST_WITH_SVG_PER_TURN)
    {
        // Create a per turn visitor to map per turn, and buffer again with it
        per_turn_visitor<point_type> ptv(complete.str(), visitor.get_points());
        bg::detail::buffer::buffer_inserter<GeometryOut>(geometry,
                            std::back_inserter(buffered),
                            distance_strategy,
                            side_strategy,
                            join_strategy,
                            end_strategy,
                            point_strategy,
                            rescale_policy,
                            ptv);
        ptv.map_input_output(geometry, buffered, distance_strategy.negative());
        // self_ips NYI here
    }
#elif defined(TEST_WITH_SVG)
    rescale_policy_type rescale_policy_output
            = bg::get_rescale_policy<rescale_policy_type>(envelope_output);
    buffer_mapper.map_self_ips(mapper, buffered, strategy, rescale_policy_output);
#endif

}

template
<
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy,
    typename DistanceStrategy,
    typename SideStrategy,
    typename PointStrategy,
    typename Geometry
>
void test_buffer(std::string const& caseid, Geometry const& geometry,
            JoinStrategy const& join_strategy,
            EndStrategy const& end_strategy,
            DistanceStrategy const& distance_strategy,
            SideStrategy const& side_strategy,
            PointStrategy const& point_strategy,
            double expected_area,
            ut_settings const& settings = ut_settings())
{
    test_buffer<GeometryOut>(caseid, geometry,
        join_strategy, end_strategy, distance_strategy, side_strategy, point_strategy,
        -1, -1, expected_area, settings);
}

#ifdef BOOST_GEOMETRY_CHECK_WITH_POSTGIS
static int counter = 0;
#endif

template
<
    typename Geometry,
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy
>
void test_one(std::string const& caseid, std::string const& wkt,
        JoinStrategy const& join_strategy, EndStrategy const& end_strategy,
        int expected_count, int expected_holes_count, double expected_area,
        double distance_left, ut_settings const& settings = ut_settings(),
        double distance_right = same_distance)
{
    namespace bg = boost::geometry;
    Geometry g;
    bg::read_wkt(wkt, g);
    bg::correct(g);


#ifdef BOOST_GEOMETRY_CHECK_WITH_POSTGIS
    std::cout
        << (counter > 0 ? "union " : "")
        << "select " << counter++
        << ", '" << caseid << "' as caseid"
        << ", ST_Area(ST_Buffer(ST_GeomFromText('" << wkt << "'), "
        << distance_left
        << ", 'endcap=" << end_name << " join=" << join_name << "'))"
        << ", "  << expected_area
        << std::endl;
#endif


    bg::strategy::buffer::side_straight side_strategy;
    bg::strategy::buffer::point_circle circle_strategy(88);

    bg::strategy::buffer::distance_asymmetric
    <
        typename bg::coordinate_type<Geometry>::type
    > distance_strategy(distance_left,
                        bg::math::equals(distance_right, same_distance)
                        ? distance_left : distance_right);

    test_buffer<GeometryOut>
            (caseid, g,
            join_strategy, end_strategy,
            distance_strategy, side_strategy, circle_strategy,
            expected_count, expected_holes_count, expected_area,
            settings);

#if !defined(BOOST_GEOMETRY_COMPILER_MODE_DEBUG) && defined(BOOST_GEOMETRY_COMPILER_MODE_RELEASE)

    // Also test symmetric distance strategy if right-distance is not specified
    // (only in release mode)
    if (bg::math::equals(distance_right, same_distance))
    {
        bg::strategy::buffer::distance_symmetric
        <
            typename bg::coordinate_type<Geometry>::type
        > sym_distance_strategy(distance_left);

        test_buffer<GeometryOut>
                (caseid + "_sym", g,
                join_strategy, end_strategy,
                sym_distance_strategy, side_strategy, circle_strategy,
                expected_count, expected_holes_count, expected_area,
                settings);

    }
#endif
}

template
<
    typename Geometry,
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy
>
void test_one(std::string const& caseid, std::string const& wkt,
        JoinStrategy const& join_strategy, EndStrategy const& end_strategy,
        double expected_area,
        double distance_left, ut_settings const& settings = ut_settings(),
        double distance_right = same_distance)
{
    test_one<Geometry, GeometryOut>(caseid, wkt, join_strategy, end_strategy,
        -1 ,-1, expected_area,
        distance_left, settings, distance_right);
}

template
<
    typename Geometry,
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy,
    typename DistanceStrategy,
    typename SideStrategy,
    typename PointStrategy
>
void test_with_custom_strategies(std::string const& caseid,
        std::string const& wkt,
        JoinStrategy const& join_strategy,
        EndStrategy const& end_strategy,
        DistanceStrategy const& distance_strategy,
        SideStrategy const& side_strategy,
        PointStrategy const& point_strategy,
        double expected_area,
        ut_settings const& settings = ut_settings())
{
    namespace bg = boost::geometry;
    Geometry g;
    bg::read_wkt(wkt, g);
    bg::correct(g);

    test_buffer<GeometryOut>
            (caseid, g,
            join_strategy, end_strategy,
            distance_strategy, side_strategy, point_strategy,
            expected_area, settings);
}

#endif
