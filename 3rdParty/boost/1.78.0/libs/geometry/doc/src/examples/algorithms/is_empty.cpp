// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2015, Oracle and/or its affiliates

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

//[is_empty
//` Check if a geometry is the empty set

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>


int main()
{
    boost::geometry::model::multi_linestring
        <
            boost::geometry::model::linestring
                <
                    boost::geometry::model::d2::point_xy<double>
                >
        > mls;
    boost::geometry::read_wkt("MULTILINESTRING((0 0,0 10,10 0),(1 1,8 1,1 8))", mls);
    std::cout << "Is empty? " << (boost::geometry::is_empty(mls) ? "yes" : "no") << std::endl;
    boost::geometry::clear(mls);
    std::cout << "Is empty (after clearing)? " << (boost::geometry::is_empty(mls) ? "yes" : "no") << std::endl;
    return 0;
}

//]


//[is_empty_output
/*`
Output:
[pre
Is empty? no
Is empty (after clearing)? yes
]
*/
//]
