// Boost.Geometry
// QuickBook Example

// Copyright (c) 2018, Oracle and/or its affiliates
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[line_interpolate
//` Shows how to interpolate points on a linestring

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

using namespace boost::geometry;

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    using segment_type = model::segment<point_type>;
    using linestring_type = model::linestring<point_type>;
    using multipoint_type = model::multi_point<point_type>;

    segment_type const s { {0, 0}, {1, 1} };
    linestring_type const l { {0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 2} };
    point_type p;
    multipoint_type mp;

    std::cout << "point interpolation" << std::endl;

    line_interpolate(s, std::sqrt(2)/4, p);
    std::cout << "on segment : " << wkt(p) << std::endl;

    line_interpolate(l, 1.4, p);
    std::cout << "on linestring : " << wkt(p) << std::endl << std::endl;

    std::cout << "multipoint interpolation" << std::endl;

    line_interpolate(s, std::sqrt(2)/4, mp);
    std::cout << "on segment : " << wkt(mp) << std::endl;

    mp=multipoint_type();
    line_interpolate(l, 1.4, mp);
    std::cout << "on linestring : " << wkt(mp) << std::endl;

    return 0;
}

//]

//[line_interpolate_output
/*`
Output:
[pre
point interpolation
on segment : POINT(0.25 0.25)
on linestring : POINT(1 0.4)

multipoint interpolation
on segment : MULTIPOINT((0.25 0.25),(0.5 0.5),(0.75 0.75),(1 1))
on linestring : MULTIPOINT((1 0.4),(0.2 1))
]
*/
//]
