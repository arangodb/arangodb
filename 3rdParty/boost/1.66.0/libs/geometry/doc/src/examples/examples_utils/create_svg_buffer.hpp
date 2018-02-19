// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Code to create SVG for buffer examples

#ifndef CREATE_SVG_BUFFER_HPP
#define CREATE_SVG_BUFFER_HPP

#include <fstream>

#if defined(HAVE_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

template <typename Geometry1, typename Geometry2>
void create_svg_buffer(std::string const& filename, Geometry1 const& original, Geometry2 const& buffer)
{
#if defined(HAVE_SVG)
    typedef typename boost::geometry::point_type<Geometry1>::type point_type;
    std::ofstream svg(filename.c_str());

    boost::geometry::svg_mapper<point_type> mapper(svg, 400, 400);
    mapper.add(original);
    mapper.add(buffer);

    // Draw buffer at bottom
    mapper.map(buffer, "fill-opacity:0.6;fill:rgb(255,255,64);stroke:rgb(255,128,0);stroke-width:3");

    // Draw original on top
    mapper.map(original, "fill-opacity:0.6;fill:rgb(51,51,153);stroke:rgb(51,51,153);stroke-width:2");

#else
    boost::ignore_unused_variable_warning(filename);
    boost::ignore_unused_variable_warning(original);
    boost::ignore_unused_variable_warning(buffer);
#endif
}

// NOTE: convert manually from svg to png using Inkscape ctrl-shift-E
// and copy png to html/img/...


#endif // CREATE_SVG_BUFFER_HPP

