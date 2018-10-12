// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2011-2014 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Code to create SVG for examples

#ifndef CREATE_SVG_ONE_HPP
#define CREATE_SVG_ONE_HPP

#include <fstream>
#include <boost/algorithm/string.hpp>

#if defined(HAVE_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

template <typename Geometry>
void create_svg(std::string const& filename, Geometry const& g)
{
#if defined(HAVE_SVG)
    std::cout  << std::endl << "[$img/algorithms/" << boost::replace_all_copy(filename, ".svg", ".png") << "]" << std::endl << std::endl;

    typedef typename boost::geometry::point_type<Geometry>::type point_type;
    std::ofstream svg(filename.c_str());

    boost::geometry::svg_mapper<point_type> mapper(svg, 400, 400);
    mapper.add(g);

    mapper.map(g, "fill-opacity:0.3;fill:rgb(51,51,153);stroke:rgb(51,51,153);stroke-width:2");
#else
    boost::ignore_unused(filename, g);
#endif
}

// NOTE: convert manually from svg to png using Inkscape ctrl-shift-E
// and copy png to html/img/algorithms/


#endif // CREATE_SVG_ONE_HPP

