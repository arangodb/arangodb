// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017-2021.
// Modifications copyright (c) 2017-2021 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_IS_CONVEX_HPP
#define BOOST_GEOMETRY_ALGORITHMS_IS_CONVEX_HPP


#include <boost/range/empty.hpp>

#include <boost/geometry/algorithms/detail/equals/point_point.hpp>
#include <boost/geometry/algorithms/detail/dummy_geometries.hpp>
#include <boost/geometry/algorithms/detail/visit.hpp>
#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/visit.hpp>
#include <boost/geometry/geometries/adapted/boost_variant.hpp> // For backward compatibility
#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/iterators/ever_circling_iterator.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>
#include <boost/geometry/strategies/is_convex/cartesian.hpp>
#include <boost/geometry/strategies/is_convex/geographic.hpp>
#include <boost/geometry/strategies/is_convex/spherical.hpp>
#include <boost/geometry/views/detail/closed_clockwise_view.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace is_convex
{

struct ring_is_convex
{
    template <typename Ring, typename Strategies>
    static inline bool apply(Ring const& ring, Strategies const& strategies)
    {
        std::size_t n = boost::size(ring);
        if (n < detail::minimum_ring_size<Ring>::value)
        {
            // (Too) small rings are considered as non-concave, is convex
            return true;
        }

        // Walk in clockwise direction, consider ring as closed
        // (though closure is not important in this algorithm - any dupped
        //  point is skipped)
        using view_type = detail::closed_clockwise_view<Ring const>;
        view_type const view(ring);

        using it_type = geometry::ever_circling_range_iterator<view_type const>;
        it_type previous(view);
        it_type current(view);
        current++;

        auto const equals_strategy = strategies.relate(dummy_point(), dummy_point());

        std::size_t index = 1;
        while (equals::equals_point_point(*current, *previous, equals_strategy)
            && index < n)
        {
            current++;
            index++;
        }

        if (index == n)
        {
            // All points are apparently equal
            return true;
        }

        it_type next = current;
        next++;
        while (equals::equals_point_point(*current, *next, equals_strategy))
        {
            next++;
        }

        auto const side_strategy = strategies.side();

        // We have now three different points on the ring
        // Walk through all points, use a counter because of the ever-circling
        // iterator
        for (std::size_t i = 0; i < n; i++)
        {
            int const side = side_strategy.apply(*previous, *current, *next);
            if (side == 1)
            {
                // Next is on the left side of clockwise ring:
                // the piece is not convex
                return false;
            }

            previous = current;
            current = next;

            // Advance next to next different point
            // (because there are non-equal points, this loop is not infinite)
            next++;
            while (equals::equals_point_point(*current, *next, equals_strategy))
            {
                next++;
            }
        }
        return true;
    }
};


struct polygon_is_convex
{
    template <typename Polygon, typename Strategies>
    static inline bool apply(Polygon const& polygon, Strategies const& strategies)
    {
        return boost::empty(interior_rings(polygon))
            && ring_is_convex::apply(exterior_ring(polygon), strategies);
    }
};

struct multi_polygon_is_convex
{
    template <typename MultiPolygon, typename Strategies>
    static inline bool apply(MultiPolygon const& multi_polygon, Strategies const& strategies)
    {
        auto const size = boost::size(multi_polygon);
        return size == 0 // For consistency with ring_is_convex
            || (size == 1 && polygon_is_convex::apply(range::front(multi_polygon), strategies));
    }
};


}} // namespace detail::is_convex
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template
<
    typename Geometry,
    typename Tag = typename tag<Geometry>::type
>
struct is_convex
{
    template <typename Strategies>
    static inline bool apply(Geometry const&, Strategies const&)
    {
        // Convexity is not defined for PointLike and Linear geometries.
        // We could implement this because the following definitions would work:
        // - no line segment between two points on the interior or boundary ever goes outside.
        // - convex_hull of geometry is equal to the original geometry, this implies equal
        //   topological dimension.
        // For MultiPoint we'd have to check whether or not an arbitrary number of equal points
        //   is stored.
        // MultiPolygon we'd have to check for continuous chain of Linestrings which would require
        //   the use of relate(pt, seg) or distance(pt, pt) strategy.
        return false;
    }
};

template <typename Box>
struct is_convex<Box, box_tag>
{
    template <typename Strategies>
    static inline bool apply(Box const& , Strategies const& )
    {
        // Any box is convex (TODO: consider spherical boxes)
        // TODO: in spherical and geographic the answer would be "false" most of the time.
        //   Assuming that:
        //   - it even makes sense to consider Box in spherical and geographic in this context
        //     because it's not a Polygon, e.g. it can degenerate to a Point.
        //   - line segments are defined by geodesics and box edges by parallels and meridians
        //   - we use this definition: A convex polygon is a simple polygon (not self-intersecting)
        //     in which no line segment between two points on the boundary ever goes outside the
        //     polygon.
        //   Then a geodesic segment would go into the exterior of a Box for all horizontal edges
        //   of a Box unless it was one of the poles (edge degenerated to a point) or equator and
        //   longitude difference was lesser than 360 (otherwise depending on the CS there would be
        //   no solution or there would be two possible solutions - segment going through one of
        //   the poles, at least in case of oblate spheroid, either way the answer would probably
        //   be "false").
        return true;
    }
};

template <typename Ring>
struct is_convex<Ring, ring_tag> : detail::is_convex::ring_is_convex
{};

template <typename Polygon>
struct is_convex<Polygon, polygon_tag> : detail::is_convex::polygon_is_convex
{};

template <typename MultiPolygon>
struct is_convex<MultiPolygon, multi_polygon_tag> : detail::is_convex::multi_polygon_is_convex
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH

namespace resolve_strategy {

template
<
    typename Strategies,
    bool IsUmbrella = strategies::detail::is_umbrella_strategy<Strategies>::value
>
struct is_convex
{
    template <typename Geometry>
    static bool apply(Geometry const& geometry, Strategies const& strategies)
    {
        return dispatch::is_convex<Geometry>::apply(geometry, strategies);
    }
};

template <typename Strategy>
struct is_convex<Strategy, false>
{
    template <typename Geometry>
    static bool apply(Geometry const& geometry, Strategy const& strategy)
    {
        using strategies::is_convex::services::strategy_converter;
        return dispatch::is_convex
            <
                Geometry
            >::apply(geometry, strategy_converter<Strategy>::get(strategy));
    }
};

template <>
struct is_convex<default_strategy, false>
{
    template <typename Geometry>
    static bool apply(Geometry const& geometry, default_strategy const& )
    {
        typedef typename strategies::is_convex::services::default_strategy
            <
                Geometry
            >::type strategy_type;

        return dispatch::is_convex<Geometry>::apply(geometry, strategy_type());
    }
};

} // namespace resolve_strategy

namespace resolve_dynamic {

template <typename Geometry, typename Tag = typename tag<Geometry>::type>
struct is_convex
{
    template <typename Strategy>
    static bool apply(Geometry const& geometry, Strategy const& strategy)
    {
        concepts::check<Geometry>();
        return resolve_strategy::is_convex<Strategy>::apply(geometry, strategy);
    }
};

template <typename Geometry>
struct is_convex<Geometry, dynamic_geometry_tag>
{
    template <typename Strategy>
    static inline bool apply(Geometry const& geometry, Strategy const& strategy)
    {
        bool result = false;
        traits::visit<Geometry>::apply([&](auto const& g)
        {
            result = is_convex<util::remove_cref_t<decltype(g)>>::apply(g, strategy);
        }, geometry);
        return result;
    }
};

// NOTE: This is a simple implementation checking if a GC contains single convex geometry.
//   Technically a GC could store e.g. polygons touching with edges and together creating a convex
//   region. To check this we'd require relate() strategy and the algorithm would be quite complex.
template <typename Geometry>
struct is_convex<Geometry, geometry_collection_tag>
{
    template <typename Strategy>
    static inline bool apply(Geometry const& geometry, Strategy const& strategy)
    {
        bool result = false;
        bool is_first = true;
        detail::visit_breadth_first([&](auto const& g)
        {
            result = is_first
                  && is_convex<util::remove_cref_t<decltype(g)>>::apply(g, strategy);
            is_first = false;
            return result;
        }, geometry);
        return result;
    }
};

} // namespace resolve_dynamic

// TODO: documentation / qbk
template<typename Geometry>
inline bool is_convex(Geometry const& geometry)
{
    return resolve_dynamic::is_convex
            <
                Geometry
            >::apply(geometry, geometry::default_strategy());
}

// TODO: documentation / qbk
template<typename Geometry, typename Strategy>
inline bool is_convex(Geometry const& geometry, Strategy const& strategy)
{
    return resolve_dynamic::is_convex<Geometry>::apply(geometry, strategy);
}


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_IS_CONVEX_HPP
