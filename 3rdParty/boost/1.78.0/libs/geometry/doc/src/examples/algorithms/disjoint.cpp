// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[disjoint
//` Checks if two geometries are disjoint

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Checks if two geometries are disjoint, which means that two geometries have zero intersection.
    bg::model::polygon<bg::model::d2::point_xy<double> > poly1;
    bg::read_wkt("POLYGON((0 2,-2 0,-4 2,-2 4,0 2))", poly1);
    bg::model::polygon<bg::model::d2::point_xy<double> > poly2;
    bg::read_wkt("POLYGON((2 2,4 4,6 2,4 0,2 2))", poly2);
    bool check_disjoint = bg::disjoint(poly1, poly2);
    if (check_disjoint) {
         std::cout << "Disjoint: Yes" << std::endl;
    } else {
        std::cout << "Disjoint: No" << std::endl;
    }

    bg::model::polygon<bg::model::d2::point_xy<double> > poly3;
    bg::read_wkt("POLYGON((0 2,2 4,4 2,2 0,0 2))", poly3);
    check_disjoint = bg::disjoint(poly1, poly3);
    if (check_disjoint) {
         std::cout << "Disjoint: Yes" << std::endl;
    } else {
        std::cout << "Disjoint: No" << std::endl;
    }

    return 0;
}

//]


//[disjoint_output
/*`
Output:
[pre
Disjoint: Yes
Disjoint: No

[$img/algorithms/disjoint.png]

]
*/
//]
