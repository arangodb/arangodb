// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2017-2020.
// Modifications copyright (c) 2017-2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/overlay/sort_by_side.hpp>

#include <boost/geometry/strategies/strategies.hpp>  // for equals/within
#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>

#if defined(TEST_WITH_SVG)
#include "debug_sort_by_side_svg.hpp"
#endif

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

template
<
    typename Geometry, typename Point,
    typename RobustPolicy, typename Strategy
>
std::vector<std::size_t> apply_get_turns(std::string const& case_id,
            Geometry const& geometry1, Geometry const& geometry2,
            Point const& turn_point, Point const& origin_point,
            RobustPolicy const& robust_policy,
            Strategy const& strategy,
            std::size_t expected_open_count,
            std::size_t expected_max_rank,
            std::vector<bg::signed_size_type> const& expected_right_count)
{
    using namespace boost::geometry;

    std::vector<std::size_t> result;

//todo: maybe should be enriched to count left/right - but can also be counted from ranks
    typedef typename bg::point_type<Geometry>::type point_type;
    typedef bg::detail::overlay::turn_info
    <
        point_type,
        typename bg::detail::segment_ratio_type<point_type, RobustPolicy>::type
    > turn_info;
    typedef std::deque<turn_info> turn_container_type;

    turn_container_type turns;

    detail::get_turns::no_interrupt_policy policy;
    bg::get_turns
        <
            false, false,
            detail::overlay::assign_null_policy
        >(geometry1, geometry2, strategy, robust_policy, turns, policy);


    // Define sorter, sorting counter-clockwise such that polygons are on the
    // right side
    typedef decltype(strategy.side()) side_strategy;
    typedef bg::detail::overlay::sort_by_side::side_sorter
        <
            false, false, overlay_union,
            point_type, side_strategy, std::less<int>
        > sbs_type;

    sbs_type sbs(strategy.side());

    std::cout << "Case: " << case_id << std::endl;

    bool has_origin = false;
    for (std::size_t turn_index = 0; turn_index < turns.size(); turn_index++)
    {
        turn_info const& turn = turns[turn_index];

        if (bg::equals(turn.point, turn_point))
        {
//            std::cout << "Found turn: " << turn_index << std::endl;
            for (int i = 0; i < 2; i++)
            {
                Point point1, point2, point3;
                bg::copy_segment_points<false, false>(geometry1, geometry2,
                        turn.operations[i].seg_id, point1, point2, point3);
                bool const is_origin = ! has_origin && bg::equals(point1, origin_point);
                if (is_origin)
                {
                    has_origin = true;
                }

                sbs.add(turn, turn.operations[i], turn_index, i,
                        geometry1, geometry2, is_origin);

            }
        }
    }

    BOOST_CHECK_MESSAGE(has_origin,
                        "  caseid="  << case_id
                        << " origin not found");

    if (!has_origin)
    {
        for (std::size_t turn_index = 0; turn_index < turns.size(); turn_index++)
        {
            turn_info const& turn = turns[turn_index];
            if (bg::equals(turn.point, turn_point))
            {
                for (int i = 0; i < 2; i++)
                {
                    Point point1, point2, point3;
                    bg::copy_segment_points<false, false>(geometry1, geometry2,
                            turn.operations[i].seg_id, point1, point2, point3);
//                    std::cout << "Turn " << turn_index << " op " << i << " point1=" << bg::wkt(point1) << std::endl;
                }
            }
        }
        return result;
    }


    sbs.apply(turn_point);

    sbs.find_open();
    sbs.assign_zones(detail::overlay::operation_union);

    std::size_t const open_count = sbs.open_count(detail::overlay::operation_union);
    std::size_t const max_rank = sbs.m_ranked_points.back().rank;
    std::vector<bg::signed_size_type> right_count(max_rank + 1, -1);

    int previous_rank = -1;
    int previous_to_rank = -1;
    for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
    {
        typename sbs_type::rp const& ranked_point = sbs.m_ranked_points[i];

        int const rank = static_cast<int>(ranked_point.rank);
        bool const set_right = rank != previous_to_rank;
        if (rank != previous_rank)
        {
            BOOST_CHECK_MESSAGE(rank == previous_rank + 1,
                                "  caseid="  << case_id
                                << " ranks: conflict in ranks="  << ranked_point.rank
                                << " vs " << previous_rank + 1);
            previous_rank = rank;
        }

        if (ranked_point.direction != bg::detail::overlay::sort_by_side::dir_to)
        {
            continue;
        }

        previous_to_rank = rank;
        if (set_right)
        {
            right_count[rank] = ranked_point.count_right;
        }
        else
        {
            BOOST_CHECK_MESSAGE(right_count[rank] == int(ranked_point.count_right),
                                "  caseid="  << case_id
                                << " ranks: conflict in right_count="  << ranked_point.count_right
                                << " vs " << right_count[rank]);
        }

    }
    BOOST_CHECK_MESSAGE(open_count == expected_open_count,
                        "  caseid="  << case_id
                        << " open_count: expected="  << expected_open_count
                        << " detected="  << open_count);

    BOOST_CHECK_MESSAGE(max_rank == expected_max_rank,
                        "  caseid="  << case_id
                        << " max_rank: expected="  << expected_max_rank
                        << " detected="  << max_rank);

    BOOST_CHECK_MESSAGE(right_count == expected_right_count,
                        "  caseid="  << case_id
                        << " right count: expected="  << as_string(expected_right_count)
                        << " detected="  << as_string(right_count));

#if defined(TEST_WITH_SVG)
    debug::sorted_side_map(case_id, sbs, turn_point, geometry1, geometry2);
#endif

    return result;
}


template <typename T>
void test_basic(std::string const& case_id,
                std::string const& wkt1,
                std::string const& wkt2,
                std::string const& wkt_turn_point,
                std::string const& wkt_origin_point,
                std::size_t expected_open_count,
                std::size_t expected_max_rank,
                std::vector<bg::signed_size_type> const& expected_right_count)
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    typedef bg::model::polygon<point_type> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    multi_polygon g1;
    bg::read_wkt(wkt1, g1);

    multi_polygon g2;
    bg::read_wkt(wkt2, g2);

    point_type turn_point, origin_point;
    bg::read_wkt(wkt_turn_point, turn_point);
    bg::read_wkt(wkt_origin_point, origin_point);

    // Reverse if necessary
    bg::correct(g1);
    bg::correct(g2);

    typedef typename bg::rescale_overlay_policy_type
    <
        multi_polygon,
        multi_polygon
    >::type rescale_policy_type;

    rescale_policy_type robust_policy
        = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

    typedef typename bg::strategies::relate::services::default_strategy
        <
            multi_polygon, multi_polygon
        >::type strategy_type;

    strategy_type strategy;

    apply_get_turns(case_id, g1, g2, turn_point, origin_point,
        robust_policy, strategy,
        expected_open_count, expected_max_rank, expected_right_count);
}

using boost::assign::list_of;

template <typename T>
void test_all()
{
    test_basic<T>("simplex",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 0,1, 0)))",
      "POINT(1 1)", "POINT(1 0)",
      2, 3, list_of(-1)(1)(-1)(1));

    test_basic<T>("dup1",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 0,1, 0)),((0 2,1 2,1 1,0 1,0 2)))",
      "POINT(1 1)", "POINT(1 0)",
      2, 3, list_of(-1)(1)(-1)(2));

    test_basic<T>("dup2",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)),((1 0,1 1,2 1,2 0,1, 0)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 0,1, 0)))",
      "POINT(1 1)", "POINT(1 0)",
      2, 3, list_of(-1)(2)(-1)(1));

    test_basic<T>("dup3",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)),((1 0,1 1,2 1,2 0,1, 0)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 0,1, 0)),((0 2,1 2,1 1,0 1,0 2)))",
      "POINT(1 1)", "POINT(1 0)",
      2, 3, list_of(-1)(2)(-1)(2));

    test_basic<T>("threequart1",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)),((1 0,1 1,2 1,2 0,1, 0)))",
      "MULTIPOLYGON(((1 2,2 2,2 1,1 1,1 2)))",
      "POINT(1 1)", "POINT(1 0)",
      1, 3, list_of(-1)(1)(1)(1));

    test_basic<T>("threequart2",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)),((1 0,1 1,2 1,2 0,1, 0)))",
      "MULTIPOLYGON(((1 2,2 2,2 1,1 1,1 2)),((2 0,1 0,1 1,2 0)))",
      "POINT(1 1)", "POINT(1 0)",
      1, 4, list_of(-1)(2)(1)(1)(1));

    test_basic<T>("threequart3",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)),((1 0,1 1,2 1,2 0,1, 0)))",
      "MULTIPOLYGON(((1 2,2 2,2 1,1 1,1 2)),((2 0,1 0,1 1,2 0)),((0 1,0 2,1 1,0 1)))",
      "POINT(1 1)", "POINT(1 0)",
      1, 5, list_of(-1)(2)(1)(1)(-1)(2));

    test_basic<T>("full1",
      "MULTIPOLYGON(((0 2,1 2,1 1,0 1,0 2)),((1 0,1 1,2 1,2 0,1, 0)))",
      "MULTIPOLYGON(((1 2,2 2,2 1,1 1,1 2)),((0 0,0 1,1 1,1 0,0 0)))",
      "POINT(1 1)", "POINT(1 0)",
      0, 3, list_of(1)(1)(1)(1));

    test_basic<T>("hole1",
      "MULTIPOLYGON(((0 0,0 3,2 3,2 2,3 2,3 0,0 0),(1 1,2 1,2 2,1 2,1 1)),((4 2,3 2,3 3,4 3,4 2)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 2,1 2,1 4,4 4,4 0,1, 0),(3 2,3 3,2 3,2 2,3 2)))",
      "POINT(1 2)", "POINT(2 2)",
      1, 2, list_of(-1)(2)(1));

    test_basic<T>("hole2",
      "MULTIPOLYGON(((0 0,0 3,2 3,2 2,3 2,3 0,0 0),(1 1,2 1,2 2,1 2,1 1)),((4 2,3 2,3 3,4 3,4 2)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 2,1 2,1 4,4 4,4 0,1, 0),(3 2,3 3,2 3,2 2,3 2)))",
      "POINT(2 2)", "POINT(2 1)",
      2, 3, list_of(-1)(2)(-1)(2));

    test_basic<T>("hole3",
      "MULTIPOLYGON(((0 0,0 3,2 3,2 2,3 2,3 0,0 0),(1 1,2 1,2 2,1 2,1 1)),((4 2,3 2,3 3,4 3,4 2)))",
      "MULTIPOLYGON(((1 0,1 1,2 1,2 2,1 2,1 4,4 4,4 0,1, 0),(3 2,3 3,2 3,2 2,3 2)))",
      "POINT(3 2)", "POINT(2 2)",
      1, 3, list_of(-1)(2)(-1)(2));
}


int test_main(int, char* [])
{
    test_all<double>();
    return 0;
}
