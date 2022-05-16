// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[touches_one_geometry
//` Checks if a geometry has at least one touching point (self-tangency)

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Checks if the geometry has self-tangency.
    bg::model::polygon<bg::model::d2::point_xy<double> > poly1;
    bg::read_wkt("POLYGON((0 0,0 3,2 3,2 2,1 2,1 1,2 1,2 2,3 2,3 0,0 0))", poly1);
    bool check_touches = bg::touches(poly1);
    if (check_touches) {
         std::cout << "Touches: Yes" << std::endl;
    } else {
        std::cout << "Touches: No" << std::endl;
    }

    bg::model::polygon<bg::model::d2::point_xy<double> > poly2;
    bg::read_wkt("POLYGON((0 0,0 4,4 4,4 0,2 3,0 0))", poly2);
    check_touches = bg::touches(poly2);
    if (check_touches) {
         std::cout << "Touches: Yes" << std::endl;
    } else {
        std::cout << "Touches: No" << std::endl;
    }

    return 0;
}

//]


//[touches_one_geometry_output
/*`
Output:
[pre
Touches: Yes

[$img/algorithms/touches_one_geometry.png]

Touches: No
]
*/
//]
