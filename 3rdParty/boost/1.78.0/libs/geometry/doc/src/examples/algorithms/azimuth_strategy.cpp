// Boost.Geometry
// QuickBook Example

// Copyright (c) 2021, Oracle and/or its affiliates
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[azimuth_strategy
//` Shows how to calculate azimuth in geographic coordinate system

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>

int main()
{
    namespace bg = boost::geometry;
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > point_type;

    point_type p1(0, 0);
    point_type p2(1, 1);

    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);
    bg::strategies::azimuth::geographic<> strategy(spheroid);

    auto azimuth = boost::geometry::azimuth(p1, p2, strategy);

    std::cout << "azimuth: " << azimuth << std::endl;

    return 0;
}

//]

//[azimuth_strategy_output
/*`
Output:
[pre
azimuth: 0.788674
]
*/
//]
