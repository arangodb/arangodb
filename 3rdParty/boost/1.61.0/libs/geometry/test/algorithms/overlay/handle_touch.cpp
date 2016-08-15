// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_DEBUG_IDENTIFIER
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER

//#define BOOST_GEOMETRY_DEBUG_HANDLE_TOUCH


#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>


#include <geometry_test_common.hpp>


#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/enrichment_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/traversal_info.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/enrich_intersection_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/handle_touch.hpp>

#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>

#include <boost/geometry/policies/robustness/get_rescale_policy.hpp>

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/correct.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>


#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>


namespace detail
{

template
<
        typename G1, typename G2,
        bg::detail::overlay::operation_type Direction,
        bool Reverse1, bool Reverse2
        >
struct test_handle_touch
{

    static void apply(std::string const& case_id,
                      std::size_t expected_traverse,
                      std::size_t expected_skipped,
                      std::size_t expected_start,
                      G1 const& g1, G2 const& g2)
    {

        typedef typename bg::strategy::side::services::default_strategy
                <
                typename bg::cs_tag<G1>::type
                >::type side_strategy_type;

        typedef typename bg::point_type<G2>::type point_type;
        typedef typename bg::rescale_policy_type<point_type>::type
                rescale_policy_type;

        rescale_policy_type rescale_policy
                = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

        typedef bg::detail::overlay::traversal_turn_info
                <
                point_type,
                typename bg::segment_ratio_type<point_type, rescale_policy_type>::type
                > turn_info;
        std::vector<turn_info> turns;

        bg::detail::get_turns::no_interrupt_policy policy;
        bg::get_turns<Reverse1, Reverse2, bg::detail::overlay::assign_null_policy>(g1, g2, rescale_policy, turns, policy);
        bg::enrich_intersection_points<Reverse1, Reverse2>(turns,
                                                           Direction == 1 ? bg::detail::overlay::operation_union
                                                                          : bg::detail::overlay::operation_intersection,
                                                           g1, g2, rescale_policy, side_strategy_type());

        typedef bg::model::ring<typename bg::point_type<G2>::type> ring_type;
        typedef std::vector<ring_type> out_vector;

#ifdef BOOST_GEOMETRY_DEBUG_HANDLE_TOUCH
        std::cout << "*** Case: " << case_id << std::endl;
#endif

        bg::detail::overlay::handle_touch(Direction, turns);

        // Check number of resulting u/u turns

        std::size_t uu_traverse = 0;
        std::size_t uu_skipped = 0;
        std::size_t uu_start = 0;
        BOOST_FOREACH(turn_info const& turn, turns)
        {
            if (turn.both(bg::detail::overlay::operation_union))
            {
                if (turn.switch_source)
                {
                    uu_traverse++;
                }
                else
                {
                    uu_skipped++;
                }
                if (turn.selectable_start)
                {
                    uu_start++;
                }
            }
        }

        BOOST_CHECK_MESSAGE(expected_traverse == uu_traverse,
                            "handle_touch: " << case_id
                            << " traverse expected: " << expected_traverse
                            << " detected: " << uu_traverse
                            << " type: " << string_from_type
                            <typename bg::coordinate_type<G1>::type>::name());
        BOOST_CHECK_MESSAGE(expected_skipped == uu_skipped,
                            "handle_touch: " << case_id
                            << " skipped expected: " << expected_skipped
                            << " detected: " << uu_skipped
                            << " type: " << string_from_type
                            <typename bg::coordinate_type<G1>::type>::name());
        BOOST_CHECK_MESSAGE(expected_start == uu_start,
                            "handle_touch: " << case_id
                            << " start expected: " << expected_start
                            << " detected: " << uu_skipped
                            << " type: " << string_from_type
                            <typename bg::coordinate_type<G1>::type>::name());

#if defined(TEST_WITH_SVG)
        {
            std::ostringstream filename;
            filename << "handle_touch"
                     << "_" << case_id
                     << "_" << string_from_type<typename bg::coordinate_type<G1>::type>::name()
                     << ".svg";

            std::ofstream svg(filename.str().c_str());

            bg::svg_mapper<typename bg::point_type<G2>::type> mapper(svg, 500, 500);
            mapper.add(g1);
            mapper.add(g2);

            // Input shapes in green/blue
            mapper.map(g1, "fill-opacity:0.5;fill:rgb(153,204,0);"
                       "stroke:rgb(153,204,0);stroke-width:3");
            mapper.map(g2, "fill-opacity:0.3;fill:rgb(51,51,153);"
                       "stroke:rgb(51,51,153);stroke-width:3");


            // turn points in orange, + enrichment/traversal info
            typedef typename bg::coordinate_type<G1>::type coordinate_type;

            // Simple map to avoid two texts at same place (note that can still overlap!)
            std::map<std::pair<int, int>, int> offsets;
            int index = 0;
            int const margin = 5;

            BOOST_FOREACH(turn_info const& turn, turns)
            {
                int lineheight = 8;
                mapper.map(turn.point, "fill:rgb(255,128,0);"
                           "stroke:rgb(0,0,0);stroke-width:1", 3);

                {
                    coordinate_type half = 0.5;
                    coordinate_type ten = 10;
                    // Map characteristics
                    // Create a rounded off point
                    std::pair<int, int> p
                            = std::make_pair(
                                  boost::numeric_cast<int>(half
                                                           + ten * bg::get<0>(turn.point)),
                                  boost::numeric_cast<int>(half
                                                           + ten * bg::get<1>(turn.point))
                                  );

                    std::string color = "fill:rgb(0,0,0);";
                    std::string fontsize = "font-size:8px;";

                    if (turn.both(bg::detail::overlay::operation_union))
                    {
                        // Adapt color to give visual feedback in SVG
                        if (turn.switch_source && turn.selectable_start)
                        {
                            color = "fill:rgb(0,0,255);"; // blue
                        }
                        else if (turn.switch_source)
                        {
                            color = "fill:rgb(0,128,0);"; // green
                        }
                        else
                        {
                            color = "fill:rgb(255,0,0);"; // red
                        }
                    }
                    else if (turn.discarded)
                    {
                        color = "fill:rgb(92,92,92);";
                        fontsize = "font-size:6px;";
                        lineheight = 6;
                    }
                    const std::string style = color + fontsize + "font-family:Arial;";

                    {
                        std::ostringstream out;
                        out << index
                            << ": " << bg::method_char(turn.method)
                            << std::endl
                            << "op: " << bg::operation_char(turn.operations[0].operation)
                                << " / " << bg::operation_char(turn.operations[1].operation)
                                << std::endl;

                        if (turn.operations[0].enriched.next_ip_index != -1)
                        {
                            out << "ip: " << turn.operations[0].enriched.next_ip_index;
                        }
                        else
                        {
                            out << "vx: " << turn.operations[0].enriched.travels_to_vertex_index
                                << " -> ip: "  << turn.operations[0].enriched.travels_to_ip_index;
                        }
                        out << " / ";
                        if (turn.operations[1].enriched.next_ip_index != -1)
                        {
                            out << "ip: " << turn.operations[1].enriched.next_ip_index;
                        }
                        else
                        {
                            out << "vx: " << turn.operations[1].enriched.travels_to_vertex_index
                                << " -> ip: " << turn.operations[1].enriched.travels_to_ip_index;
                        }

                        out << std::endl;



                        offsets[p] += lineheight;
                        int offset = offsets[p];
                        offsets[p] += lineheight * 3;
                        mapper.text(turn.point, out.str(), style, margin, offset, lineheight);
                    }
                    index++;
                }
            }
        }
#endif
    }
};
}

template
<
        typename G1, typename G2,
        bg::detail::overlay::operation_type Direction,
        bool Reverse1 = false,
        bool Reverse2 = false
        >
struct test_handle_touch
{
    typedef detail::test_handle_touch
    <
    G1, G2, Direction, Reverse1, Reverse2
    > detail_test_handle_touch;

    inline static void apply(std::string const& case_id,
                             std::size_t expected_traverse,
                             std::size_t expected_skipped,
                             std::size_t expected_start,
                             std::string const& wkt1, std::string const& wkt2)
    {
        if (wkt1.empty() || wkt2.empty())
        {
            return;
        }

        G1 g1;
        bg::read_wkt(wkt1, g1);

        G2 g2;
        bg::read_wkt(wkt2, g2);

        bg::correct(g1);
        bg::correct(g2);

        detail_test_handle_touch::apply(case_id,
                                        expected_traverse,
                                        expected_skipped,
                                        expected_start,
                                        g1, g2);

    }
};

template <typename Polygon>
void test_geometries()
{
    namespace ov = bg::detail::overlay;

    typedef test_handle_touch
            <
                Polygon, Polygon,
                ov::operation_union
            > test_union;

    test_union::apply("case_36", 1, 0, 0, case_36[0], case_36[1]);
    test_union::apply("case_80", 1, 0, 0, case_80[0], case_80[1]);
    test_union::apply("case_81", 1, 0, 0, case_81[0], case_81[1]);
    test_union::apply("case_82", 0, 2, 0, case_82[0], case_82[1]);
    test_union::apply("case_83", 2, 0, 1, case_83[0], case_83[1]);
    test_union::apply("case_84", 0, 3, 0, case_84[0], case_84[1]);
    test_union::apply("case_85", 1, 0, 0, case_85[0], case_85[1]);
}


template <typename MultiPolygon>
void test_multi_geometries()
{
    namespace ov = bg::detail::overlay;

    typedef test_handle_touch
            <
                MultiPolygon, MultiPolygon,
                ov::operation_union
            > test_union;

    test_union::apply
            (
                "uu_case_1", 0, 1, 0,
                "MULTIPOLYGON(((4 0,2 2,4 4,6 2,4 0)))",
                "MULTIPOLYGON(((4 4,2 6,4 8,6 6,4 4)))"
                );
    test_union::apply
            (
                "uu_case_2", 0, 2, 0,
                "MULTIPOLYGON(((0 0,0 2,2 4,4 2,6 4,8 2,8 0,0 0)))",
                "MULTIPOLYGON(((0 8,8 8,8 6,6 4,4 6,2 4,0 6,0 8)))"
                );

    // Provided by Menelaos (1)
    test_union::apply
            (
                "uu_case_3", 0, 2, 0,
                "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((15 5,15 10,20 10,20 5,15 5)))",
                "MULTIPOLYGON(((10 0,15 5,15 0,10 0)),((10 5,10 10,15 10,15 5,10 5)))"
                );
    // Provided by Menelaos (2)
    test_union::apply
            (
                "uu_case_4", 1, 0, 0,
                "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((15 5,15 10,20 10,20 5,15 5)))",
                "MULTIPOLYGON(((10 0,15 5,20 5,20 0,10 0)),((10 5,10 10,15 10,15 5,10 5)))"
                );

    // Mailed by Barend
    test_union::apply
            (
                "uu_case_5", 1, 0, 0,
                "MULTIPOLYGON(((4 0,2 2,4 4,6 2,4 0)),((4 6,6 8,8 6,6 4,4 6)))",
                "MULTIPOLYGON(((4 4,2 6,4 8,6 6,4 4)),((4 2,7 6,8 3,4 2)))"
                );

    // Formerly referred to as a
    test_union::apply
            (
                "uu_case_6", 2, 0, 0,
                "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((7 7,7 11,10 11,10 7,7 7)))",
                "MULTIPOLYGON(((6 6,6 8,8 8,8 6,6 6)),((6 10,6 12,8 12,8 10,6 10)),((9 9,11 9,11 2,3 2,3 9,5 9,5 3,9 3,9 9)))"
                );
    // Should result in 1 polygon with 2 holes
    // "POLYGON((4 9,4 10,6 10,6 12,8 12,8 11,10 11,10 9,11 9,11 2,3 2,3 9,4 9),(6 10,6 8,7 8,7 10,6 10),(6 8,5 8,5 3,9 3,9 7,8 7,8 6,6 6,6 8))"

    // Formerly referred to as b
    test_union::apply
            (
                "uu_case_7", 0, 2, 0,
                "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((7 7,7 11,10 11,10 7,7 7)))",
                "MULTIPOLYGON(((6 6,6 8,8 8,8 6,6 6)),((6 10,6 12,8 12,8 10,6 10)))"
                );
    // Should result in 2 polygons
    // "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((7 8,7 10,6 10,6 12,8 12,8 11,10 11,10 7,8 7,8 6,6 6,6 8,7 8)))"

    // Formerly referred to as c
    test_union::apply
            (
                "uu_case_8", 0, 4, 0,
                "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((8 8,8 10,10 10,10 8,8 8)),((7 11,7 13,13 13,13 5,7 5,7 7,11 7,11 11,7 11)))",
                "MULTIPOLYGON(((6 6,6 8,8 8,8 6,6 6)),((6 10,6 12,8 12,8 10,6 10)))"
                );

    // Shoud result in 3 polygons:
    // "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((8 8,8 10,10 10,10 8,8 8)),((7 12,7 13,13 13,13 5,7 5,7 6,6 6,6 8,8 8,8 7,11 7,11 11,8 11,8 10,6 10,6 12,7 12)))"

    // Formerly referred to as d
    test_union::apply
            (
                "uu_case_9", 0, 2, 0,
                "MULTIPOLYGON(((2 4,2 6,4 6,4 4,2 4)),((6 4,6 6,8 6,8 4,6 4)),((1 0,1 3,9 3,9 0,1 0)))",
                "MULTIPOLYGON(((0 2,0 4,2 4,2 2,0 2)),((8 2,8 4,10 4,10 2,8 2)),((3 5,3 7,7 7,7 5,3 5)))"
                );
    // Should result in 2 polygons:
    // "MULTIPOLYGON(((2 4,2 6,3 6,3 7,7 7,7 6,8 6,8 4,6 4,6 5,4 5,4 4,2 4)),((1 0,1 2,0 2,0 4,2 4,2 3,8 3,8 4,10 4,10 2,9 2,9 0,1 0)))"

    // With a c/c turn
    test_union::apply
            (
                "uu_case_10", 1, 0, 0,
                "MULTIPOLYGON(((6 4,6 9,9 9,9 6,11 6,11 4,6 4)),((10 7,10 10,12 10,12 7,10 7)))",
                "MULTIPOLYGON(((10 5,10 8,12 8,12 5,10 5)),((6 10,8 12,10 10,8 8,6 10)))"
                );

    // With c/c turns in both involved polygons
    test_union::apply
            (
                "uu_case_11", 1, 0, 0,
                "MULTIPOLYGON(((7 4,7 8,9 8,9 6,11 6,11 4,7 4)),((10 7,10 10,12 10,12 7,10 7)))",
                "MULTIPOLYGON(((10 5,10 8,12 8,12 5,10 5)),((7 7,7 10,10 10,9 9,9 7,7 7)))"
                );

    // Same but here c/c not directly involved in the turns itself
    // (This one breaks if continue is not checked in handle_touch)
    test_union::apply
            (
                "uu_case_12", 1, 0, 0,
                "MULTIPOLYGON(((10 8,10 10,12 10,12 8,10 8)),((10 4,10 7,12 7,12 4,10 4)),((7 5,7 8,9 8,9 5,7 5)))",
                "MULTIPOLYGON(((7 3,7 6,9 6,9 5,11 5,11 3,7 3)),((10 6,10 9,12 9,12 6,10 6)),((7 7,7 10,10 10,9 9,9 7,7 7)))"
                );

    test_union::apply
        (
            "case_62_multi", 0, 1, 0, case_62_multi[0], case_62_multi[1]
        );
    test_union::apply
        (
            "case_63_multi", 0, 1, 0, case_63_multi[0], case_63_multi[1]
        );
    test_union::apply
        (
            "case_65_multi", 0, 2, 0, case_65_multi[0], case_65_multi[1]
        );
    test_union::apply
        (
            "case_66_multi", 0, 2, 0, case_66_multi[0], case_66_multi[1]
        );
    test_union::apply
        (
            "case_75_multi", 0, 4, 0, case_75_multi[0], case_75_multi[1]
        );
    test_union::apply
        (
            "case_76_multi", 0, 5, 0, case_76_multi[0], case_76_multi[1]
        );
    test_union::apply
        (
            "case_101_multi", 2, 0, 0, case_101_multi[0], case_101_multi[1]
        );
    test_union::apply
        (
            "case_108_multi", 0, 0, 0, case_108_multi[0], case_108_multi[1]
        );

    // NOTE: this result is still to be checked
    test_union::apply
        (
            "case_recursive_boxes_3", 8, 18, 0,
            case_recursive_boxes_3[0], case_recursive_boxes_3[1]
        );

}


template <typename T, bool Clockwise>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;

    test_multi_geometries
        <
            bg::model::multi_polygon
                <
                bg::model::polygon<point_type, Clockwise>
                >
        >();

    test_geometries<bg::model::polygon<point_type, Clockwise> >();
}


int test_main(int, char* [])
{
    test_all<double, true>();

    return 0;
}
