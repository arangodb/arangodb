// Boost.Geometry
// QuickBook Example

// Copyright (c) 2018, Oracle and/or its affiliates
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[line_interpolate_strategy
//` Shows how to interpolate points on a linestring in geographic coordinate system

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

using namespace boost::geometry;

int main()
{
    typedef model::d2::point_xy<double,cs::geographic<degree> > point_type;
    using segment_type = model::segment<point_type>;
    using linestring_type = model::linestring<point_type>;
    using multipoint_type = model::multi_point<point_type>;

    segment_type const s { {0, 0}, {1, 1} };
    linestring_type const l { {0, 1}, {1, 1}, {1, 2}, {0, 2}, {0, 3} };
    point_type p;
    multipoint_type mp;
    double distance = 50000;

    srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);
    strategy::line_interpolate
            ::geographic<strategy::vincenty> str(spheroid);

    std::cout << "point interpolation" << std::endl;

    line_interpolate(s, distance, p, str);
    std::cout << "on segment : " << wkt(p) << std::endl;

    line_interpolate(l, distance, p, str);
    std::cout << "on linestring : " << wkt(p) << std::endl << std::endl;

    std::cout << "multipoint interpolation" << std::endl;

    line_interpolate(s, distance, mp, str);
    std::cout << "on segment : " << wkt(mp) << std::endl;

    mp=multipoint_type();
    line_interpolate(l,distance, mp, str);
    std::cout << "on linestring : " << wkt(mp) << std::endl;

    return 0;
}

//]

//[line_interpolate_strategy_output
/*`
Output:
[pre
point interpolation
on segment : POINT(0.318646 0.31869)
on linestring : POINT(0.449226 1.00004)

multipoint interpolation
on segment : MULTIPOINT((0.318646 0.31869),(0.637312 0.63737),(0.956017 0.95603))
on linestring : MULTIPOINT((0.449226 1.00004),(0.898451 1.00001),(1 1.34997),
(1 1.80215),(0.74722 2.00006),(0.297791 2.00006),(0 2.15257),(0 2.60474))
]
*/
//]
