// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

#include <boost/geometry/algorithms/distance.hpp>

int test_main(int, char*[])
{
    geom g;

    bg::distance(g.pt, g.pt, bg::strategy::distance::pythagoras<>());
    bg::distance(g.pt, g.pt, bg::strategy::distance::haversine<>());
    bg::distance(g.pt, g.pt, bg::strategy::distance::geographic<>());
    bg::distance(g.pt, g.mpt, bg::strategy::distance::pythagoras<>());
    bg::distance(g.pt, g.mpt, bg::strategy::distance::haversine<>());
    bg::distance(g.pt, g.mpt, bg::strategy::distance::geographic<>());
    bg::distance(g.mpt, g.mpt, bg::strategy::distance::pythagoras<>());
    bg::distance(g.mpt, g.mpt, bg::strategy::distance::haversine<>());
    bg::distance(g.mpt, g.mpt, bg::strategy::distance::geographic<>());
    bg::distance(g.pt, g.ls, bg::strategy::distance::projected_point<>());
    bg::distance(g.pt, g.ls, bg::strategy::distance::cross_track<>());
    bg::distance(g.pt, g.ls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.pt, g.mls, bg::strategy::distance::projected_point<>());
    bg::distance(g.pt, g.mls, bg::strategy::distance::cross_track<>());
    bg::distance(g.pt, g.mls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpt, g.ls, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpt, g.ls, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpt, g.ls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpt, g.mls, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpt, g.mls, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpt, g.mls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.ls, g.ls, bg::strategy::distance::projected_point<>());
    bg::distance(g.ls, g.ls, bg::strategy::distance::cross_track<>());
    bg::distance(g.ls, g.ls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.ls, g.mls, bg::strategy::distance::projected_point<>());
    bg::distance(g.ls, g.mls, bg::strategy::distance::cross_track<>());
    bg::distance(g.ls, g.mls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mls, g.mls, bg::strategy::distance::projected_point<>());
    bg::distance(g.mls, g.mls, bg::strategy::distance::cross_track<>());
    bg::distance(g.mls, g.mls, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.pt, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.pt, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.pt, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.pt, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.pt, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.pt, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.pt, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.pt, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.pt, g.mpo, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpt, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpt, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpt, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpt, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpt, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpt, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpt, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpt, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpt, g.mpo, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.ls, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.ls, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.ls, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.ls, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.ls, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.ls, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.ls, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.ls, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.ls, g.mpo, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mls, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.mls, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.mls, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mls, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.mls, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.mls, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mls, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.mls, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.mls, g.mpo, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.r, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.r, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.r, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.r, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.r, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.r, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.r, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.r, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.r, g.mpo, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.po, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.po, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.po, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.po, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.po, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.po, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.po, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.po, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.po, g.mpo, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpo, g.r, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpo, g.r, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpo, g.r, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpo, g.po, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpo, g.po, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpo, g.po, bg::strategy::distance::geographic_cross_track<>());
    bg::distance(g.mpo, g.mpo, bg::strategy::distance::projected_point<>());
    bg::distance(g.mpo, g.mpo, bg::strategy::distance::cross_track<>());
    bg::distance(g.mpo, g.mpo, bg::strategy::distance::geographic_cross_track<>());

    return 0;
}
