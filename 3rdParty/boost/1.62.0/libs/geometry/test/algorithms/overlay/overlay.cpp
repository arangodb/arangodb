// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

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
                case 2 :
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
        }
        if (! turn.operations[index].enriched.startable)
        {
            os << "$";
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
        if (turn.switch_source)
        {
            out << "#";
        }

        std::string style =  "fill:rgb(0,0,0);font-family:Arial;font-size:8px";
        if (turn.colocated)
        {
            style =  "fill:rgb(255,0,0);font-family:Arial;font-size:8px";
        }
        else if (turn.discarded)
        {
            style =  "fill:rgb(92,92,92);font-family:Arial;font-size:6px";
        }
        else if (turn.cluster_id != -1)
        {
            style =  "fill:rgb(0,0,255);font-family:Arial;font-size:8px";
        }
        else if (! lab1 || ! lab2)
        {
            style =  "fill:rgb(0,0,255);font-family:Arial;font-size:6px";
        }

        add_text(turn, out.str(), style);
    }

    template <typename Turn>
    void add_text(Turn const& turn, std::string const& text, std::string const& style)
    {
        int const margin = 5;
        int const lineheight = 8;
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
        double expected_area)
{
    Geometry g1;
    bg::read_wkt(wkt1, g1);

    Geometry g2;
    bg::read_wkt(wkt2, g2);

    // Reverse if necessary
    bg::correct(g1);
    bg::correct(g2);

#if defined(TEST_WITH_SVG)
    std::ostringstream filename;
    filename << "overlay"
        << "_" << caseid
        << "_" << string_from_type<typename bg::coordinate_type<Geometry>::type>::name()
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
            Geometry, Geometry, false, OverlayType == bg::overlay_difference,
            false, geometry_out,
            OverlayType
        > overlay;

    typedef typename bg::rescale_overlay_policy_type
    <
        Geometry,
        Geometry
    >::type rescale_policy_type;

    rescale_policy_type robust_policy
        = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

    typedef bg::intersection_strategies
    <
        typename bg::cs_tag<Geometry>::type,
        Geometry,
        Geometry,
        typename bg::point_type<Geometry>::type,
        rescale_policy_type
    > strategy;

#if defined(TEST_WITH_SVG)
    map_visitor<svg_mapper> visitor(mapper);
#else
    bg::detail::overlay::overlay_null_visitor visitor;
#endif

    Geometry result;
    overlay::apply(g1, g2, robust_policy, std::back_inserter(result),
                   strategy(), visitor);

    BOOST_CHECK_CLOSE(bg::area(result), expected_area, 0.001);

#if defined(TEST_WITH_SVG)
    mapper.map(result, "fill-opacity:0.2;stroke-opacity:0.4;fill:rgb(255,0,0);"
                        "stroke:rgb(255,0,255);stroke-width:8");

#endif
}

template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    typedef bg::model::polygon<point_type> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_multi_simplex_union",
            case_multi_simplex[0], case_multi_simplex[1],
            14.58
        );
    test_overlay<multi_polygon, bg::overlay_intersection>
        (
            "case_multi_simplex_intersection",
            case_multi_simplex[0], case_multi_simplex[1],
            6.42
        );
    test_overlay<multi_polygon, bg::overlay_difference>
        (
            "case_multi_simplex_diff_a",
            case_multi_simplex[0], case_multi_simplex[1],
            5.58
        );
    test_overlay<multi_polygon, bg::overlay_difference>
        (
            "case_multi_simplex_diff_b",
            case_multi_simplex[1], case_multi_simplex[0],
            2.58
        );

    // Contains 5 clusters, needing immediate selection of next turn
    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_58_multi_0_3_union",
            case_58_multi[0], case_58_multi[3],
            19.8333333
        );

    // Contains many clusters, needing to exclude u/u turns
    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_recursive_boxes_3_union",
            case_recursive_boxes_3[0], case_recursive_boxes_3[1],
            56.5
        );
    // Contains 4 clusters, one of which having 4 turns
    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_recursive_boxes_7_union",
            case_recursive_boxes_7[0], case_recursive_boxes_7[1],
            7.0
        );

    // Contains 5 clusters, needing immediate selection of next turn
    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_89_multi_union",
            case_89_multi[0], case_89_multi[1],
            6.0
        );

    // Needs ux/next_turn_index==-1 to be filtered out
    test_overlay<multi_polygon, bg::overlay_intersection>
        (
            "case_77_multi_intersection",
            case_77_multi[0], case_77_multi[1],
            9.0
        );

    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_101_multi_union",
            case_101_multi[0], case_101_multi[1],
            22.25
        );
    test_overlay<multi_polygon, bg::overlay_intersection>
        (
            "case_101_multi_intersection",
            case_101_multi[0], case_101_multi[1],
            4.75
        );

    test_overlay<multi_polygon, bg::overlay_intersection>
        (
            "case_recursive_boxes_11_intersection",
            case_recursive_boxes_11[0], case_recursive_boxes_11[1],
            1.0
        );

    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_recursive_boxes_4_union",
            case_recursive_boxes_4[0], case_recursive_boxes_4[1],
            96.75
        );

    test_overlay<multi_polygon, bg::overlay_intersection>
        (
            "case_58_multi_b6_intersection",
            case_58_multi[6], case_58_multi[2],
            13.25
        );
    test_overlay<multi_polygon, bg::overlay_intersection>
        (
            "case_72_multi_intersection_inv_b",
            case_72_multi[2], case_72_multi[1],
            6.15
        );
    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_recursive_boxes_12_union",
            case_recursive_boxes_12[0], case_recursive_boxes_12[1],
            6.0
        );
    test_overlay<multi_polygon, bg::overlay_union>
        (
            "case_recursive_boxes_13_union",
            case_recursive_boxes_13[0], case_recursive_boxes_13[1],
            10.25
        );


//    std::cout
//        << "    \""
//        << bg::inverse<multi_polygon>(case_65_multi[0], 1.0)
//        << "\"" << std::endl;
}

int test_main(int, char* [])
{
    test_all<double>();
    return 0;
 }
