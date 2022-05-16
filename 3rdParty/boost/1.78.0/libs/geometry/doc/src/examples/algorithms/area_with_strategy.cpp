// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2018.
// Modifications copyright (c) 2018 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[area_with_strategy
//` Calculate the area of a polygon

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
    // Create spherical polygon
    bg::model::polygon<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > > sph_poly;
    bg::read_wkt("POLYGON((0 0,0 1,1 0,0 0))", sph_poly);

    // Create spherical strategy with mean Earth radius in meters
    bg::strategy::area::spherical<> sph_strategy(6371008.8);

    // Calculate the area of a spherical polygon
    double area = bg::area(sph_poly, sph_strategy);
    std::cout << "Area: " << area << std::endl;

    // Create geographic polygon
    bg::model::polygon<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > > geo_poly;
    bg::read_wkt("POLYGON((0 0,0 1,1 0,0 0))", geo_poly);

    // Create geographic strategy with WGS84 spheroid
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);
    bg::strategy::area::geographic<> geo_strategy(spheroid);

    // Calculate the area of a geographic polygon
    area = bg::area(geo_poly, geo_strategy);
    std::cout << "Area: " << area << std::endl;

    return 0;
}

//]


//[area_with_strategy_output
/*`
Output:
[pre
Area: 6.18249e+09
Area: 6.15479e+09
]
*/
//]
