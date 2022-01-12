// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[touches_two_geometries
//` Checks if two geometries have at least one touching point (tangent - non overlapping)

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Checks if the two geometries touch
    bg::model::polygon<bg::model::d2::point_xy<double> > poly1;
    bg::read_wkt("POLYGON((0 0,0 4,4 4,4 0,0 0))", poly1);
    bg::model::polygon<bg::model::d2::point_xy<double> > poly2;
    bg::read_wkt("POLYGON((0 0,0 -4,-4 -4,-4 0,0 0))", poly2);
    bool check_touches = bg::touches(poly1, poly2);
    if (check_touches) {
         std::cout << "Touches: Yes" << std::endl;
    } else {
        std::cout << "Touches: No" << std::endl;
    }

    bg::model::polygon<bg::model::d2::point_xy<double> > poly3;
    bg::read_wkt("POLYGON((1 1,0 -4,-4 -4,-4 0,1 1))", poly3);
    check_touches = bg::touches(poly1, poly3);
    if (check_touches) {
         std::cout << "Touches: Yes" << std::endl;
    } else {
        std::cout << "Touches: No" << std::endl;
    }

    return 0;
}

//]


//[touches_two_geometries_output
/*`
Output:
[pre
Touches: Yes

[$img/algorithms/touches_two_geometries.png]

Touches: No
]
*/
//]
