// Boost.Geometry

// Copyright (c) 2019-2021, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_CS_UNDEFINED_TEST_SETOPS_HPP
#define BOOST_GEOMETRY_TEST_CS_UNDEFINED_TEST_SETOPS_HPP

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

#endif // BOOST_GEOMETRY_TEST_CS_UNDEFINED_TEST_SETOPS_HPP
