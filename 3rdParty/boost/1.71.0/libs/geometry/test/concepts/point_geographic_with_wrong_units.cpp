// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <test_geometries/custom_lon_lat_point.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>


namespace bg = boost::geometry;


int main()
{
    bg::concepts::check
        <
            bg::model::point<double, 2, bg::cs::geographic<int> >
        >();

    return 0;
}
