// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[crosses
//` Checks if two geometries crosses

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Checks if the two geometries (here, a polygon and a linestring) crosses or not. 
    bg::model::polygon<bg::model::d2::point_xy<double> > poly;
    bg::read_wkt("POLYGON((0 0,0 3,3 3,3 0,0 0))", poly);
    bg::model::linestring<bg::model::d2::point_xy<double> > line1;
    bg::read_wkt("LINESTRING(1 1,2 2,4 4)", line1);
    bool check_crosses = bg::crosses(poly, line1);
    if (check_crosses) {
         std::cout << "Crosses: Yes" << std::endl;
    } else {
        std::cout << "Crosses: No" << std::endl;
    }

    // Edge case: linestring just touches the polygon but doesn't crosses it.
    bg::model::linestring<bg::model::d2::point_xy<double> > line2;
    bg::read_wkt("LINESTRING(1 1,1 2,1 3)", line2);
    check_crosses = bg::crosses(poly, line2);
    if (check_crosses) {
         std::cout << "Crosses: Yes" << std::endl;
    } else {
        std::cout << "Crosses: No" << std::endl;
    }

    return 0;
}

//]


//[crosses_output
/*`
Output:
[pre
Crosses: Yes
Crosses: No

[$img/algorithms/crosses.png]

]
*/
//]
