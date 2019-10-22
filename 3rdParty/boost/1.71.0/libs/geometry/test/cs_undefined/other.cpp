// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

#include <boost/geometry/algorithms/centroid.hpp>
#include <boost/geometry/algorithms/simplify.hpp>

template <typename G>
inline void simplify(G const& g1, G & g2)
{
    bg::simplify(g1, g2, 1, bg::strategy::simplify::douglas_peucker<geom::point, bg::strategy::distance::projected_point<> >());
    bg::simplify(g1, g2, 1, bg::strategy::simplify::douglas_peucker<geom::point, bg::strategy::distance::cross_track<> >());

    // TODO: douglas_peucker does not define a ctor taking distance strategy
    // which is needed in geographic CS
    bg::simplify(g1, g2, 1, bg::strategy::simplify::douglas_peucker<geom::point, bg::strategy::distance::geographic_cross_track<> >());
}

int test_main(int, char*[])
{
    geom g, o;

    // this compiles but it shouldn't!
    // internally default_strategy is defined as not_applicable_strategy for box
    bg::centroid(g.b, o.pt);

    ::simplify(g.ls, o.ls);
    // TODO:
    //::simplify(g.r, o.r); // area (point order) strategy not propagated
    //::simplify(g.po, o.po); // area (point order) strategy not propagated

    return 0;
}
