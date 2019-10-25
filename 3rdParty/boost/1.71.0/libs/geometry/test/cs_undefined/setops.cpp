// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include "common.hpp"

#include <boost/geometry/algorithms/difference.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/sym_difference.hpp>
#include <boost/geometry/algorithms/union.hpp>

template <typename G1, typename G2, typename G3, typename S>
inline void set_idsu(G1 const& g1, G2 const& g2, G3 & g3, S const& s)
{
    bg::intersection(g1, g2, g3, s);
    bg::difference(g1, g2, g3, s);
    bg::sym_difference(g1, g2, g3, s);
    bg::union_(g1, g2, g3, s);
}

template <typename G1, typename G2, typename G3, typename S>
inline void set_ids(G1 const& g1, G2 const& g2, G3 & g3, S const& s)
{
    bg::intersection(g1, g2, g3, s);
    bg::difference(g1, g2, g3, s);
    bg::sym_difference(g1, g2, g3, s);
}

template <typename G1, typename G2, typename G3, typename S>
inline void set_id(G1 const& g1, G2 const& g2, G3 & g3, S const& s)
{
    bg::intersection(g1, g2, g3, s);
    bg::difference(g1, g2, g3, s);
}

template <typename G1, typename G2, typename G3, typename S>
inline void set_i(G1 const& g1, G2 const& g2, G3 & g3, S const& s)
{
    bg::intersection(g1, g2, g3, s);
}

template <typename G1, typename G2, typename G3, typename S>
inline void set_d(G1 const& g1, G2 const& g2, G3 & g3, S const& s)
{
    bg::difference(g1, g2, g3, s);
}

template <typename G1, typename G2, typename G3>
inline void set_idsu_pp(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_idsu(g1, g2, g3, bg::strategy::within::cartesian_point_point());
    ::set_idsu(g1, g2, g3, bg::strategy::within::spherical_point_point());
}

template <typename G1, typename G2, typename G3>
inline void set_idsu_ps(G1 const& g1, G2 const& g2, G3 & g3)
{
    typedef typename bg::point_type<G1>::type point_type;
    ::set_idsu(g1, g2, g3, bg::strategy::within::cartesian_winding<point_type>());
    ::set_idsu(g1, g2, g3, bg::strategy::within::spherical_winding<point_type>());
    ::set_idsu(g1, g2, g3, bg::strategy::within::geographic_winding<point_type>());
}

template <typename G1, typename G2, typename G3>
inline void set_idsu_ss(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_idsu(g1, g2, g3, bg::strategy::intersection::cartesian_segments<>());
    ::set_idsu(g1, g2, g3, bg::strategy::intersection::spherical_segments<>());
    ::set_idsu(g1, g2, g3, bg::strategy::intersection::geographic_segments<>());
}

template <typename G1, typename G2, typename G3>
inline void set_ids_pp(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_ids(g1, g2, g3, bg::strategy::within::cartesian_point_point());
    ::set_ids(g1, g2, g3, bg::strategy::within::spherical_point_point());
}

template <typename G1, typename G2, typename G3>
inline void set_ids_ps(G1 const& g1, G2 const& g2, G3 & g3)
{
    typedef typename bg::point_type<G1>::type point_type;
    ::set_ids(g1, g2, g3, bg::strategy::within::cartesian_winding<point_type>());
    ::set_ids(g1, g2, g3, bg::strategy::within::spherical_winding<point_type>());
    ::set_ids(g1, g2, g3, bg::strategy::within::geographic_winding<point_type>());
}

template <typename G1, typename G2, typename G3>
inline void set_ids_ss(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_ids(g1, g2, g3, bg::strategy::intersection::cartesian_segments<>());
    ::set_ids(g1, g2, g3, bg::strategy::intersection::spherical_segments<>());
    ::set_ids(g1, g2, g3, bg::strategy::intersection::geographic_segments<>());
}

template <typename G1, typename G2, typename G3>
inline void set_id_pp(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_id(g1, g2, g3, bg::strategy::within::cartesian_point_point());
    ::set_id(g1, g2, g3, bg::strategy::within::spherical_point_point());
}

template <typename G1, typename G2, typename G3>
inline void set_id_ps(G1 const& g1, G2 const& g2, G3 & g3)
{
    typedef typename bg::point_type<G1>::type point_type;
    ::set_id(g1, g2, g3, bg::strategy::within::cartesian_winding<point_type>());
    ::set_id(g1, g2, g3, bg::strategy::within::spherical_winding<point_type>());
    ::set_id(g1, g2, g3, bg::strategy::within::geographic_winding<point_type>());
}

template <typename G1, typename G2, typename G3>
inline void set_id_ss(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_id(g1, g2, g3, bg::strategy::intersection::cartesian_segments<>());
    ::set_id(g1, g2, g3, bg::strategy::intersection::spherical_segments<>());
    ::set_id(g1, g2, g3, bg::strategy::intersection::geographic_segments<>());
}

template <typename G1, typename G2, typename G3>
inline void set_i_pp(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_i(g1, g2, g3, bg::strategy::within::cartesian_point_point());
    ::set_i(g1, g2, g3, bg::strategy::within::spherical_point_point());
}

template <typename G1, typename G2, typename G3>
inline void set_i_ps(G1 const& g1, G2 const& g2, G3 & g3)
{
    typedef typename bg::point_type<G1>::type point_type;
    ::set_i(g1, g2, g3, bg::strategy::within::cartesian_winding<point_type>());
    ::set_i(g1, g2, g3, bg::strategy::within::spherical_winding<point_type>());
    ::set_i(g1, g2, g3, bg::strategy::within::geographic_winding<point_type>());
}

template <typename G1, typename G2, typename G3>
inline void set_i_ss(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_i(g1, g2, g3, bg::strategy::intersection::cartesian_segments<>());
    ::set_i(g1, g2, g3, bg::strategy::intersection::spherical_segments<>());
    ::set_i(g1, g2, g3, bg::strategy::intersection::geographic_segments<>());
}

template <typename G1, typename G2, typename G3>
inline void set_d_pp(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_d(g1, g2, g3, bg::strategy::within::cartesian_point_point());
    ::set_d(g1, g2, g3, bg::strategy::within::spherical_point_point());
}

template <typename G1, typename G2, typename G3>
inline void set_d_ps(G1 const& g1, G2 const& g2, G3 & g3)
{
    typedef typename bg::point_type<G1>::type point_type;
    ::set_d(g1, g2, g3, bg::strategy::within::cartesian_winding<point_type>());
    ::set_d(g1, g2, g3, bg::strategy::within::spherical_winding<point_type>());
    ::set_d(g1, g2, g3, bg::strategy::within::geographic_winding<point_type>());
}

template <typename G1, typename G2, typename G3>
inline void set_d_ss(G1 const& g1, G2 const& g2, G3 & g3)
{
    ::set_d(g1, g2, g3, bg::strategy::intersection::cartesian_segments<>());
    ::set_d(g1, g2, g3, bg::strategy::intersection::spherical_segments<>());
    ::set_d(g1, g2, g3, bg::strategy::intersection::geographic_segments<>());
}

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
