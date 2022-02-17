// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[boost_polygon_box
//`Shows how to use Boost.Polygon rectangle_data within Boost.Geometry

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>

int main()
{
    typedef boost::polygon::rectangle_data<int> rect;

    rect b = boost::polygon::construct<rect>(1, 2, 3, 4);

    std::cout << "Area (using Boost.Geometry): "
        << boost::geometry::area(b) << std::endl;
    std::cout << "Area (using Boost.Polygon): "
        << boost::polygon::area(b) << std::endl;

    return 0;
}

//]

//[boost_polygon_box_output
/*`
Output:
[pre
Area (using Boost.Geometry): 4
Area (using Boost.Polygon): 4
]
*/
//]
