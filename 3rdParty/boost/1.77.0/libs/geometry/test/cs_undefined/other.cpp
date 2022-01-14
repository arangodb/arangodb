// Boost.Geometry

// Copyright (c) 2019-2021, Oracle and/or its affiliates.

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
    bg::simplify(g1, g2, 1, bg::strategy::simplify::douglas_peucker<geom::point, bg::strategy::distance::geographic_cross_track<> >());

    bg::simplify(g1, g2, 1, bg::strategies::simplify::cartesian<>());
    bg::simplify(g1, g2, 1, bg::strategies::simplify::spherical<>());
    bg::simplify(g1, g2, 1, bg::strategies::simplify::geographic<>());
}

template <typename G, typename P>
inline void centroid(G const& g, P & p)
{
    bg::centroid(g, p, bg::strategies::centroid::cartesian<>());
    bg::centroid(g, p, bg::strategies::centroid::spherical<>());
    bg::centroid(g, p, bg::strategies::centroid::geographic<>());
}

int test_main(int, char*[])
{
    geom g, o;

    ::centroid(g.b, o.pt);

    ::simplify(g.pt, o.pt);
    ::simplify(g.ls, o.ls);
    ::simplify(g.mls, o.mls);
    ::simplify(g.r, o.r);
    ::simplify(g.po, o.po);
    ::simplify(g.mpo, o.mpo);

    return 0;
}
