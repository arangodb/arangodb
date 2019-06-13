// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_SIMPLIFY_HPP
#define BOOST_GEOMETRY_ALGORITHMS_SIMPLIFY_HPP

#include <cstddef>

#include <boost/core/ignore_unused.hpp>
#include <boost/range.hpp>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant_fwd.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/mutable_range.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/strategies/agnostic/simplify_douglas_peucker.hpp>
#include <boost/geometry/strategies/concepts/simplify_concept.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>
#include <boost/geometry/strategies/distance.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/convert.hpp>
#include <boost/geometry/algorithms/detail/equals/point_point.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>
#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>

#include <boost/geometry/algorithms/detail/distance/default_strategies.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace simplify
{

template <typename Range>
inline bool is_degenerate(Range const& range)
{
    return boost::size(range) == 2
        && detail::equals::equals_point_point(geometry::range::front(range),
                                              geometry::range::back(range));
}

struct simplify_range_insert
{
    template<typename Range, typename Strategy, typename OutputIterator, typename Distance>
    static inline void apply(Range const& range, OutputIterator out,
                             Distance const& max_distance, Strategy const& strategy)
    {
        boost::ignore_unused(strategy);

        if (is_degenerate(range))
        {
            std::copy(boost::begin(range), boost::begin(range) + 1, out);
        }
        else if (boost::size(range) <= 2 || max_distance < 0)
        {
            std::copy(boost::begin(range), boost::end(range), out);
        }
        else
        {
            strategy.apply(range, out, max_distance);
        }
    }
};


struct simplify_copy
{
    template <typename RangeIn, typename RangeOut, typename Strategy, typename Distance>
    static inline void apply(RangeIn const& range, RangeOut& out,
                             Distance const& , Strategy const& )
    {
        std::copy
            (
                boost::begin(range), boost::end(range),
                    geometry::range::back_inserter(out)
            );
    }
};


template <std::size_t MinimumToUseStrategy>
struct simplify_range
{
    template <typename RangeIn, typename RangeOut, typename Strategy, typename Distance>
    static inline void apply(RangeIn const& range, RangeOut& out,
                    Distance const& max_distance, Strategy const& strategy)
    {
        // For a RING:
        // Note that, especially if max_distance is too large,
        // the output ring might be self intersecting while the input ring is
        // not, although chances are low in normal polygons

        if (boost::size(range) <= MinimumToUseStrategy || max_distance < 0)
        {
            simplify_copy::apply(range, out, max_distance, strategy);
        }
        else
        {
            simplify_range_insert::apply
                (
                    range, geometry::range::back_inserter(out), max_distance, strategy
                );
        }

        // Verify the two remaining points are equal. If so, remove one of them.
        // This can cause the output being under the minimum size
        if (is_degenerate(out))
        {
            range::resize(out, 1);
        }
    }
};

struct simplify_ring
{
private :
    template <typename Area>
    static inline int area_sign(Area const& area)
    {
        return area > 0 ? 1 : area < 0 ? -1 : 0;
    }

    template <typename Strategy, typename Ring>
    static std::size_t get_opposite(std::size_t index, Ring const& ring)
    {
        typename Strategy::distance_strategy_type distance_strategy;

        // Verify if it is NOT the case that all points are less than the
        // simplifying distance. If so, output is empty.
        typename Strategy::distance_type max_distance(-1);

        typename geometry::point_type<Ring>::type point = range::at(ring, index);
        std::size_t i = 0;
        for (typename boost::range_iterator<Ring const>::type
                it = boost::begin(ring); it != boost::end(ring); ++it, ++i)
        {
            // This actually is point-segment distance but will result
            // in point-point distance
            typename Strategy::distance_type dist = distance_strategy.apply(*it, point, point);
            if (dist > max_distance)
            {
                max_distance = dist;
                index = i;
            }
        }
        return index;
    }

public :
    template <typename Ring, typename Strategy, typename Distance>
    static inline void apply(Ring const& ring, Ring& out,
                    Distance const& max_distance, Strategy const& strategy)
    {
        std::size_t const size = boost::size(ring);
        if (size == 0)
        {
            return;
        }

        int const input_sign = area_sign(geometry::area(ring));

        std::set<std::size_t> visited_indexes;

        // Rotate it into a copied vector
        // (vector, because source type might not support rotation)
        // (duplicate end point will be simplified away)
        typedef typename geometry::point_type<Ring>::type point_type;

        std::vector<point_type> rotated(size);

        // Closing point (but it will not start here)
        std::size_t index = 0;

        // Iterate (usually one iteration is enough)
        for (std::size_t iteration = 0; iteration < 4u; iteration++)
        {
            // Always take the opposite. Opposite guarantees that no point
            // "halfway" is chosen, creating an artefact (very narrow triangle)
            // Iteration 0: opposite to closing point (1/2, = on convex hull)
            //              (this will start simplification with that point
            //               and its opposite ~0)
            // Iteration 1: move a quarter on that ring, then opposite to 1/4
            //              (with its opposite 3/4)
            // Iteration 2: move an eight on that ring, then opposite (1/8)
            // Iteration 3: again move a quarter, then opposite (7/8)
            // So finally 8 "sides" of the ring have been examined (if it were
            // a semi-circle). Most probably, there are only 0 or 1 iterations.
            switch (iteration)
            {
                case 1 : index = (index + size / 4) % size; break;
                case 2 : index = (index + size / 8) % size; break;
                case 3 : index = (index + size / 4) % size; break;
            }
            index = get_opposite<Strategy>(index, ring);

            if (visited_indexes.count(index) > 0)
            {
                // Avoid trying the same starting point more than once
                continue;
            }

            std::rotate_copy(boost::begin(ring), range::pos(ring, index),
                             boost::end(ring), rotated.begin());

            // Close the rotated copy
            rotated.push_back(range::at(ring, index));

            simplify_range<0>::apply(rotated, out, max_distance, strategy);

            // Verify that what was positive, stays positive (or goes to 0)
            // and what was negative stays negative (or goes to 0)
            int const output_sign = area_sign(geometry::area(out));
            if (output_sign == input_sign)
            {
                // Result is considered as satisfactory (usually this is the
                // first iteration - only for small rings, having a scale
                // similar to simplify_distance, next iterations are tried
                return;
            }

            // Original is simplified away. Possibly there is a solution
            // when another starting point is used
            geometry::clear(out);

            if (iteration == 0
                && geometry::perimeter(ring) < 3 * max_distance)
            {
                // Check if it is useful to iterate. A minimal triangle has a
                // perimeter of a bit more than 3 times the simplify distance
                return;
            }

            // Prepare next try
            visited_indexes.insert(index);
            rotated.resize(size);
        }
    }
};


struct simplify_polygon
{
private:

    template
    <
        typename IteratorIn,
        typename InteriorRingsOut,
        typename Distance,
        typename Strategy
    >
    static inline void iterate(IteratorIn begin, IteratorIn end,
                    InteriorRingsOut& interior_rings_out,
                    Distance const& max_distance, Strategy const& strategy)
    {
        typedef typename boost::range_value<InteriorRingsOut>::type single_type;
        for (IteratorIn it = begin; it != end; ++it)
        {
            single_type out;
            simplify_ring::apply(*it, out, max_distance, strategy);
            if (! geometry::is_empty(out))
            {
                range::push_back(interior_rings_out, out);
            }
        }
    }

    template
    <
        typename InteriorRingsIn,
        typename InteriorRingsOut,
        typename Distance,
        typename Strategy
    >
    static inline void apply_interior_rings(
                    InteriorRingsIn const& interior_rings_in,
                    InteriorRingsOut& interior_rings_out,
                    Distance const& max_distance, Strategy const& strategy)
    {
        range::clear(interior_rings_out);

        iterate(
            boost::begin(interior_rings_in), boost::end(interior_rings_in),
            interior_rings_out,
            max_distance, strategy);
    }

public:
    template <typename Polygon, typename Strategy, typename Distance>
    static inline void apply(Polygon const& poly_in, Polygon& poly_out,
                    Distance const& max_distance, Strategy const& strategy)
    {
        // Note that if there are inner rings, and distance is too large,
        // they might intersect with the outer ring in the output,
        // while it didn't in the input.
        simplify_ring::apply(exterior_ring(poly_in), exterior_ring(poly_out),
            max_distance, strategy);

        apply_interior_rings(interior_rings(poly_in),
            interior_rings(poly_out), max_distance, strategy);
    }
};


template<typename Policy>
struct simplify_multi
{
    template <typename MultiGeometry, typename Strategy, typename Distance>
    static inline void apply(MultiGeometry const& multi, MultiGeometry& out,
                    Distance const& max_distance, Strategy const& strategy)
    {
        range::clear(out);

        typedef typename boost::range_value<MultiGeometry>::type single_type;

        for (typename boost::range_iterator<MultiGeometry const>::type
                it = boost::begin(multi); it != boost::end(multi); ++it)
        {
            single_type single_out;
            Policy::apply(*it, single_out, max_distance, strategy);
            if (! geometry::is_empty(single_out))
            {
                range::push_back(out, single_out);
            }
        }
    }
};


}} // namespace detail::simplify
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template
<
    typename Geometry,
    typename Tag = typename tag<Geometry>::type
>
struct simplify: not_implemented<Tag>
{};

template <typename Point>
struct simplify<Point, point_tag>
{
    template <typename Distance, typename Strategy>
    static inline void apply(Point const& point, Point& out,
                    Distance const& , Strategy const& )
    {
        geometry::convert(point, out);
    }
};

// Linestring, keep 2 points (unless those points are the same)
template <typename Linestring>
struct simplify<Linestring, linestring_tag>
    : detail::simplify::simplify_range<2>
{};

template <typename Ring>
struct simplify<Ring, ring_tag>
    : detail::simplify::simplify_ring
{};

template <typename Polygon>
struct simplify<Polygon, polygon_tag>
    : detail::simplify::simplify_polygon
{};


template
<
    typename Geometry,
    typename Tag = typename tag<Geometry>::type
>
struct simplify_insert: not_implemented<Tag>
{};


template <typename Linestring>
struct simplify_insert<Linestring, linestring_tag>
    : detail::simplify::simplify_range_insert
{};

template <typename Ring>
struct simplify_insert<Ring, ring_tag>
    : detail::simplify::simplify_range_insert
{};

template <typename MultiPoint>
struct simplify<MultiPoint, multi_point_tag>
    : detail::simplify::simplify_copy
{};


template <typename MultiLinestring>
struct simplify<MultiLinestring, multi_linestring_tag>
    : detail::simplify::simplify_multi<detail::simplify::simplify_range<2> >
{};


template <typename MultiPolygon>
struct simplify<MultiPolygon, multi_polygon_tag>
    : detail::simplify::simplify_multi<detail::simplify::simplify_polygon>
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_strategy
{

struct simplify
{
    template <typename Geometry, typename Distance, typename Strategy>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        dispatch::simplify<Geometry>::apply(geometry, out, max_distance, strategy);
    }

    template <typename Geometry, typename Distance>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             default_strategy)
    {
        typedef typename point_type<Geometry>::type point_type;

        typedef typename strategy::distance::services::default_strategy
        <
            point_tag, segment_tag, point_type
        >::type ds_strategy_type;

        typedef strategy::simplify::douglas_peucker
        <
            point_type, ds_strategy_type
        > strategy_type;

        BOOST_CONCEPT_ASSERT(
            (concepts::SimplifyStrategy<strategy_type, point_type>)
        );

        apply(geometry, out, max_distance, strategy_type());
    }
};

struct simplify_insert
{
    template
    <
        typename Geometry,
        typename OutputIterator,
        typename Distance,
        typename Strategy
    >
    static inline void apply(Geometry const& geometry,
                             OutputIterator& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        dispatch::simplify_insert<Geometry>::apply(geometry, out, max_distance, strategy);
    }

    template <typename Geometry, typename OutputIterator, typename Distance>
    static inline void apply(Geometry const& geometry,
                             OutputIterator& out,
                             Distance const& max_distance,
                             default_strategy)
    {
        typedef typename point_type<Geometry>::type point_type;

        typedef typename strategy::distance::services::default_strategy
        <
            point_tag, segment_tag, point_type
        >::type ds_strategy_type;

        typedef strategy::simplify::douglas_peucker
        <
            point_type, ds_strategy_type
        > strategy_type;

        BOOST_CONCEPT_ASSERT(
            (concepts::SimplifyStrategy<strategy_type, point_type>)
        );

        apply(geometry, out, max_distance, strategy_type());
    }
};

} // namespace resolve_strategy


namespace resolve_variant {

template <typename Geometry>
struct simplify
{
    template <typename Distance, typename Strategy>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        resolve_strategy::simplify::apply(geometry, out, max_distance, strategy);
    }
};

template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct simplify<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> >
{
    template <typename Distance, typename Strategy>
    struct visitor: boost::static_visitor<void>
    {
        Distance const& m_max_distance;
        Strategy const& m_strategy;

        visitor(Distance const& max_distance, Strategy const& strategy)
            : m_max_distance(max_distance)
            , m_strategy(strategy)
        {}

        template <typename Geometry>
        void operator()(Geometry const& geometry, Geometry& out) const
        {
            simplify<Geometry>::apply(geometry, out, m_max_distance, m_strategy);
        }
    };

    template <typename Distance, typename Strategy>
    static inline void
    apply(boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> const& geometry,
          boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>& out,
          Distance const& max_distance,
          Strategy const& strategy)
    {
        boost::apply_visitor(
            visitor<Distance, Strategy>(max_distance, strategy),
            geometry,
            out
        );
    }
};

} // namespace resolve_variant


/*!
\brief Simplify a geometry using a specified strategy
\ingroup simplify
\tparam Geometry \tparam_geometry
\tparam Distance A numerical distance measure
\tparam Strategy A type fulfilling a SimplifyStrategy concept
\param strategy A strategy to calculate simplification
\param geometry input geometry, to be simplified
\param out output geometry, simplified version of the input geometry
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed
\param strategy simplify strategy to be used for simplification, might
    include point-distance strategy

\image html svg_simplify_country.png "The image below presents the simplified country"
\qbk{distinguish,with strategy}
*/
template<typename Geometry, typename Distance, typename Strategy>
inline void simplify(Geometry const& geometry, Geometry& out,
                     Distance const& max_distance, Strategy const& strategy)
{
    concepts::check<Geometry>();

    geometry::clear(out);

    resolve_variant::simplify<Geometry>::apply(geometry, out, max_distance, strategy);
}




/*!
\brief Simplify a geometry
\ingroup simplify
\tparam Geometry \tparam_geometry
\tparam Distance \tparam_numeric
\note This version of simplify simplifies a geometry using the default
    strategy (Douglas Peucker),
\param geometry input geometry, to be simplified
\param out output geometry, simplified version of the input geometry
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed

\qbk{[include reference/algorithms/simplify.qbk]}
 */
template<typename Geometry, typename Distance>
inline void simplify(Geometry const& geometry, Geometry& out,
                     Distance const& max_distance)
{
    concepts::check<Geometry>();

    geometry::simplify(geometry, out, max_distance, default_strategy());
}


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace simplify
{


/*!
\brief Simplify a geometry, using an output iterator
    and a specified strategy
\ingroup simplify
\tparam Geometry \tparam_geometry
\param geometry input geometry, to be simplified
\param out output iterator, outputs all simplified points
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed
\param strategy simplify strategy to be used for simplification,
    might include point-distance strategy

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/simplify.qbk]}
*/
template<typename Geometry, typename OutputIterator, typename Distance, typename Strategy>
inline void simplify_insert(Geometry const& geometry, OutputIterator out,
                            Distance const& max_distance, Strategy const& strategy)
{
    concepts::check<Geometry const>();

    resolve_strategy::simplify_insert::apply(geometry, out, max_distance, strategy);
}

/*!
\brief Simplify a geometry, using an output iterator
\ingroup simplify
\tparam Geometry \tparam_geometry
\param geometry input geometry, to be simplified
\param out output iterator, outputs all simplified points
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed

\qbk{[include reference/algorithms/simplify_insert.qbk]}
 */
template<typename Geometry, typename OutputIterator, typename Distance>
inline void simplify_insert(Geometry const& geometry, OutputIterator out,
                            Distance const& max_distance)
{
    // Concept: output point type = point type of input geometry
    concepts::check<Geometry const>();
    concepts::check<typename point_type<Geometry>::type>();

    simplify_insert(geometry, out, max_distance, default_strategy());
}

}} // namespace detail::simplify
#endif // DOXYGEN_NO_DETAIL



}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_SIMPLIFY_HPP
