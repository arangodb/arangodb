// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2015, 2016, 2017.
// Modifications copyright (c) 2015-2017 Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_UNION_HPP
#define BOOST_GEOMETRY_TEST_UNION_HPP

#include <fstream>

#include <geometry_test_common.hpp>
#include "../setop_output_type.hpp"
#include "../check_validity.hpp"

#include <boost/core/ignore_unused.hpp>
#include <boost/foreach.hpp>
#include <boost/range/algorithm/copy.hpp>

#include <boost/geometry/algorithms/union.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>


#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

struct ut_settings
{
    double percentage;
    bool test_validity;

    ut_settings()
        : percentage(0.001)
        , test_validity(true)
    {}

};

#if defined(BOOST_GEOMETRY_TEST_CHECK_VALID_INPUT)
template <typename Geometry>
inline void check_input_validity(std::string const& caseid, int case_index,
                Geometry const& geometry)
{
    std::string message;
    if (!bg::is_valid(geometry, message))
    {
        std::cout << caseid << " Input ["
                  << case_index << "] not valid" << std::endl
                  << "   ("  << message << ")" << std::endl;
    }
}
#endif



template <typename Range>
inline std::size_t num_points(Range const& rng, bool add_for_open = false)
{
    std::size_t result = 0;
    for (typename boost::range_iterator<Range const>::type it = boost::begin(rng);
            it != boost::end(rng); ++it)
    {
        result += bg::num_points(*it, add_for_open);
    }
    return result;
}

template <typename OutputType, typename G1, typename G2>
void test_union(std::string const& caseid, G1 const& g1, G2 const& g2,
        int expected_count, int expected_hole_count,
        int expected_point_count, double expected_area,
        ut_settings const& settings)
{
    typedef typename bg::coordinate_type<G1>::type coordinate_type;
    boost::ignore_unused<coordinate_type>();
    boost::ignore_unused(expected_point_count);

    // Declare output (vector of rings or multi_polygon)
    typedef typename setop_output_type<OutputType>::type result_type;
    result_type clip;

#if defined(BOOST_GEOMETRY_DEBUG_ROBUSTNESS)
    std::cout << "*** UNION " << caseid << std::endl;
#endif

#if defined(BOOST_GEOMETRY_TEST_CHECK_VALID_INPUT)
    check_input_validity(caseid, 0, g1);
    check_input_validity(caseid, 1, g2);
#endif

    // Check normal behaviour
    bg::union_(g1, g2, clip);

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    {
        // Check strategy passed explicitly
        result_type clip_s;
        typedef typename bg::strategy::intersection::services::default_strategy
            <
                typename bg::cs_tag<OutputType>::type
            >::type strategy_type;
        bg::union_(g1, g2, clip_s, strategy_type());
        BOOST_CHECK_EQUAL(num_points(clip), num_points(clip_s));
    }
#endif

    if (settings.test_validity)
    {
        std::string message;
        bool const valid = check_validity<result_type>::apply(clip, message);
        BOOST_CHECK_MESSAGE(valid,
            "union: " << caseid << " not valid: " << message
            << " type: " << (type_for_assert_message<G1, G2>()));
    }


    typename bg::default_area_result<OutputType>::type area = 0;
    std::size_t n = 0;
    std::size_t holes = 0;
    for (typename result_type::iterator it = clip.begin();
            it != clip.end(); ++it)
    {
        area += bg::area(*it);
        holes += bg::num_interior_rings(*it);
        n += bg::num_points(*it, true);
    }


#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    {
        // Test inserter functionality
        // Test if inserter returns output-iterator (using Boost.Range copy)
        result_type inserted, array_with_one_empty_geometry;
        array_with_one_empty_geometry.push_back(OutputType());
        boost::copy(array_with_one_empty_geometry, bg::detail::union_::union_insert<OutputType>(g1, g2, std::back_inserter(inserted)));

        typename bg::default_area_result<OutputType>::type area_inserted = 0;
        int index = 0;
        for (typename result_type::iterator it = inserted.begin();
                it != inserted.end();
                ++it, ++index)
        {
            // Skip the empty polygon created above to avoid the empty_input_exception
            if (! bg::is_empty(*it))
            {
                area_inserted += bg::area(*it);
            }
        }
        BOOST_CHECK_EQUAL(boost::size(clip), boost::size(inserted) - 1);
        BOOST_CHECK_CLOSE(area_inserted, expected_area, settings.percentage);
    }
#endif



#if defined(BOOST_GEOMETRY_DEBUG_ROBUSTNESS)
    std::cout << "*** case: " << caseid
        << " area: " << area
        << " points: " << n
        << " polygons: " << boost::size(clip)
        << " holes: " << holes
        << std::endl;
#endif

    BOOST_CHECK_MESSAGE(expected_count < 0 || int(clip.size()) == expected_count,
            "union: " << caseid
            << " #clips expected: " << expected_count
            << " detected: " << clip.size()
            << " type: " << (type_for_assert_message<G1, G2>())
            );

    BOOST_CHECK_MESSAGE(expected_hole_count < 0 || int(holes) == expected_hole_count,
            "union: " << caseid
            << " #holes expected: " << expected_hole_count
            << " detected: " << holes
            << " type: " << (type_for_assert_message<G1, G2>())
            );

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    BOOST_CHECK_MESSAGE(expected_point_count < 0 || std::abs(int(n) - expected_point_count) < 3,
            "union: " << caseid
            << " #points expected: " << expected_point_count
            << " detected: " << n
            << " type: " << (type_for_assert_message<G1, G2>())
            );
#endif

    BOOST_CHECK_CLOSE(area, expected_area, settings.percentage);

#if defined(TEST_WITH_SVG)
    {
        bool const ccw =
            bg::point_order<G1>::value == bg::counterclockwise
            || bg::point_order<G2>::value == bg::counterclockwise;
        bool const open =
            bg::closure<G1>::value == bg::open
            || bg::closure<G2>::value == bg::open;

        std::ostringstream filename;
        filename << "union_"
            << caseid << "_"
            << string_from_type<coordinate_type>::name()
            << (ccw ? "_ccw" : "")
            << (open ? "_open" : "")
#if defined(BOOST_GEOMETRY_NO_SELF_TURNS)
           << "_no_self"
#endif
#if defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
            << "_no_rob"
#endif
            << ".svg";

        std::ofstream svg(filename.str().c_str());

        bg::svg_mapper
            <
                typename bg::point_type<G2>::type
            > mapper(svg, 500, 500);
        mapper.add(g1);
        mapper.add(g2);

        mapper.map(g1, "fill-opacity:0.5;fill:rgb(153,204,0);"
                "stroke:rgb(153,204,0);stroke-width:3");
        mapper.map(g2, "fill-opacity:0.3;fill:rgb(51,51,153);"
                "stroke:rgb(51,51,153);stroke-width:3");
        //mapper.map(g1, "opacity:0.6;fill:rgb(0,0,255);stroke:rgb(0,0,0);stroke-width:1");
        //mapper.map(g2, "opacity:0.6;fill:rgb(0,255,0);stroke:rgb(0,0,0);stroke-width:1");

        for (typename result_type::const_iterator it = clip.begin();
                it != clip.end(); ++it)
        {
            mapper.map(*it, "fill-opacity:0.2;stroke-opacity:0.4;fill:rgb(255,0,0);"
                    "stroke:rgb(255,0,255);stroke-width:8");
            //mapper.map(*it, "opacity:0.6;fill:none;stroke:rgb(255,0,0);stroke-width:5");
        }
    }
#endif
}

template <typename OutputType, typename G1, typename G2>
void test_one(std::string const& caseid,
        std::string const& wkt1, std::string const& wkt2,
        int expected_count, int expected_hole_count,
        int expected_point_count, double expected_area,
        ut_settings const& settings = ut_settings())
{
    G1 g1;
    bg::read_wkt(wkt1, g1);

    G2 g2;
    bg::read_wkt(wkt2, g2);

    // Reverse/close if necessary (e.g. G1/G2 are ccw and/or open)
    bg::correct(g1);
    bg::correct(g2);

    test_union<OutputType>(caseid, g1, g2,
        expected_count, expected_hole_count, expected_point_count,
        expected_area, settings);
}

#endif
