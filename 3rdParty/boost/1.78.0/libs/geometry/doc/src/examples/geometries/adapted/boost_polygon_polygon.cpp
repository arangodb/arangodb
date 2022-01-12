// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[boost_polygon_polygon
//`Shows how to use Boost.Polygon polygon_with_holes_data within Boost.Geometry

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>

int main()
{
    typedef boost::polygon::polygon_with_holes_data<int> polygon;
    typedef boost::polygon::polygon_traits<polygon>::point_type point;
    typedef boost::polygon::polygon_with_holes_traits<polygon>::hole_type hole;

    point pts[5] = {
        boost::polygon::construct<point>(0, 0),
        boost::polygon::construct<point>(0, 10),
        boost::polygon::construct<point>(10, 10),
        boost::polygon::construct<point>(10, 0),
        boost::polygon::construct<point>(0, 0)
    };
    point hole_pts[5] = {
        boost::polygon::construct<point>(1, 1),
        boost::polygon::construct<point>(9, 1),
        boost::polygon::construct<point>(9, 9),
        boost::polygon::construct<point>(1, 9),
        boost::polygon::construct<point>(1, 1)
    };

    hole hls[1];
    boost::polygon::set_points(hls[0], hole_pts, hole_pts+5);

    polygon poly;
    boost::polygon::set_points(poly, pts, pts+5);
    boost::polygon::set_holes(poly, hls, hls+1);
    
    std::cout << "Area (using Boost.Geometry): "
        << boost::geometry::area(poly) << std::endl;
    std::cout << "Area (using Boost.Polygon): "
        << boost::polygon::area(poly) << std::endl;

    return 0;
}

//]

//[boost_polygon_polygon_output
/*`
Output:
[pre
Area (using Boost.Geometry): 36
Area (using Boost.Polygon): 36
]
*/
//]
