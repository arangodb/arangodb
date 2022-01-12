// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[point_xyz
//` Declaration and use of the Boost.Geometry model::d3::point_xyz, modelling the Point Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xyz.hpp>

namespace bg = boost::geometry;

int main()
{
    bg::model::d3::point_xyz<double> point1;
    bg::model::d3::point_xyz<double> point2(3, 4, 5); /*< Construct, assigning coordinates. >*/

    bg::set<0>(point1, 1.0); /*< Set a coordinate, generic. >*/
    point1.y(2.0); /*< Set a coordinate, class-specific ([*Note]: prefer `bg::set()`). >*/
    point1.z(4.0);

    double x = bg::get<0>(point1); /*< Get a coordinate, generic. >*/
    double y = point1.y(); /*< Get a coordinate, class-specific ([*Note]: prefer `bg::get()`). >*/
    double z = point1.z();

    std::cout << x << ", " << y << ", " << z << std::endl;
    return 0;
}

//]


//[point_xyz_output
/*`
Output:
[pre
1, 2, 4
]
*/
//]
