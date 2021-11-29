// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[perimeter
//` Calculate the perimeter of a polygon

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Calculate the perimeter of a cartesian polygon
    bg::model::polygon<bg::model::d2::point_xy<double> > poly;
    bg::read_wkt("POLYGON((0 0,3 4,5 -5,-2 -4, 0 0))", poly);
    double perimeter = bg::perimeter(poly);
    std::cout << "Perimeter: " << perimeter << std::endl;

    return 0;
}

//]


//[perimeter_output
/*`
Output:
[pre
Perimeter: 25.7627
]
*/
//]
