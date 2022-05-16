// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2015, Oracle and/or its affiliates

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

//[is_valid_message
//` Checks whether a geometry is valid and, if not valid, prints a message describing the reason

#include <iostream>
#include <string>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
/*<-*/ #include "create_svg_one.hpp" /*->*/

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    typedef boost::geometry::model::polygon<point_type> polygon_type;

    polygon_type poly;
    boost::geometry::read_wkt("POLYGON((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 2,0 0),(0 0,2 9,1 9,0 0),(2 9,9 2,9 9,2 9))", poly);

    std::string message;
    bool valid = boost::geometry::is_valid(poly, message);
    std::cout << "is valid? " << (valid ? "yes" : "no") << std::endl;
    if (! valid)
    {
        std::cout << "why not valid? " << message << std::endl;
    }
    /*<-*/ create_svg("is_valid_example.svg", poly); /*->*/
    return 0;
}

//]

//[is_valid_message_output
/*`
Output:
[pre
is valid? no
why not valid? Geometry has disconnected interior

[$img/algorithms/is_valid_example.png]

]

*/
//]
