// Boost.Geometry
// QuickBook Example

// Copyright (c) 2020 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[svg_mapper_custom
//` Shows the usage of svg_mapper with arrows, groups and a larger margin

#include <iostream>
#include <fstream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

int main()
{
    // Specify the basic type
    using point_type = boost::geometry::model::d2::point_xy<double>;

    // Declare linestrings and set their values
    boost::geometry::model::linestring<point_type> a, b, c;
    a.push_back({1, 0});
    a.push_back({3, 3});

    b.push_back({5, 0});
    b.push_back({3, 2});

    c.push_back({4, 5});
    c.push_back({3, 4});

    // Declare a stream and an SVG mapper
    // The factor of 0.95 zooms out to give a convenient margin
    std::ofstream svg("my_map.svg");
    boost::geometry::svg_mapper<point_type> mapper(svg, 400, 400, 0.95);

    // Write a marker definition.
    svg << "<defs>";
    svg << "<marker id=\"arrowhead\" markerWidth=\"5\" markerHeight=\"3.5\""
           " refX=\"0\" refY=\"1.75\" orient=\"auto\">"
           " <polygon points=\"0 0, 5 1.75, 0 3.5\"/></marker>";
    svg << "</defs>";

    // Add geometries such that all these geometries fit exactly on the map
    mapper.add(a);
    mapper.add(b);
    mapper.add(c);

    // Group the first two geometries
    svg << "<g>";
    mapper.map(a, "opacity:0.5;stroke-width:1;stroke:gray;marker-end:url(#arrowhead)");
    mapper.map(b, "opacity:0.5;stroke-width:3;stroke:gray;marker-end:url(#arrowhead)");
    svg << "</g>";

    mapper.map(c, "opacity:0.5;stroke-width:5;stroke:red;marker-end:url(#arrowhead)");

    return 0;
}

//]


//[svg_mapper_custom_output
/*`
Output:

[$img/io/svg_mapper_custom.png]
*/
//]
