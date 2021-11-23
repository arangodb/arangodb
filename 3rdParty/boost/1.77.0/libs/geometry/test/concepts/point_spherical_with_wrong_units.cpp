// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <test_geometries/custom_lon_lat_point.hpp>

#include <boost/core/ignore_unused.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>


namespace bg = boost::geometry;


int main()
{
    bg::model::point<double, 2, bg::cs::spherical_equatorial<double> > p;

    boost::ignore_unused(p);

    return 0;
}
