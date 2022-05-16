// Boost.Geometry
// QuickBook Example

// Copyright (c) 2018, Oracle and/or its affiliates
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[densify_strategy
//` Shows how to densify a linestring in geographic coordinate system

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/linestring.hpp>

int main()
{
    namespace bg = boost::geometry;
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > point_type;
    typedef bg::model::linestring<point_type> linestring_type;

    linestring_type ls;
    bg::read_wkt("LINESTRING(0 0,1 1)", ls);

    linestring_type res;

    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);
    bg::strategy::densify::geographic<> strategy(spheroid);

    boost::geometry::densify(ls, res, 50000.0, strategy);

    std::cout << "densified: " << boost::geometry::wkt(res) << std::endl;

    return 0;
}

//]

//[densify_strategy_output
/*`
Output:
[pre
densified: LINESTRING(0 0,0.249972 0.250011,0.499954 0.500017,0.749954 0.750013,1 1)
]
*/
//]
