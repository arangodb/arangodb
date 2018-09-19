// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2014, Oracle and/or its affiliates

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

//[is_simple
//` Checks whether a geometry is simple

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
/*<-*/ #include "create_svg_one.hpp" /*->*/

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    typedef boost::geometry::model::linestring<point_type> linestring_type;
    typedef boost::geometry::model::multi_linestring<linestring_type> multi_linestring_type;

    multi_linestring_type multi_linestring;
    boost::geometry::read_wkt("MULTILINESTRING((0 0,0 10,10 10,10 0,0 0),(10 10,20 20))", multi_linestring);

    std::cout << "is simple? "
              << (boost::geometry::is_simple(multi_linestring) ? "yes" : "no")
              << std::endl;
    /*<-*/ create_svg("is_simple_example.svg", multi_linestring); /*->*/
    return 0;
}

//]

//[is_simple_output
/*`
Output:
[pre
is simple? no

[$img/algorithms/is_simple_example.png]

]

*/
//]
