// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_DIFFERENCE_HPP
#define BOOST_GEOMETRY_TEST_DIFFERENCE_HPP

#include <fstream>
#include <iomanip>

#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/foreach.hpp>

#include <boost/range/algorithm/copy.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/difference.hpp>
#include <boost/geometry/algorithms/sym_difference.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/remove_spikes.hpp>

#include <boost/geometry/geometries/geometries.hpp>


#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>


#if defined(TEST_WITH_SVG)
#  define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#  define BOOST_GEOMETRY_DEBUG_IDENTIFIER
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#  include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#endif


struct ut_settings
{
    double percentage;
    bool sym_difference;
    bool remove_spikes;

    // TODO: set by default to true when all tests pass
    bool test_validity;

    ut_settings()
        : percentage(0.0001)
        , sym_difference(true)
        , remove_spikes(false)
        , test_validity(false)
    {}

};

inline ut_settings tolerance(double percentage)
{
    ut_settings result;
    result.percentage = percentage;
    return result;
}


template <typename Output, typename G1, typename G2>
void difference_output(std::string const& caseid, G1 const& g1, G2 const& g2, Output const& output)
{
    boost::ignore_unused(caseid, g1, g2, output);

#if defined(TEST_WITH_SVG)
    {
        typedef typename bg::coordinate_type<G1>::type coordinate_type;
        typedef typename bg::point_type<G1>::type point_type;

        std::ostringstream filename;
        filename << "difference_"
            << caseid << "_"
            << string_from_type<coordinate_type>::name()
#if defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
            << "_no_rob"
#endif
            << ".svg";

        std::ofstream svg(filename.str().c_str());

        bg::svg_mapper<point_type> mapper(svg, 500, 500);

        mapper.add(g1);
        mapper.add(g2);

        mapper.map(g1, "fill-opacity:0.3;fill:rgb(51,51,153);stroke:rgb(51,51,153);stroke-width:3");
        mapper.map(g2, "fill-opacity:0.5;fill:rgb(153,204,0);stroke:rgb(153,204,0);stroke-width:3");


        for (typename Output::const_iterator it = output.begin(); it != output.end(); ++it)
        {
            mapper.map(*it,
                //sym ? "fill-opacity:0.2;stroke-opacity:0.4;fill:rgb(255,255,0);stroke:rgb(255,0,255);stroke-width:8" :
                "fill-opacity:0.2;stroke-opacity:0.4;fill:rgb(255,0,0);stroke:rgb(255,0,255);stroke-width:8");
        }
    }
#endif
}

template <typename OutputType, typename G1, typename G2>
std::string test_difference(std::string const& caseid, G1 const& g1, G2 const& g2,
        int expected_count, int expected_point_count,
        double expected_area,
        bool sym,
        ut_settings const& settings)
{
    typedef typename bg::coordinate_type<G1>::type coordinate_type;
    boost::ignore_unused<coordinate_type>();

    bg::model::multi_polygon<OutputType> result;

    if (sym)
    {
        bg::sym_difference(g1, g2, result);
    }
    else
    {
        bg::difference(g1, g2, result);
    }

    if (settings.remove_spikes)
    {
        bg::remove_spikes(result);
    }

    std::ostringstream return_string;
    return_string << bg::wkt(result);

    typename bg::default_area_result<G1>::type const area = bg::area(result);
    std::size_t const n = expected_point_count >= 0
                          ? bg::num_points(result) : 0;

#if ! defined(BOOST_GEOMETRY_NO_BOOST_TEST)
    if (settings.test_validity)
    {
        // std::cout << bg::dsv(result) << std::endl;
        std::string message;
        bool const valid = bg::is_valid(result, message);
        BOOST_CHECK_MESSAGE(valid,
            "difference: " << caseid << " not valid " << message);
    }
#endif

    difference_output(caseid, g1, g2, result);

#ifndef BOOST_GEOMETRY_DEBUG_ASSEMBLE
    {
        // Test inserter functionality
        // Test if inserter returns output-iterator (using Boost.Range copy)
        typedef typename bg::point_type<G1>::type point_type;
        typedef typename bg::rescale_policy_type<point_type>::type
            rescale_policy_type;

        rescale_policy_type rescale_policy
                = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

        std::vector<OutputType> inserted, array_with_one_empty_geometry;
        array_with_one_empty_geometry.push_back(OutputType());
        if (sym)
        {
            boost::copy(array_with_one_empty_geometry,
                bg::detail::sym_difference::sym_difference_insert<OutputType>
                    (g1, g2, rescale_policy, std::back_inserter(inserted)));
        }
        else
        {
            boost::copy(array_with_one_empty_geometry,
                bg::detail::difference::difference_insert<OutputType>(
                    g1, g2, rescale_policy, std::back_inserter(inserted)));
        }

        BOOST_CHECK_EQUAL(boost::size(result), boost::size(inserted) - 1);
    }
#endif



#if ! defined(BOOST_GEOMETRY_NO_BOOST_TEST)
    if (expected_point_count >= 0)
    {
        BOOST_CHECK_MESSAGE(bg::math::abs(int(n) - expected_point_count) < 3,
                "difference: " << caseid
                << " #points expected: " << expected_point_count
                << " detected: " << n
                << " type: " << (type_for_assert_message<G1, G2>())
                );
    }

    if (expected_count >= 0)
    {
        BOOST_CHECK_MESSAGE(int(result.size()) == expected_count,
                "difference: " << caseid
                << " #outputs expected: " << expected_count
                << " detected: " << result.size()
                << " type: " << (type_for_assert_message<G1, G2>())
                );
    }

    BOOST_CHECK_CLOSE(area, expected_area, settings.percentage);
#endif


    return return_string.str();
}


#ifdef BOOST_GEOMETRY_CHECK_WITH_POSTGIS
static int counter = 0;
#endif


template <typename OutputType, typename G1, typename G2>
std::string test_one(std::string const& caseid,
        std::string const& wkt1, std::string const& wkt2,
        int expected_count1,
        int expected_point_count1,
        double expected_area1,
        int expected_count2,
        int expected_point_count2,
        double expected_area2,
        int expected_count_s,
        int expected_point_count_s,
        double expected_area_s,
        ut_settings const& settings = ut_settings())
{
    G1 g1;
    bg::read_wkt(wkt1, g1);

    G2 g2;
    bg::read_wkt(wkt2, g2);

    bg::correct(g1);
    bg::correct(g2);

    std::string result = test_difference<OutputType>(caseid + "_a", g1, g2,
        expected_count1, expected_point_count1,
        expected_area1, false, settings);

#ifdef BOOST_GEOMETRY_DEBUG_ASSEMBLE
    return result;
#endif

    test_difference<OutputType>(caseid + "_b", g2, g1,
        expected_count2, expected_point_count2,
        expected_area2, false, settings);

    if (settings.sym_difference)
    {
        test_difference<OutputType>(caseid + "_s", g1, g2,
            expected_count_s,
            expected_point_count_s,
            expected_area_s,
            true, settings);
    }
    return result;
}

template <typename OutputType, typename G1, typename G2>
std::string test_one(std::string const& caseid,
        std::string const& wkt1, std::string const& wkt2,
        int expected_count1,
        int expected_point_count1,
        double expected_area1,
        int expected_count2,
        int expected_point_count2,
        double expected_area2,
        ut_settings const& settings = ut_settings())
{
    return test_one<OutputType, G1, G2>(caseid, wkt1, wkt2,
        expected_count1, expected_point_count1, expected_area1,
        expected_count2, expected_point_count2, expected_area2,
        expected_count1 + expected_count2,
        expected_point_count1 >= 0 && expected_point_count2 >= 0
            ? (expected_point_count1 + expected_point_count2) : -1,
        expected_area1 + expected_area2,
        settings);
}

template <typename OutputType, typename G1, typename G2>
void test_one_lp(std::string const& caseid,
        std::string const& wkt1, std::string const& wkt2,
        std::size_t expected_count,
        int expected_point_count,
        double expected_length)
{
    G1 g1;
    bg::read_wkt(wkt1, g1);

    G2 g2;
    bg::read_wkt(wkt2, g2);

    bg::correct(g1);

    std::vector<OutputType> pieces;
    bg::difference(g1, g2, pieces);

    typename bg::default_length_result<G1>::type length = 0;
    std::size_t n = 0;
    std::size_t piece_count = 0;
    for (typename std::vector<OutputType>::iterator it = pieces.begin();
            it != pieces.end();
            ++it)
    {
        if (expected_point_count >= 0)
        {
            n += bg::num_points(*it);
        }
        piece_count++;
        length += bg::length(*it);
    }

    BOOST_CHECK_MESSAGE(piece_count == expected_count,
            "difference: " << caseid
            << " #outputs expected: " << expected_count
            << " detected: " << pieces.size()
            );

    if (expected_point_count >= 0)
    {
        BOOST_CHECK_MESSAGE(n == std::size_t(expected_point_count),
                "difference: " << caseid
                << " #points expected: " << std::size_t(expected_point_count)
                << " detected: " << n
                << " type: " << (type_for_assert_message<G1, G2>())
                );
    }

    BOOST_CHECK_CLOSE(length, expected_length, 0.001);

    std::string lp = "lp_";
    difference_output(lp + caseid, g1, g2, pieces);
}


#endif
