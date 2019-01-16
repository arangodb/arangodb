// Boost.Geometry

// Copyright (c) 2018 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_ENVELOPE_AREAL_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_ENVELOPE_AREAL_HPP

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/iterators/segment_iterator.hpp>

#include <boost/geometry/algorithms/detail/envelope/range.hpp>
#include <boost/geometry/algorithms/detail/envelope/linear.hpp>

#include <boost/geometry/algorithms/dispatch/envelope.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace envelope
{

template <typename EnvelopePolicy>
struct envelope_polygon
{
    template <typename Polygon, typename Box, typename Strategy>
    static inline void apply(Polygon const& polygon, Box& mbr, Strategy const& strategy)
    {
        typename ring_return_type<Polygon const>::type ext_ring
            = exterior_ring(polygon);

        if (geometry::is_empty(ext_ring))
        {
            // if the exterior ring is empty, consider the interior rings
            envelope_multi_range
                <
                    EnvelopePolicy
                >::apply(interior_rings(polygon), mbr, strategy);
        }
        else
        {
            // otherwise, consider only the exterior ring
            EnvelopePolicy::apply(ext_ring, mbr, strategy);
        }
    }
};


}} // namespace detail::envelope
#endif // DOXYGEN_NO_DETAIL

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{


template <typename Ring, typename CS_Tag>
struct envelope<Ring, ring_tag, CS_Tag>
    : detail::envelope::envelope_range
{};

template <typename Ring>
struct envelope<Ring, ring_tag, spherical_equatorial_tag>
    : detail::envelope::envelope_linestring_or_ring_on_spheroid
{};

template <typename Ring>
struct envelope<Ring, ring_tag, geographic_tag>
    : detail::envelope::envelope_linestring_or_ring_on_spheroid
{};


template <typename Polygon, typename CS_Tag>
struct envelope<Polygon, polygon_tag, CS_Tag>
    : detail::envelope::envelope_polygon
        <
            detail::envelope::envelope_range
        >
{};

template <typename Polygon>
struct envelope<Polygon, polygon_tag, spherical_equatorial_tag>
    : detail::envelope::envelope_polygon
        <
            detail::envelope::envelope_linestring_or_ring_on_spheroid
        >
{};

template <typename Polygon>
struct envelope<Polygon, polygon_tag, geographic_tag>
    : detail::envelope::envelope_polygon
        <
            detail::envelope::envelope_linestring_or_ring_on_spheroid
        >
{};


template <typename MultiPolygon, typename CS_Tag>
struct envelope<MultiPolygon, multi_polygon_tag, CS_Tag>
    : detail::envelope::envelope_multi_range
        <
            detail::envelope::envelope_polygon
              <
                  detail::envelope::envelope_range
              >
        >
{};

template <typename MultiPolygon>
struct envelope<MultiPolygon, multi_polygon_tag, spherical_equatorial_tag>
    : detail::envelope::envelope_multi_range
        <
            detail::envelope::envelope_polygon
              <
                  detail::envelope::envelope_linestring_or_ring_on_spheroid
              >
        >
{};

template <typename MultiPolygon>
struct envelope<MultiPolygon, multi_polygon_tag, geographic_tag>
    : detail::envelope::envelope_multi_range
        <
            detail::envelope::envelope_polygon
              <
                  detail::envelope::envelope_linestring_or_ring_on_spheroid
              >
        >
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_ENVELOPE_AREAL_HPP
