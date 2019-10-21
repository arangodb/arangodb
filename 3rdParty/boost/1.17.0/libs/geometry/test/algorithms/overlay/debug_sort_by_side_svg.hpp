// Boost.Geometry

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_DEBUG_SORT_BY_SIDE_SVG_HPP
#define BOOST_GEOMETRY_TEST_DEBUG_SORT_BY_SIDE_SVG_HPP

#include <fstream>
#include <sstream>
#include <set>

#include <boost/geometry/io/svg/svg_mapper.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/sort_by_side.hpp>

namespace boost { namespace geometry { namespace debug
{


template <typename Sbs, typename Point, typename Geometry1, typename Geometry2>
inline void sorted_side_map(std::string const& case_id,
    Sbs const& sbs,  Point const& point,
    Geometry1 const& geometry1,
    Geometry2 const& geometry2,
    int take_turn_index = -1, int take_operation_index = -1)
{

    // Check number of sources (buffer has only one source)
    std::set<signed_size_type> sources;
    for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
    {
        const typename Sbs::rp& er = sbs.m_ranked_points[i];
        sources.insert(er.seg_id.source_index);
    }
    std::size_t const source_count = sources.size();

    std::ostringstream filename;
    filename << "sort_by_side_" << case_id << ".svg";
    std::ofstream svg(filename.str().c_str());

    typedef geometry::svg_mapper<Point> mapper_type;
    typedef geometry::model::referring_segment<Point const> seg;

    mapper_type mapper(svg, 500, 500);

    for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
    {
        const typename Sbs::rp& er = sbs.m_ranked_points[i];
        mapper.add(er.point);
    }

    if (sources.count(0) > 0)
    {
        mapper.map(geometry1, "fill-opacity:0.5;fill:rgb(153,204,0);stroke:rgb(153,204,0);stroke-width:0");
    }
    if (sources.count(1) > 0)
    {
        mapper.map(geometry2, "fill-opacity:0.3;fill:rgb(51,51,153);stroke:rgb(51,51,153);stroke-width:0");
    }

    const std::string style = "fill:rgb(0,0,0);font-family:Arial;font-size:10px;";
    for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
    {
        const typename Sbs::rp& er = sbs.m_ranked_points[i];

        std::ostringstream out;
        out << er.rank
            << " (" << i << ")"
            << " z=" << er.zone
            << " " << (er.direction == detail::overlay::sort_by_side::dir_to ? "t" : "f")
            << " " << er.turn_index
            << "[" << er.operation_index << "]";

        if (er.direction == detail::overlay::sort_by_side::dir_to)
        {
            out << " L=" << er.count_left << " R=" << er.count_right;
        }
        else
        {
            out << " l=" << er.count_left << " r=" << er.count_right;
        }
        out << " " << operation_char(er.operation);
        if (source_count > 1)
        {
            out << " s=" << er.seg_id.source_index;
        }

        bool left = (i / 2) % 2 == 1;
        int x_offset = left ? -6 : 6;
        int y_offset = i % 2 == 0 ? 0 : 10;
        const std::string align = left ? "text-anchor:end;" : "";

        std::string const source_style
                = er.seg_id.source_index == 0
                    ? "opacity:0.7;stroke:rgb(0,255,0);stroke-width:4;"
                    : "opacity:0.7;stroke:rgb(0,0,255);stroke-width:4;";
        mapper.map(seg(point, er.point), source_style);

        if (er.direction == detail::overlay::sort_by_side::dir_to)
        {
            if (er.turn_index == take_turn_index
                    && er.operation_index == take_operation_index)
            {
                mapper.map(er.point, "opacity:0.7;fill:rgb(255,0,255);", 3);
            }
            else
            {
                mapper.map(er.point, "opacity:0.7;fill:rgb(0,0,0);", 3);
            }
        }

        mapper.text(er.point, out.str(), style + align, x_offset, y_offset);
    }
    mapper.map(sbs.m_origin, "opacity:0.9;fill:rgb(255,0,0);", 5);
}

}}} // namespace boost::geometry::debug

#endif // BOOST_GEOMETRY_TEST_DEBUG_SORT_BY_SIDE_SVG_HPP
