// Boost.Geometry

// Copyright (c) 2019-2021, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "test_setops.hpp"

int test_main(int, char*[])
{
    geom g;

    // A/A->P
    ::set_i_ss(g.r, g.r, g.mpt);
    ::set_i_ss(g.r, g.po, g.mpt);
    ::set_i_ss(g.r, g.mpo, g.mpt);
    ::set_i_ss(g.po, g.r, g.mpt);
    ::set_i_ss(g.po, g.po, g.mpt);
    ::set_i_ss(g.po, g.mpo, g.mpt);
    ::set_i_ss(g.mpo, g.r, g.mpt);
    ::set_i_ss(g.mpo, g.po, g.mpt);
    ::set_i_ss(g.mpo, g.mpo, g.mpt);

    // A/A->L
    ::set_i_ss(g.r, g.r, g.mls);
    ::set_i_ss(g.r, g.po, g.mls);
    ::set_i_ss(g.r, g.mpo, g.mls);
    ::set_i_ss(g.po, g.r, g.mls);
    ::set_i_ss(g.po, g.po, g.mls);
    ::set_i_ss(g.po, g.mpo, g.mls);
    ::set_i_ss(g.mpo, g.r, g.mls);
    ::set_i_ss(g.mpo, g.po, g.mls);
    ::set_i_ss(g.mpo, g.mpo, g.mls);

    // A/A->A
    ::set_idsu_ss(g.r, g.r, g.mpo);
    ::set_idsu_ss(g.r, g.po, g.mpo);
    ::set_idsu_ss(g.r, g.mpo, g.mpo);
    ::set_idsu_ss(g.po, g.r, g.mpo);
    ::set_idsu_ss(g.po, g.po, g.mpo);
    ::set_idsu_ss(g.po, g.mpo, g.mpo);
    ::set_idsu_ss(g.mpo, g.r, g.mpo);
    ::set_idsu_ss(g.mpo, g.po, g.mpo);
    ::set_idsu_ss(g.mpo, g.mpo, g.mpo);

    return 0;
}
