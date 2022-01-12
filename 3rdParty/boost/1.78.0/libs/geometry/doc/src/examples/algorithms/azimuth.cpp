// Boost.Geometry
// QuickBook Example

// Copyright (c) 2021, Oracle and/or its affiliates
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[azimuth
//` Shows how to calculate azimuth

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;

    point_type p1(0, 0);
    point_type p2(1, 1);

    auto azimuth = boost::geometry::azimuth(p1, p2);

    std::cout << "azimuth: " << azimuth << std::endl;

    return 0;
}

//]

//[azimuth_output
/*`
Output:
[pre
azimuth: 0.785398
]
*/
//]
