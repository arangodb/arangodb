// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[overlaps
//` Checks if two geometries overlap

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Checks if the two geometries overlaps or not. 
    bg::model::polygon<bg::model::d2::point_xy<double> > poly1;
    bg::read_wkt("POLYGON((0 0,0 4,4 4,4 0,0 0))", poly1);
    bg::model::polygon<bg::model::d2::point_xy<double> > poly2;
    bg::read_wkt("POLYGON((2 2,2 6,6 7,6 1,2 2))", poly2);
    bool check_overlap = bg::overlaps(poly1, poly2);
    if (check_overlap) {
         std::cout << "Overlaps: Yes" << std::endl;
    } else {
        std::cout << "Overlaps: No" << std::endl;
    }

    bg::model::polygon<bg::model::d2::point_xy<double> > poly3;
    bg::read_wkt("POLYGON((-1 -1,-3 -4,-7 -7,-4 -3,-1 -1))", poly3);
    check_overlap = bg::overlaps(poly1, poly3);
    if (check_overlap) {
         std::cout << "Overlaps: Yes" << std::endl;
    } else {
        std::cout << "Overlaps: No" << std::endl;
    }

    return 0;
}

//]


//[overlaps_output
/*`
Output:
[pre
Overlaps: Yes

[$img/algorithms/overlaps.png]

Overlaps: No
]
*/
//]
