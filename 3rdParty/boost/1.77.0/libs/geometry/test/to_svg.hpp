// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014 Oracle and/or its affiliates.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

#ifndef BOOST_GEOMETRY_TEST_TO_SVG_HPP
#define BOOST_GEOMETRY_TEST_TO_SVG_HPP

#include <fstream>

#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/svg/svg_mapper.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/self_turn_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/traversal_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/enrichment_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/enrich_intersection_points.hpp>
#include <boost/foreach.hpp>

#include <boost/geometry/algorithms/detail/relate/turns.hpp>

template <typename G, typename Turns, typename Mapper>
inline void turns_to_svg(Turns const& turns, Mapper & mapper, bool /*enrich*/ = false)
{
    namespace bg = boost::geometry;

    // turn points in orange, + enrichment/traversal info
    typedef typename bg::coordinate_type<G>::type coordinate_type;
    typedef typename boost::range_value<Turns>::type turn_info;

    // Simple map to avoid two texts at same place (note that can still overlap!)
    std::map<std::pair<int, int>, int> offsets;
    int index = 0;
    int const margin = 5;

    BOOST_FOREACH(turn_info const& turn, turns)
    {
        int lineheight = 10;
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
        std::string style =  "fill:rgb(0,0,0);font-family:Arial;font-size:12px";

        if (turn.discarded)
        {
            style =  "fill:rgb(92,92,92);font-family:Arial;font-size:10px";
            lineheight = 6;
        }

        //if (! turn.discarded && ! turn.blocked() && ! turn.both(bg::detail::overlay::operation_union))
        //if (! turn.discarded)
        {
            std::ostringstream out;
            out << index
                << ": " << bg::method_char(turn.method);

            if ( turn.discarded )
                out << " (discarded)\n";
            else if ( turn.blocked() )
                out << " (blocked)\n";
            else
                out << '\n';

            out << bg::operation_char(turn.operations[0].operation)
                <<": seg: " << turn.operations[0].seg_id.source_index
                << ' ' << turn.operations[0].seg_id.multi_index
                << ' ' << turn.operations[0].seg_id.ring_index
                << ' ' << turn.operations[0].seg_id.segment_index << ", ";
            out << "other: " << turn.operations[1].seg_id.source_index
                << ' ' << turn.operations[1].seg_id.multi_index
                << ' ' << turn.operations[1].seg_id.ring_index
                << ' ' << turn.operations[1].seg_id.segment_index;

            /*if ( enrich )
            {
            out << ", ";
            if (turn.operations[0].enriched.next_ip_index != -1)
            {
            out << "ip: " << turn.operations[0].enriched.next_ip_index;
            }
            else
            {
            out << "vx: " << turn.operations[0].enriched.travels_to_vertex_index
            << " -> ip: "  << turn.operations[0].enriched.travels_to_ip_index;
            }
            }*/

            out << '\n';

            out << bg::operation_char(turn.operations[1].operation)
                << ": seg: " << turn.operations[1].seg_id.source_index
                << ' ' << turn.operations[1].seg_id.multi_index
                << ' ' << turn.operations[1].seg_id.ring_index
                << ' ' << turn.operations[1].seg_id.segment_index << ", ";
            out << "other: " << turn.operations[0].seg_id.source_index
                << ' ' << turn.operations[0].seg_id.multi_index
                << ' ' << turn.operations[0].seg_id.ring_index
                << ' ' << turn.operations[0].seg_id.segment_index;

            /*if ( enrich )
            {
                out << ", ";
                if (turn.operations[1].enriched.next_ip_index != -1)
                {
                    out << "ip: " << turn.operations[1].enriched.next_ip_index;
                }
                else
                {
                    out << "vx: " << turn.operations[1].enriched.travels_to_vertex_index
                        << " -> ip: " << turn.operations[1].enriched.travels_to_ip_index;
                }
            }*/

            //out << std::endl;

            /*out

                << std::setprecision(3)
                << "dist: " << boost::numeric_cast<double>(turn.operations[0].enriched.distance)
                << " / "  << boost::numeric_cast<double>(turn.operations[1].enriched.distance)
                << std::endl
                << "vis: " << bg::visited_char(turn.operations[0].visited)
                << " / "  << bg::visited_char(turn.operations[1].visited);
            */

            /*
                out << index
                    << ": " << bg::operation_char(turn.operations[0].operation)
                    << " " << bg::operation_char(turn.operations[1].operation)
                    << " (" << bg::method_char(turn.method) << ")"
                    << (turn.ignore() ? " (ignore) " : " ")
                    << std::endl

                    << "ip: " << turn.operations[0].enriched.travels_to_ip_index
                    << "/"  << turn.operations[1].enriched.travels_to_ip_index;

                if (turn.operations[0].enriched.next_ip_index != -1
                    || turn.operations[1].enriched.next_ip_index != -1)
                {
                    out << " [" << turn.operations[0].enriched.next_ip_index
                        << "/"  << turn.operations[1].enriched.next_ip_index
                        << "]"
                        ;
                }
                out << std::endl;


                out
                    << "vx:" << turn.operations[0].enriched.travels_to_vertex_index
                    << "/"  << turn.operations[1].enriched.travels_to_vertex_index
                    << std::endl

                    << std::setprecision(3)
                    << "dist: " << turn.operations[0].enriched.distance
                    << " / "  << turn.operations[1].enriched.distance
                    << std::endl
                    */



                offsets[p] += lineheight;
                int offset = offsets[p];
                offsets[p] += lineheight * 3;
                mapper.text(turn.point, out.str(), style, margin, offset, lineheight);
            }
            index++;
        }
    }
}

template <typename G1, typename P>
inline void geom_to_svg(G1 const& g1,
                        boost::geometry::svg_mapper<P> & mapper)
{
    mapper.add(g1);

    mapper.map(g1, "fill-opacity:0.5;fill:rgb(153,204,0);"
        "stroke:rgb(153,204,0);stroke-width:3");
}

template <typename G1, typename G2, typename P>
inline void geom_to_svg(G1 const& g1, G2 const& g2,
                        boost::geometry::svg_mapper<P> & mapper)
{
    mapper.add(g1);
    mapper.add(g2);

    mapper.map(g1, "fill-opacity:0.5;fill:rgb(153,204,0);"
        "stroke:rgb(153,204,0);stroke-width:3");
    mapper.map(g2, "fill-opacity:0.3;fill:rgb(51,51,153);"
        "stroke:rgb(51,51,153);stroke-width:3");
}

template <typename G1>
inline void geom_to_svg(G1 const& g1, std::string const& filename)
{
    namespace bg = boost::geometry;
    typedef typename bg::point_type<G1>::type mapper_point_type;

    std::ofstream svg(filename.c_str(), std::ios::trunc);
    bg::svg_mapper<mapper_point_type> mapper(svg, 500, 500);

    geom_to_svg(g1, mapper);
}

template <typename G1, typename G2>
inline void geom_to_svg(G1 const& g1, G2 const& g2, std::string const& filename)
{
    namespace bg = boost::geometry;
    typedef typename bg::point_type<G1>::type mapper_point_type;

    std::ofstream svg(filename.c_str(), std::ios::trunc);
    bg::svg_mapper<mapper_point_type> mapper(svg, 500, 500);

    geom_to_svg(g1, g2, mapper);
}

template <typename G1>
inline void geom_to_svg(std::string const& wkt1, std::string const& filename)
{
    namespace bg = boost::geometry;

    G1 g1;
    bg::read_wkt(wkt1, g1);
    geom_to_svg(g1, filename);
}

template <typename G1, typename G2>
inline void geom_to_svg(std::string const& wkt1, std::string const& wkt2, std::string const& filename)
{
    namespace bg = boost::geometry;

    G1 g1;
    G2 g2;
    bg::read_wkt(wkt1, g1);
    bg::read_wkt(wkt2, g2);
    geom_to_svg(g1, g2, filename);
}

struct to_svg_assign_policy
{
    static bool const include_no_turn = false;
    static bool const include_degenerate = false;
    static bool const include_opposite = false;
    static bool const include_start_turn = false;
};

template <typename G>
inline void to_svg(G const& g, std::string const& filename, bool /*sort*/ = true)
{
    namespace bg = boost::geometry;

    typedef typename bg::point_type<G>::type P;

    std::ofstream svg(filename.c_str(), std::ios::trunc);

    bg::svg_mapper<P> mapper(svg, 500, 500);

    mapper.add(g);

    mapper.map(g, "fill-opacity:0.5;fill:rgb(153,204,0);"
        "stroke:rgb(153,204,0);stroke-width:3");

    // GET TURNS

    typedef bg::segment_ratio<double> sr;
    typedef bg::detail::overlay::traversal_turn_info<P, sr> turn_info;
    typedef bg::detail::overlay::assign_null_policy AssignPolicy;
    //typedef to_svg_assign_policy AssignPolicy;    

    typedef std::deque<turn_info> Turns;
    typedef bg::detail::self_get_turn_points::no_interrupt_policy InterruptPolicy;

    Turns turns;
    InterruptPolicy interrupt_policy;

    typedef bg::detail::overlay::get_turn_info<AssignPolicy> TurnPolicy;

    bg::detail::self_get_turn_points::get_turns
        <
            false, TurnPolicy
        >::apply(g, bg::detail::no_rescale_policy(), turns, interrupt_policy);

    turns_to_svg<G>(turns, mapper);
}

template <typename G1, typename G2>
inline void to_svg(G1 const& g1, G2 const& g2, std::string const& filename, bool sort = true, bool use_old_turns_policy = false, bool enrich = false)
{
    namespace bg = boost::geometry;

    typedef typename bg::point_type<G1>::type mapper_point_type;

    std::ofstream svg(filename.c_str(), std::ios::trunc);

    bg::svg_mapper<mapper_point_type> mapper(svg, 500, 500);

    mapper.add(g1);
    mapper.add(g2);

    mapper.map(g1, "fill-opacity:0.5;fill:rgb(153,204,0);"
        "stroke:rgb(153,204,0);stroke-width:3");
    mapper.map(g2, "fill-opacity:0.3;fill:rgb(51,51,153);"
        "stroke:rgb(51,51,153);stroke-width:3");

    // GET TURNS
    
    typedef typename bg::detail::relate::turns::get_turns<G1, G2>::turn_info turn_info;
    //typedef bg::detail::overlay::traversal_turn_info<P1> turn_info;
    //typedef bg::detail::overlay::assign_null_policy AssignPolicy;
    typedef to_svg_assign_policy AssignPolicy;

    typedef std::deque<turn_info> Turns;
    typedef bg::detail::get_turns::no_interrupt_policy InterruptPolicy;
    static const bool Reverse1 = bg::detail::overlay::do_reverse<bg::point_order<G1>::value>::value;
    static const bool Reverse2 = bg::detail::overlay::do_reverse<bg::point_order<G2>::value>::value;

    Turns turns;
    InterruptPolicy interrupt_policy;

    if ( use_old_turns_policy )
    {
        boost::geometry::get_turns
            <
                Reverse1, Reverse2, AssignPolicy
            >(g1, g2, bg::detail::no_rescale_policy(), turns, interrupt_policy);
    }
    else
    {
        typedef bg::detail::get_turns::get_turn_info_type
            <
                G1, G2, AssignPolicy
            > TurnPolicy;

        bg::detail::relate::turns::get_turns
            <
                G1, G2, TurnPolicy
            >::apply(turns, g1, g2);
    }

    if ( sort )
    {
        typedef bg::detail::relate::turns::less
            <
                0,
                bg::detail::relate::turns::less_op_xxx_linear
                    <
                        0, bg::detail::relate::turns::op_to_int<>
                    >,
                typename bg::cs_tag<G1>::type
            > less;
        std::sort(boost::begin(turns), boost::end(turns), less());
    }

    /*if ( enrich )
    {
        typedef typename bg::strategy::side::services::default_strategy
            <
                typename bg::cs_tag<G1>::type
            >::type side_strategy_type;

        bg::enrich_intersection_points<bg::detail::overlay::do_reverse<bg::point_order<G1>::value>::value,
                                       bg::detail::overlay::do_reverse<bg::point_order<G2>::value>::value>
                (turns, bg::detail::overlay::operation_union,
                 g1, g1,
                 bg::detail::no_rescale_policy(),
                 side_strategy_type());
    }*/

    turns_to_svg<G1>(turns, mapper, enrich);
}

template <typename G>
inline void to_svg(std::string const& wkt, std::string const& filename)
{
    G g;
    boost::geometry::read_wkt(wkt, g);
    to_svg(g, filename);
}

template <typename G1, typename G2>
inline void to_svg(std::string const& wkt1, std::string const& wkt2, std::string const& filename, bool sort = true, bool reverse_by_geometry_id = false, bool enrich = false)
{
    G1 g1;
    G2 g2;
    boost::geometry::read_wkt(wkt1, g1);
    boost::geometry::read_wkt(wkt2, g2);
    to_svg(g1, g2, filename, sort, reverse_by_geometry_id, enrich);
}

#endif // BOOST_GEOMETRY_TEST_TO_SVG_HPP
