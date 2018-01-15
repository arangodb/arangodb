// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[boost_polygon_ring
//`Shows how to use Boost.Polygon polygon_data within Boost.Geometry

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>

int main()
{
    typedef boost::polygon::polygon_data<int> polygon;
    typedef boost::polygon::polygon_traits<polygon>::point_type point;

    point pts[5] = {
        boost::polygon::construct<point>(0, 0),
        boost::polygon::construct<point>(0, 10),
        boost::polygon::construct<point>(10, 10),
        boost::polygon::construct<point>(10, 0),
        boost::polygon::construct<point>(0, 0)
    };

    polygon poly;
    boost::polygon::set_points(poly, pts, pts+5);
    
    std::cout << "Area (using Boost.Geometry): "
        << boost::geometry::area(poly) << std::endl;
    std::cout << "Area (using Boost.Polygon): "
        << boost::polygon::area(poly) << std::endl;

    return 0;
}

//]

//[boost_polygon_ring_output
/*`
Output:
[pre
Area (using Boost.Geometry): 100
Area (using Boost.Polygon): 100
]
*/
//]
