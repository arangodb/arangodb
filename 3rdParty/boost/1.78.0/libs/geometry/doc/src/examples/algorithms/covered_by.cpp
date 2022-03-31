// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[covered_by
//` Checks if the first geometry is inside or on border the second geometry

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Checks if the first geometry is inside or on border the second geometry.
    bg::model::polygon<bg::model::d2::point_xy<double> > poly1;
    bg::read_wkt("POLYGON((0 2,0 3,2 4,1 2,0 2))", poly1);
    bg::model::polygon<bg::model::d2::point_xy<double> > poly2;
    bg::read_wkt("POLYGON((0 4,3 4,2 2,0 1,0 4))", poly2);
    bool check_covered = bg::covered_by(poly1, poly2);
    if (check_covered) {
         std::cout << "Covered: Yes" << std::endl;
    } else {
        std::cout << "Covered: No" << std::endl;
    }

    bg::model::polygon<bg::model::d2::point_xy<double> > poly3;
    bg::read_wkt("POLYGON((-1 -1,-3 -4,-7 -7,-4 -3,-1 -1))", poly3);
    check_covered = bg::covered_by(poly1, poly3);
    if (check_covered) {
         std::cout << "Covered: Yes" << std::endl;
    } else {
        std::cout << "Covered: No" << std::endl;
    }

    // This should return true since both polygons are same, so they are lying on each other.
    check_covered = bg::covered_by(poly1, poly1);
    if (check_covered) {
         std::cout << "Covered: Yes" << std::endl;
    } else {
        std::cout << "Covered: No" << std::endl;
    }

    return 0;
}

//]


//[covered_by_output
/*`
Output:
[pre
Covered: Yes

[$img/algorithms/covered_by.png]

Covered: No
Covered: Yes
]
*/
//]
