// Boost.Geometry
// QuickBook Example
// Copyright (c) 2018, Oracle and/or its affiliates
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[discrete_frechet_distance_strategy
//` Calculate Similarity between two geometries as the discrete frechet distance between them.

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/linestring.hpp>

int main()
{
    namespace bg = boost::geometry;
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > point_type;
    typedef bg::model::linestring<point_type> linestring_type;

    linestring_type ls1, ls2;
    bg::read_wkt("LINESTRING(0 0,1 1,1 2,2 1,2 2)", ls1);
    bg::read_wkt("LINESTRING(1 0,0 1,1 1,2 1,3 1)", ls2);

    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);
    bg::strategy::distance::geographic<> strategy(spheroid);

    double res = bg::discrete_frechet_distance(ls1, ls2, strategy);

    std::cout << "Discrete Frechet Distance: " << res << std::endl;

    return 0;
}

//]

//[discrete_frechet_distance_strategy_output
/*`
Output:
[pre
Discrete Frechet Distance: 156874
]
*/
//]
