// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2015, Oracle and/or its affiliates

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

//[is_valid_failure
//` Checks whether a geometry is valid and, if not valid, checks if it could be fixed by bg::correct; if so bg::correct is called on the geometry

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
/*<-*/ #include "create_svg_one.hpp" /*->*/

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    typedef boost::geometry::model::polygon<point_type> polygon_type;

    polygon_type poly;
    boost::geometry::read_wkt("POLYGON((0 0,0 10,10 10,10 0),(0 0,9 2,9 1,0 0),(0 0,2 9,1 9,0 0))", poly);

    std::cout << "original geometry: " << boost::geometry::dsv(poly) << std::endl;
    boost::geometry::validity_failure_type failure;
    bool valid = boost::geometry::is_valid(poly, failure);

    // if the invalidity is only due to lack of closing points and/or wrongly oriented rings, then bg::correct can fix it
    bool could_be_fixed = (failure == boost::geometry::failure_not_closed
                           || failure == boost::geometry::failure_wrong_orientation);
    std::cout << "is valid? " << (valid ? "yes" : "no") << std::endl;
    if (! valid)
    {
        std::cout << "can boost::geometry::correct remedy invalidity? " << (could_be_fixed ? "possibly yes" : "no") << std::endl;
        if (could_be_fixed)
        {
            boost::geometry::correct(poly);
            std::cout << "after correction: " << (boost::geometry::is_valid(poly) ? "valid" : "still invalid") << std::endl;
            std::cout << "corrected geometry: " << boost::geometry::dsv(poly) << std::endl;
        }
    }
    /*<-*/ create_svg("is_valid_failure_example.svg", poly); /*->*/
    return 0;
}

//]

//[is_valid_failure_output
/*`
Output:
[pre
original geometry: (((0, 0), (0, 10), (10, 10), (10, 0)), ((0, 0), (9, 2), (9, 1), (0, 0)), ((0, 0), (2, 9), (1, 9), (0, 0)))
is valid? no
can boost::geometry::correct remedy invalidity? possibly yes
after correction: valid
corrected geometry: (((0, 0), (0, 10), (10, 10), (10, 0), (0, 0)), ((0, 0), (9, 1), (9, 2), (0, 0)), ((0, 0), (2, 9), (1, 9), (0, 0)))

[$img/algorithms/is_valid_failure_example.png]

]

*/
//]
