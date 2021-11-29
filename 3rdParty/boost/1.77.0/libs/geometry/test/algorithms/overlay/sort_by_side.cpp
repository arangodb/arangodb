// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2017-2020.
// Modifications copyright (c) 2017-2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>

#include "multi_overlay_cases.hpp"


namespace
{

template <typename T>
std::string as_string(std::vector<T> const& v)
{
    std::stringstream out;
    bool first = true;
    BOOST_FOREACH(T const& value, v)
    {
        out << (first ? "[" : " , ") << value;
        first = false;
    }
    out << (first ? "" : "]");
    return out.str();
}

}

// Adapted copy of handle_colocations::gather_cluster_properties
template
<
    bool Reverse1, bool Reverse2,
    bg::overlay_type OverlayType,
    typename Clusters,
    typename Turns,
    typename Geometry1,
    typename Geometry2,
    typename SideStrategy
>
std::vector<std::size_t> gather_cluster_properties(
        Clusters& clusters, Turns& turns,
        bg::detail::overlay::operation_type for_operation,
        Geometry1 const& geometry1, Geometry2 const& geometry2,
        SideStrategy const& strategy)
{
    using namespace boost::geometry;
    using namespace boost::geometry::detail::overlay;

    std::vector<std::size_t> result;

    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::point_type point_type;
    typedef typename turn_type::turn_operation_type turn_operation_type;

    // Define sorter, sorting counter-clockwise such that polygons are on the
    // right side
    typedef sort_by_side::side_sorter
        <
            Reverse1, Reverse2, OverlayType, point_type, SideStrategy, std::less<int>
        > sbs_type;

    for (typename Clusters::iterator mit = clusters.begin();
         mit != clusters.end(); ++mit)
    {
        cluster_info& cinfo = mit->second;
        std::set<signed_size_type> const& ids = cinfo.turn_indices;
        if (ids.empty())
        {
            return result;
        }

        sbs_type sbs(strategy);
        point_type turn_point; // should be all the same for all turns in cluster

        bool first = true;
        for (typename std::set<signed_size_type>::const_iterator sit = ids.begin();
             sit != ids.end(); ++sit)
        {
            signed_size_type turn_index = *sit;
            turn_type const& turn = turns[turn_index];
            if (first)
            {
                turn_point = turn.point;
            }
            for (int i = 0; i < 2; i++)
            {
                turn_operation_type const& op = turn.operations[i];
                sbs.add(turn, op, turn_index, i, geometry1, geometry2, first);
                first = false;
            }
        }
        sbs.apply(turn_point);

        sbs.find_open();
        sbs.assign_zones(for_operation);

        cinfo.open_count = sbs.open_count(for_operation);
        result.push_back(cinfo.open_count);
    }
    return result;
}


// Adapted copy of overlay::apply
template
<
    bg::overlay_type OverlayType,
    bool Reverse1, bool Reverse2, bool ReverseOut,
    typename GeometryOut,
    typename Geometry1, typename Geometry2,
    typename RobustPolicy, typename Strategy
>
std::vector<std::size_t> apply_overlay(
            Geometry1 const& geometry1, Geometry2 const& geometry2,
            RobustPolicy const& robust_policy,
            Strategy const& strategy)
{
    using namespace boost::geometry;

    typedef typename bg::point_type<GeometryOut>::type point_type;
    typedef bg::detail::overlay::traversal_turn_info
    <
        point_type,
        typename bg::detail::segment_ratio_type<point_type, RobustPolicy>::type
    > turn_info;
    typedef std::deque<turn_info> turn_container_type;

    // Define the clusters, mapping cluster_id -> turns
    typedef std::map
        <
            signed_size_type,
            bg::detail::overlay::cluster_info
        > cluster_type;

    turn_container_type turns;

    detail::get_turns::no_interrupt_policy policy;
    bg::get_turns
        <
            Reverse1, Reverse2,
            detail::overlay::assign_null_policy
        >(geometry1, geometry2, strategy, robust_policy, turns, policy);

    cluster_type clusters;

    bg::enrich_intersection_points<Reverse1, Reverse2, OverlayType>(turns,
            clusters, geometry1, geometry2, robust_policy, strategy);

    // Gather cluster properties, with test option
    return ::gather_cluster_properties<Reverse1, Reverse2, OverlayType>(
            clusters, turns, bg::detail::overlay::operation_from_overlay<OverlayType>::value,
                geometry1, geometry2, strategy.side());
}


template <typename Geometry, bg::overlay_type OverlayType>
void test_sort_by_side(std::string const& case_id,
        std::string const& wkt1, std::string const& wkt2,
        std::vector<std::size_t> const& expected_open_count)
{
//    std::cout << case_id << std::endl;

    Geometry g1;
    bg::read_wkt(wkt1, g1);

    Geometry g2;
    bg::read_wkt(wkt2, g2);

    // Reverse if necessary
    bg::correct(g1);
    bg::correct(g2);

    typedef typename boost::range_value<Geometry>::type geometry_out;

    typedef typename bg::rescale_overlay_policy_type
    <
        Geometry,
        Geometry
    >::type rescale_policy_type;

    rescale_policy_type robust_policy
        = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

    typedef typename bg::strategies::relate::services::default_strategy
        <
            Geometry, Geometry
        >::type strategy_type;

    strategy_type strategy;

    std::vector<std::size_t> result = ::apply_overlay
                                        <
                                            OverlayType, false, false, false, geometry_out
                                        >(g1, g2, robust_policy, strategy);

    BOOST_CHECK_MESSAGE(result == expected_open_count,
                        "  caseid="  << case_id
                        << " open count: expected="  << as_string(expected_open_count)
                        << " detected="  << as_string(result));
}


// Define two small macro's to avoid repetitions of testcases/names etc
#define TEST_INT(caseid, exp) { (test_sort_by_side<multi_polygon, bg::overlay_intersection>) \
    ( #caseid "_int", caseid[0], caseid[1], exp); }

#define TEST_UNION(caseid, exp) { (test_sort_by_side<multi_polygon, bg::overlay_union>) \
    ( #caseid "_union", caseid[0], caseid[1], exp); }

using boost::assign::list_of;

template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    typedef bg::model::polygon<point_type> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    // Selection of test cases having only one cluster

    TEST_INT(case_64_multi, list_of(1));
    TEST_INT(case_72_multi, list_of(3));
    TEST_INT(case_107_multi, list_of(2));
    TEST_INT(case_123_multi, list_of(3));
    TEST_INT(case_124_multi, list_of(3));
    TEST_INT(case_recursive_boxes_10, list_of(2));
    TEST_INT(case_recursive_boxes_20, list_of(2));
    TEST_INT(case_recursive_boxes_21, list_of(1));
    TEST_INT(case_recursive_boxes_22, list_of(0));

    TEST_UNION(case_recursive_boxes_46, list_of(2)(1)(2)(1)(1)(2)(1));

    TEST_UNION(case_62_multi, list_of(2));
    TEST_UNION(case_63_multi, list_of(2));
    TEST_UNION(case_64_multi, list_of(1));
    TEST_UNION(case_107_multi, list_of(1));
    TEST_UNION(case_123_multi, list_of(1));
    TEST_UNION(case_124_multi, list_of(1));
    TEST_UNION(case_recursive_boxes_10, list_of(1));
    TEST_UNION(case_recursive_boxes_18, list_of(3));
    TEST_UNION(case_recursive_boxes_19, list_of(3));
    TEST_UNION(case_recursive_boxes_20, list_of(2));
    TEST_UNION(case_recursive_boxes_21, list_of(1));
    TEST_UNION(case_recursive_boxes_22, list_of(1));
    TEST_UNION(case_recursive_boxes_23, list_of(3));
}

int test_main(int, char* [])
{
    test_all<double>();
    return 0;
}
