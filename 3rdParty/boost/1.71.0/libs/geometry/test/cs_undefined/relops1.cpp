// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "test_relops.hpp"

int test_main(int, char*[])
{
    geom g;

    bg::disjoint(g.pt, g.b, bg::strategy::covered_by::cartesian_point_box());
    bg::disjoint(g.pt, g.b, bg::strategy::covered_by::spherical_point_box());
    bg::disjoint(g.b, g.b, bg::strategy::disjoint::cartesian_box_box());
    bg::disjoint(g.b, g.b, bg::strategy::disjoint::spherical_box_box());
    bg::within(g.pt, g.b, bg::strategy::within::cartesian_point_box());
    bg::within(g.pt, g.b, bg::strategy::within::spherical_point_box());
    bg::within(g.b, g.b, bg::strategy::within::cartesian_box_box());
    bg::within(g.b, g.b, bg::strategy::within::spherical_box_box());
    bg::covered_by(g.pt, g.b, bg::strategy::covered_by::cartesian_point_box());
    bg::covered_by(g.pt, g.b, bg::strategy::covered_by::spherical_point_box());
    bg::covered_by(g.b, g.b, bg::strategy::covered_by::cartesian_box_box());
    bg::covered_by(g.b, g.b, bg::strategy::covered_by::spherical_box_box());

    // P/P
    ::rel_pp(g.pt, g.pt);
    ::rel_pp(g.pt, g.mpt);
    ::rel_pp(g.mpt, g.mpt);

    // P/L
    //::rel_ps(g.pt, g.s); // relate not implemented
    ::rel_ps(g.pt, g.ls);
    ::rel_ps(g.pt, g.mls);
    //::rel_ps(g.mpt, g.s); // relate not implemented
    ::rel_ps(g.mpt, g.ls);
    ::rel_ps(g.mpt, g.mls);
    // P/A
    ::rel_ps(g.pt, g.r);
    ::rel_ps(g.mpt, g.r);
    ::rel_ps(g.pt, g.po);
    ::rel_ps(g.mpt, g.po);
    ::rel_ps(g.pt, g.mpo);
    ::rel_ps(g.mpt, g.mpo);

    return 0;
}
