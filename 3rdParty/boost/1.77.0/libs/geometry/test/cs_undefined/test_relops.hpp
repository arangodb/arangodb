// Boost.Geometry

// Copyright (c) 2019-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_CS_UNDEFINED_TEST_RELOPS_HPP
#define BOOST_GEOMETRY_TEST_CS_UNDEFINED_TEST_RELOPS_HPP

#include "common.hpp"

#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/crosses.hpp>
#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/algorithms/overlaps.hpp>
#include <boost/geometry/algorithms/relate.hpp>
#include <boost/geometry/algorithms/relation.hpp>
#include <boost/geometry/algorithms/touches.hpp>
#include <boost/geometry/algorithms/within.hpp>

template
<
    typename G1,
    typename G2,
    std::size_t Dim1 = bg::topological_dimension<G1>::value,
    std::size_t Dim2 = bg::topological_dimension<G2>::value
>
struct call_equals
{
    template <typename S>
    static void apply(G1 const& , G2 const& , S const& ) {}
};

template <typename G1, typename G2, std::size_t Dim>
struct call_equals<G1, G2, Dim, Dim>
{
    template <typename S>
    static void apply(G1 const& g1, G2 const& g2, S const& s)
    {
        bg::equals(g1, g2, s);
    }
};

template
<
    typename G1,
    typename G2,
    std::size_t Dim1 = bg::topological_dimension<G1>::value,
    std::size_t Dim2 = bg::topological_dimension<G2>::value
>
struct call_overlaps
{
    template <typename S>
    static void apply(G1 const& , G2 const& , S const& ) {}
};

template <typename G1, typename G2, std::size_t Dim>
struct call_overlaps<G1, G2, Dim, Dim>
{
    template <typename S>
    static void apply(G1 const& g1, G2 const& g2, S const& s)
    {
        bg::overlaps(g1, g2, s);
    }
};

template
<
    typename G1,
    typename G2,
    std::size_t Dim1 = bg::topological_dimension<G1>::value,
    std::size_t Dim2 = bg::topological_dimension<G2>::value
>
struct call_touches
{
    template <typename S>
    static void apply(G1 const& g1, G2 const& g2, S const& s)
    {
        bg::touches(g1, g2, s);
    }
};

template <typename G1, typename G2>
struct call_touches<G1, G2, 0, 0>
{
    template <typename S>
    static void apply(G1 const& , G2 const& , S const& ) {}
};

template
<
    typename G1,
    typename G2,
    std::size_t Dim1 = bg::topological_dimension<G1>::value,
    std::size_t Dim2 = bg::topological_dimension<G2>::value
>
struct call_crosses
{
    template <typename S>
    static void apply(G1 const& g1, G2 const& g2, S const& s)
    {
        bg::crosses(g1, g2, s);
    }
};

template <typename G1, typename G2>
struct call_crosses<G1, G2, 0, 0>
{
    template <typename S>
    static void apply(G1 const& , G2 const& , S const& ) {}
};

template <typename G1, typename G2>
struct call_crosses<G1, G2, 2, 2>
{
    template <typename S>
    static void apply(G1 const& , G2 const& , S const& ) {}
};

template <typename G1, typename G2, typename S>
inline void rel(G1 const& g1, G2 const& g2, S const& s)
{
    bg::relation(g1, g2, s);
    bg::relate(g1, g2, bg::de9im::mask("*********"), s);
    bg::covered_by(g1, g2, s);
    call_crosses<G1, G2>::apply(g1, g2, s);
    bg::disjoint(g1, g2, s);
    call_equals<G1, G2>::apply(g1, g2, s);
    bg::intersects(g1, g2, s);    
    call_overlaps<G1, G2>::apply(g1, g2, s);
    call_touches<G1, G2>::apply(g1, g2, s);
    bg::within(g1, g2, s);
}

template <typename G1, typename G2>
inline void rel_pp(G1 const& g1, G2 const& g2)
{
    ::rel(g1, g2, bg::strategy::within::cartesian_point_point());
    ::rel(g1, g2, bg::strategy::within::spherical_point_point());
}

template <typename G1, typename G2>
inline void rel_ps(G1 const& g1, G2 const& g2)
{
    typedef typename bg::point_type<G1>::type point;
    ::rel(g1, g2, bg::strategy::within::cartesian_winding<point>());
    ::rel(g1, g2, bg::strategy::within::spherical_winding<point>());
    ::rel(g1, g2, bg::strategy::within::geographic_winding<point>());
}

template <typename G1, typename G2>
inline void rel_ss(G1 const& g1, G2 const& g2)
{
    ::rel(g1, g2, bg::strategy::intersection::cartesian_segments<>());
    ::rel(g1, g2, bg::strategy::intersection::spherical_segments<>());
    ::rel(g1, g2, bg::strategy::intersection::geographic_segments<>());
}

#endif // BOOST_GEOMETRY_TEST_CS_UNDEFINED_TEST_RELOPS_HPP
