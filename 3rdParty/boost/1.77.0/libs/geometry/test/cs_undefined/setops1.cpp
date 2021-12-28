// Boost.Geometry

// Copyright (c) 2019-2021, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "test_setops.hpp"

int test_main(int, char*[])
{
    geom g;

    // P/P->P
    ::set_idsu_pp(g.pt, g.pt, g.mpt);
    ::set_idsu_pp(g.pt, g.mpt, g.mpt);
    ::set_idsu_pp(g.mpt, g.mpt, g.mpt);
    
    // P/L->P
    ::set_id_ps(g.pt, g.s, g.mpt);
    ::set_id_ps(g.pt, g.ls, g.mpt);
    ::set_id_ps(g.pt, g.mls, g.mpt);
    ::set_id_ps(g.mpt, g.s, g.mpt);
    ::set_id_ps(g.mpt, g.ls, g.mpt);
    ::set_id_ps(g.mpt, g.mls, g.mpt);

    // P/A->P
    // no intersection nor difference
    //::set_id_ps(g.pt, g.r, g.mpt);
    //::set_id_ps(g.pt, g.po, g.mpt);
    //::set_id_ps(g.pt, g.mpo, g.mpt);
    //::set_id_ps(g.mpt, g.r, g.mpt);
    //::set_id_ps(g.mpt, g.po, g.mpt);
    //::set_id_ps(g.mpt, g.mpo, g.mpt);

    // L/L->P
    ::set_ids_ss(g.s, g.s, g.mpt);
    //::set_i_ss(g.s, g.ls, g.mpt); // no intersection nor difference
    //::set_i_ss(g.s, g.mls, g.mpt); // no intersection nor difference
    //::set_i_ss(g.ls, g.s, g.mpt); // no intersection nor difference
    ::set_ids_ss(g.ls, g.ls, g.mpt);
    ::set_i_ss(g.ls, g.mls, g.mpt); // no difference nor sym_difference
    //::set_i_ss(g.mls, g.s, g.mpt); // no intersection nor difference
    ::set_i_ss(g.mls, g.ls, g.mpt); // no difference nor sym_difference
    ::set_ids_ss(g.mls, g.mls, g.mpt);

    // L/L->L
    //::set_ids_ss(g.s, g.s, g.mls); // union not implemented, missing specialization
    //::set_idsu_ss(g.s, g.ls, g.mls); // missing specialization
    //::set_idsu_ss(g.s, g.mls, g.mls); // missing specialization
    //::set_idsu_ss(g.ls, g.s, g.mls); // missing specialization
    ::set_idsu_ss(g.ls, g.ls, g.mls); 
    ::set_idsu_ss(g.ls, g.mls, g.mls);
    //::set_idsu_ss(g.mls, g.s, g.mls); // missing specialization
    ::set_idsu_ss(g.mls, g.ls, g.mls);
    ::set_idsu_ss(g.mls, g.mls, g.mls);

    // S/B->L ?

    // L/B->L ?   

    // L/A->P
    //::set_ids_ss(g.s, g.r, g.mpt); // no intersection
    //::set_ids_ss(g.s, g.po, g.mpt); // no intersection
    //::set_ids_ss(g.s, g.mpo, g.mpt); // no intersection
    ::set_ids_ss(g.ls, g.r, g.mpt);
    ::set_ids_ss(g.ls, g.po, g.mpt);
    ::set_ids_ss(g.ls, g.mpo, g.mpt);
    ::set_ids_ss(g.mls, g.r, g.mpt);
    ::set_ids_ss(g.mls, g.po, g.mpt);
    ::set_ids_ss(g.mls, g.mpo, g.mpt);

    // L/A->L
    //::set_id_ss(g.s, g.r, g.mls); // no intersection
    //::set_id_ss(g.s, g.po, g.mls); // no intersection
    //::set_id_ss(g.s, g.mpo, g.mls); // no intersection
    ::set_id_ss(g.ls, g.r, g.mls);
    ::set_id_ss(g.ls, g.po, g.mls);
    ::set_id_ss(g.ls, g.mpo, g.mls);
    ::set_id_ss(g.mls, g.r, g.mls);
    ::set_id_ss(g.mls, g.po, g.mls);
    ::set_id_ss(g.mls, g.mpo, g.mls);

    return 0;
}
