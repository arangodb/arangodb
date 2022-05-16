// Boost.Geometry
// QuickBook Example
// Copyright (c) 2018, Oracle and/or its affiliates
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[discrete_hausdorff_distance
//` Calculate Similarity between two geometries as the discrete hasdorff distance between them.

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    typedef boost::geometry::model::linestring<point_type> linestring_type;

    linestring_type ls1, ls2;
    boost::geometry::read_wkt("LINESTRING(0 0,1 1,1 2,2 1,2 2)", ls2);
    boost::geometry::read_wkt("LINESTRING(1 0,0 1,1 1,2 1,3 1)", ls2);

    double res = boost::geometry::discrete_hausdorff_distance(ls1, ls2);

    std::cout << "Discrete Hausdorff Distance: " << res << std::endl;

    return 0;
}

//]

//[discrete_hausdorff_distance_output
/*`
Output:
[pre
Discrete Hausdorff Distance: 1.0
]
*/
//]
