// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "test_relops.hpp"

int test_main(int, char*[])
{
    geom g;

    // L/L
    //::rel_ss(g.s, g.s); // relate not implemented
    //::rel_ss(g.s, g.ls); // relate not implemented
    //::rel_ss(g.s, g.mls); // relate not implemented
    //::rel_ss(g.ls, g.s); // relate not implemented
    ::rel_ss(g.ls, g.ls);
    ::rel_ss(g.ls, g.mls);
    //::rel_ss(g.mls, g.s); // relate not implemented
    ::rel_ss(g.mls, g.ls);
    ::rel_ss(g.mls, g.mls);
    // L/A
    //::rel_ss(g.s, g.r); // relate not implemented
    ::rel_ss(g.ls, g.r);
    ::rel_ss(g.mls, g.r);
    //::rel_ss(g.s, g.po); // relate not implemented
    ::rel_ss(g.ls, g.po);
    ::rel_ss(g.mls, g.po);
    //::rel_ss(g.s, g.mpo); // relate not implemented
    ::rel_ss(g.ls, g.mpo);
    ::rel_ss(g.mls, g.mpo);
    // A/A
    ::rel_ss(g.r, g.r);
    ::rel_ss(g.po, g.r);
    ::rel_ss(g.mpo, g.r);
    ::rel_ss(g.r, g.po);
    ::rel_ss(g.po, g.po);
    ::rel_ss(g.mpo, g.po);
    ::rel_ss(g.r, g.mpo);
    ::rel_ss(g.po, g.mpo);
    ::rel_ss(g.mpo, g.mpo);

    return 0;
}
