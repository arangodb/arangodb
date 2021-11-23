// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

#include <boost/geometry/algorithms/is_simple.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

template <typename G, typename S>
inline void is(G const& g, S const& s)
{
    bg::is_simple(g, s);
    bg::is_valid(g, s);
}

template <typename G>
inline void is_x(G const& g)
{
    ::is(g, bg::strategy::intersection::cartesian_segments<>());
    ::is(g, bg::strategy::intersection::spherical_segments<>());
    ::is(g, bg::strategy::intersection::geographic_segments<>());
}

int test_main(int, char*[])
{
    geom g;

    ::is_x(g.pt);
    ::is_x(g.mpt);
    ::is_x(g.s);
    ::is_x(g.ls);
    ::is_x(g.mls);
    ::is_x(g.r);
    ::is_x(g.po);
    ::is_x(g.mpo);

    return 0;
}
