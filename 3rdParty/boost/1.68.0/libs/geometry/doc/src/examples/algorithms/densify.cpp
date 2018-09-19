// Boost.Geometry
// QuickBook Example

// Copyright (c) 2018, Oracle and/or its affiliates
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[densify
//` Shows how to densify a polygon

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    typedef boost::geometry::model::polygon<point_type> polygon_type;

    polygon_type poly;
    boost::geometry::read_wkt(
        "POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,4 1,4 4,1 4,1 1))", poly);

    polygon_type res;

    boost::geometry::densify(poly, res, 6.0);

    std::cout << "densified: " << boost::geometry::wkt(res) << std::endl;

    return 0;
}

//]

//[densify_output
/*`
Output:
[pre
densified: POLYGON((0 0,0 5,0 10,5 10,10 10,10 5,10 0,5 0,0 0),(1 1,4 1,4 4,1 4,1 1))
]
*/
//]
