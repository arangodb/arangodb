// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>

int test_main(int, char*[])
{
    geom g;

    bg::area(g.r, bg::strategy::area::cartesian<>());
    bg::area(g.r, bg::strategy::area::spherical<>());
    bg::area(g.r, bg::strategy::area::geographic<>());
    bg::area(g.po, bg::strategy::area::cartesian<>());
    bg::area(g.po, bg::strategy::area::spherical<>());
    bg::area(g.po, bg::strategy::area::geographic<>());
    bg::area(g.mpo, bg::strategy::area::cartesian<>());
    bg::area(g.mpo, bg::strategy::area::spherical<>());
    bg::area(g.mpo, bg::strategy::area::geographic<>());

    bg::length(g.s, bg::strategy::distance::pythagoras<>());
    bg::length(g.s, bg::strategy::distance::haversine<>());
    bg::length(g.s, bg::strategy::distance::geographic<>());
    bg::length(g.ls, bg::strategy::distance::pythagoras<>());
    bg::length(g.ls, bg::strategy::distance::haversine<>());
    bg::length(g.ls, bg::strategy::distance::geographic<>());
    bg::length(g.mls, bg::strategy::distance::pythagoras<>());
    bg::length(g.mls, bg::strategy::distance::haversine<>());
    bg::length(g.mls, bg::strategy::distance::geographic<>());

    bg::perimeter(g.r, bg::strategy::distance::pythagoras<>());
    bg::perimeter(g.r, bg::strategy::distance::haversine<>());
    bg::perimeter(g.r, bg::strategy::distance::geographic<>());
    bg::perimeter(g.po, bg::strategy::distance::pythagoras<>());
    bg::perimeter(g.po, bg::strategy::distance::haversine<>());
    bg::perimeter(g.po, bg::strategy::distance::geographic<>());
    bg::perimeter(g.mpo, bg::strategy::distance::pythagoras<>());
    bg::perimeter(g.mpo, bg::strategy::distance::haversine<>());
    bg::perimeter(g.mpo, bg::strategy::distance::geographic<>());

    return 0;
}
