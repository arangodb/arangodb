// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017-2020.
// Modifications copyright (c) 2017-2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/type_traits/is_same.hpp>

#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

#include <geometry_test_common.hpp>
#include <algorithms/check_validity.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/geometries/geometries.hpp>

//#include <boost/geometry/extensions/algorithms/inverse.hpp>

#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

#include "multi_overlay_cases.hpp"


#if defined(TEST_WITH_SVG)
template <typename Mapper>
struct map_visitor
{
    map_visitor(Mapper& mapper)
        : m_mapper(mapper)
        , m_traverse_seq(0)
        , m_do_output(true)
    {}

    void print(char const* header)
    {}

    template <typename Turns>
    void print(char const* header, Turns const& turns, int turn_index)
    {
        std::string style = "fill:rgb(0,0,0);font-family:Arial;font-size:6px";
        stream(turns, turns[turn_index], turns[turn_index].operations[0], header, style);
    }

    template <typename Turns>
    void print(char const* header, Turns const& turns, int turn_index, int op_index)
    {
        std::string style = "fill:rgb(0,0,0);font-family:Arial;font-size:6px";
        stream(turns, turns[turn_index], turns[turn_index].operations[op_index], header, style);
    }

    template <typename Turns>
    void visit_turns(int phase, Turns const& turns)
    {
        typedef typename boost::range_value<Turns>::type turn_type;
        int index = 0;
        BOOST_FOREACH(turn_type const& turn, turns)
        {
            switch (phase)
            {
                case 1 :
                    m_mapper.map(turn.point, "fill:rgb(255,128,0);"
                            "stroke:rgb(0,0,0);stroke-width:1", 3);
                    break;
                case 11 :
                    m_mapper.map(turn.point, "fill:rgb(92,255,0);" // Greenish
                            "stroke:rgb(0,0,0);stroke-width:1", 3);
                    break;
                case 21 :
                    m_mapper.map(turn.point, "fill:rgb(0,128,255);" // Blueish
                            "stroke:rgb(0,0,0);stroke-width:1", 3);
                    break;
                case 3 :
                    label_turn(index, turn);
                    break;
            }
            index++;
        }
    }

    template <typename Turns, typename Turn, typename Operation>
    std::string stream_turn_index(Turns const& turns, Turn const& turn, Operation const& op)
    {
        std::ostringstream out;

        if (turn.cluster_id >= 0)
        {
            out << "cl=" << turn.cluster_id << " ";
        }

        // Because turn index is unknown here, and still useful for debugging,
        std::size_t index = 0;
        for (typename Turns::const_iterator it = turns.begin();
           it != turns.end(); ++it, ++index)
        {
          Turn const& t = *it;
          if (&t == &turn)
          {
              out << index;
              break;
          }
        }

        if (&op == &turn.operations[0]) { out << "[0]"; }
        if (&op == &turn.operations[1]) { out << "[1]"; }
        return out.str();
    }

    template <typename Clusters, typename Turns>
    void visit_clusters(Clusters const& clusters, Turns const& turns)
    {
        typedef typename boost::range_value<Turns>::type turn_type;
        int index = 0;
        BOOST_FOREACH(turn_type const& turn, turns)
        {
            if (turn.cluster_id >= 0)
            {
                std::cout << " TURN: " << index << "  part of cluster "  << turn.cluster_id << std::endl;
            }
            index++;
        }

        for (typename Clusters::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
        {
            std::cout << " CLUSTER " << it->first << ": ";
            for (typename std::set<bg::signed_size_type>::const_iterator sit
                 = it->second.turn_indices.begin();
                 sit != it->second.turn_indices.end(); ++sit)
            {
                std::cout << " "  << *sit;
            }
            std::cout << std::endl;
        }

        std::cout << std::endl;

    }

    template <typename Turns, typename Turn, typename Operation>
    void visit_traverse(Turns const& turns, Turn const& turn, Operation const& op, const std::string& header)
    {
        typedef typename boost::range_value<Turns>::type turn_type;

        if (! m_do_output)
        {
            return;
        }

        std::cout << "Visit turn " << stream_turn_index(turns, turn, op)
                  << " " << bg::operation_char(turn.operations[0].operation)
                    << bg::operation_char(turn.operations[1].operation)
                << " (" << bg::operation_char(op.operation) << ")"
                << " "  << header << std::endl;

        // Uncomment for more detailed debug info in SVG on traversal
        std::string style
                = header == "Visit" ? "fill:rgb(80,80,80)" : "fill:rgb(0,0,0)";

        style += ";font-family:Arial;font-size:6px";

        stream(turns, turn, op, header.substr(0, 1), style);
    }

    template <typename Turns, typename Turn, typename Operation>
    void visit_traverse_reject(Turns const& turns, Turn const& turn, Operation const& op,
                               bg::detail::overlay::traverse_error_type error)
    {
        if (! m_do_output)
        {
            return;
        }
        std::cout << "Reject turn " << stream_turn_index(turns, turn, op)
                  << bg::operation_char(turn.operations[0].operation)
                    << bg::operation_char(turn.operations[1].operation)
                << " (" << bg::operation_char(op.operation) << ")"
                << " "  << bg::detail::overlay::traverse_error_string(error) << std::endl;
        //return;

        std::string style =  "fill:rgb(255,0,0);font-family:Arial;font-size:7px";
        stream(turns, turn, op, bg::detail::overlay::traverse_error_string(error), style);

        m_do_output = false;
    }

    template <typename Turns, typename Turn, typename Operation>
    void visit_traverse_select_turn_from_cluster(Turns const& turns, Turn const& turn, Operation const& op)
    {
        std::cout << "Visit turn from cluster " << stream_turn_index(turns, turn, op)
                  << " " << bg::operation_char(turn.operations[0].operation)
                    << bg::operation_char(turn.operations[1].operation)
                << " (" << bg::operation_char(op.operation) << ")"
                << std::endl;
        return;
    }

    template <typename Turns, typename Turn, typename Operation>
    void stream(Turns const& turns, Turn const& turn, Operation const& op, const std::string& header, const std::string& style)
    {
        std::ostringstream out;
        out << m_traverse_seq++ << " " << header
            << " " << stream_turn_index(turns, turn, op);

        out << " " << bg::visited_char(op.visited);

        add_text(turn, out.str(), style);
    }

    template <typename Turn>
    bool label_operation(Turn const& turn, int index, std::ostream& os)
    {
        os << bg::operation_char(turn.operations[index].operation);
        bool result = false;
        if (! turn.discarded)
        {
            if (turn.operations[index].enriched.next_ip_index != -1)
            {
                os << "->" << turn.operations[index].enriched.next_ip_index;
                if (turn.operations[index].enriched.next_ip_index != -1)
                {
                    result = true;
                }
            }
            else
            {
                os << "->"  << turn.operations[index].enriched.travels_to_ip_index;
                if (turn.operations[index].enriched.travels_to_ip_index != -1)
                {
                    result = true;
                }
            }

            os << " {" << turn.operations[index].enriched.region_id
               << (turn.operations[index].enriched.isolated ? " ISO" : "")
               << "}";

            if (! turn.operations[index].enriched.startable)
            {
                os << "$";
            }
        }

        return result;
    }

    template <typename Turn>
    void label_turn(int index, Turn const& turn)
    {
        std::ostringstream out;
        out << index << " ";
        if (turn.cluster_id != -1)
        {
            out << " c=" << turn.cluster_id << " ";
        }
        bool lab1 = label_operation(turn, 0, out);
        out << " / ";
        bool lab2 = label_operation(turn, 1, out);
        if (turn.discarded)
        {
            out << "!";
        }
        if (turn.has_colocated_both)
        {
            out << "+";
        }
        bool const self_turn = bg::detail::overlay::is_self_turn<bg::overlay_union>(turn);
        if (self_turn)
        {
            out << "@";
        }

        std::string font8 = "font-family:Arial;font-size:6px";
        std::string font6 = "font-family:Arial;font-size:4px";
        std::string style =  "fill:rgb(0,0,255);" + font8;
        if (self_turn)
        {
            if (turn.discarded)
            {
                style =  "fill:rgb(128,28,128);" + font6;
            }
            else
            {
                style =  "fill:rgb(255,0,255);" + font8;
            }
        }
        else if (turn.discarded)
        {
            style =  "fill:rgb(92,92,92);" + font6;
        }
        else if (turn.cluster_id != -1)
        {
            style =  "fill:rgb(0,0,255);" + font8;
        }
        else if (! lab1 || ! lab2)
        {
            style =  "fill:rgb(0,0,255);" + font6;
        }

        add_text(turn, out.str(), style);
    }

    template <typename Turn>
    void add_text(Turn const& turn, std::string const& text, std::string const& style)
    {
        int const margin = 5;
        int const lineheight = 6;
        double const half = 0.5;
        double const ten = 10;

        // Map characteristics
        // Create a rounded off point
        std::pair<int, int> p
            = std::make_pair(
                boost::numeric_cast<int>(half
                    + ten * bg::get<0>(turn.point)),
                boost::numeric_cast<int>(half
                    + ten * bg::get<1>(turn.point))
                );
        m_mapper.text(turn.point, text, style, margin, m_offsets[p], lineheight);
        m_offsets[p] += lineheight;
    }


    Mapper& m_mapper;
    std::map<std::pair<int, int>, int> m_offsets;
    int m_traverse_seq;
    bool m_do_output;

};
#endif

template <typename Geometry, bg::overlay_type OverlayType>
void test_overlay(std::string const& caseid,
        std::string const& wkt1, std::string const& wkt2,
        double expected_area,
        std::size_t expected_clip_count,
        std::size_t expected_hole_count)
{
    Geometry g1;
    bg::read_wkt(wkt1, g1);

    Geometry g2;
    bg::read_wkt(wkt2, g2);

    // Reverse if necessary
    bg::correct(g1);
    bg::correct(g2);

#if defined(TEST_WITH_SVG)
    bool const ccw = bg::point_order<Geometry>::value == bg::counterclockwise;
    bool const open = bg::closure<Geometry>::value == bg::open;

    std::ostringstream filename;
    filename << "overlay"
        << "_" << caseid
        << "_" << string_from_type<typename bg::coordinate_type<Geometry>::type>::name()
        << (ccw ? "_ccw" : "")
        << (open ? "_open" : "")
#if defined(BOOST_GEOMETRY_USE_RESCALING)
        << "_rescaled"
#endif
        << ".svg";

    std::ofstream svg(filename.str().c_str());

    typedef bg::svg_mapper<typename bg::point_type<Geometry>::type> svg_mapper;

    svg_mapper mapper(svg, 500, 500);
    mapper.add(g1);
    mapper.add(g2);

    // Input shapes in green (src=0) / blue (src=1)
    mapper.map(g1, "fill-opacity:0.5;fill:rgb(153,204,0);"
            "stroke:rgb(153,204,0);stroke-width:3");
    mapper.map(g2, "fill-opacity:0.3;fill:rgb(51,51,153);"
            "stroke:rgb(51,51,153);stroke-width:3");
#endif


    typedef typename boost::range_value<Geometry>::type geometry_out;
    typedef bg::detail::overlay::overlay
        <
            Geometry, Geometry,
            bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value,
            OverlayType == bg::overlay_difference
            ? ! bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value
            : bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value,
            bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value,
            geometry_out,
            OverlayType
        > overlay;

    typedef typename bg::strategies::relate::services::default_strategy
        <
            Geometry, Geometry
        >::type strategy_type;

    strategy_type strategy;

    typedef typename bg::rescale_overlay_policy_type
    <
        Geometry,
        Geometry
    >::type rescale_policy_type;

    rescale_policy_type robust_policy
        = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

#if defined(TEST_WITH_SVG)
    map_visitor<svg_mapper> visitor(mapper);
#else
    bg::detail::overlay::overlay_null_visitor visitor;
#endif

    Geometry result;
    overlay::apply(g1, g2, robust_policy, std::back_inserter(result),
                   strategy, visitor);

    std::string message;
    bool const valid = check_validity<Geometry>::apply(result, caseid, g1, g2, message);
    BOOST_CHECK_MESSAGE(valid,
        "overlay: " << caseid << " not valid: " << message
        << " type: " << (type_for_assert_message<Geometry, Geometry>()));

    BOOST_CHECK_CLOSE(bg::area(result), expected_area, 0.001);
    BOOST_CHECK_MESSAGE((bg::num_interior_rings(result) == expected_hole_count),
                        caseid
                        << " hole count: detected: " << bg::num_interior_rings(result)
                        << " expected: "  << expected_hole_count);
    BOOST_CHECK_MESSAGE((result.size() == expected_clip_count),
                        caseid
                        << " clip count: detected: " << result.size()
                        << " expected: "  << expected_clip_count);

#if defined(TEST_WITH_SVG)
    mapper.map(result, "fill-opacity:0.2;stroke-opacity:0.4;fill:rgb(255,0,0);"
                        "stroke:rgb(255,0,255);stroke-width:8");

#endif
}

#define TEST_INTERSECTION(caseid, area, clips, holes) (test_overlay<multi_polygon, bg::overlay_intersection>) \
    ( #caseid "_int", caseid[0], caseid[1], area, clips, holes)
#define TEST_UNION(caseid, area, clips, holes) (test_overlay<multi_polygon, bg::overlay_union>) \
    ( #caseid "_union", caseid[0], caseid[1], area, clips, holes)
#define TEST_DIFFERENCE_A(caseid, area, clips, holes) (test_overlay<multi_polygon, bg::overlay_difference>) \
    ( #caseid "_diff_a", caseid[0], caseid[1], area, clips, holes)
#define TEST_DIFFERENCE_B(caseid, area, clips, holes) (test_overlay<multi_polygon, bg::overlay_difference>) \
    ( #caseid "_diff_b", caseid[1], caseid[0], area, clips, holes)

#define TEST_INTERSECTION_WITH(caseid, index1, index2, area, clips, holes) (test_overlay<multi_polygon, bg::overlay_intersection>) \
    ( #caseid "_int_" #index1 "_" #index2, caseid[index1], caseid[index2], area, clips, holes)
#define TEST_UNION_WITH(caseid, index1, index2, area, clips, holes) (test_overlay<multi_polygon, bg::overlay_union>) \
    ( #caseid "_union" #index1 "_" #index2, caseid[index1], caseid[index2], area, clips, holes)

template <typename T, bool Clockwise>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    typedef bg::model::polygon<point_type, Clockwise> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    TEST_UNION(case_multi_simplex, 14.58, 1, 0);
    TEST_INTERSECTION(case_multi_simplex, 6.42, 2, 0);

    TEST_DIFFERENCE_A(case_multi_simplex, 5.58, 5, 0);
    TEST_DIFFERENCE_B(case_multi_simplex, 2.58, 4, 0);

    // Contains 5 clusters, needing immediate selection of next turn
    TEST_UNION_WITH(case_58_multi, 0, 3, 19.8333333, 2, 0);

    // Contains many clusters, needing to exclude u/u turns
    TEST_UNION(case_recursive_boxes_3, 56.5, 17, 6);

    // Contains 4 clusters, one of which having 4 turns
    TEST_UNION(case_recursive_boxes_7, 7.0, 2, 0);

    // Contains 5 clusters, needing immediate selection of next turn
    TEST_UNION(case_89_multi, 6.0, 1, 0);

    // Needs ux/next_turn_index==-1 to be filtered out
    TEST_INTERSECTION(case_77_multi, 9.0, 5, 0);
    TEST_UNION(case_101_multi, 22.25, 1, 3);
    TEST_INTERSECTION(case_101_multi, 4.75, 4, 0);

    TEST_INTERSECTION(case_recursive_boxes_11, 1.0, 2, 0);
    TEST_INTERSECTION(case_recursive_boxes_30, 6.0, 4, 0);

    TEST_UNION(case_recursive_boxes_4, 96.75, 1, 2);
    TEST_INTERSECTION_WITH(case_58_multi, 2, 6, 13.25, 1, 1);
    TEST_INTERSECTION_WITH(case_72_multi, 1, 2, 6.15, 3, 1);
    TEST_UNION(case_recursive_boxes_12, 6.0, 6, 0);
    TEST_UNION(case_recursive_boxes_13, 10.25, 3, 0);


//    std::cout
//        << "    \""
//        << bg::inverse<multi_polygon>(case_65_multi[0], 1.0)
//        << "\"" << std::endl;
}

int test_main(int, char* [])
{
    test_all<double, true>();
//    test_all<double, false>();
    return 0;
 }
