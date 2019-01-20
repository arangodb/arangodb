// Boost.Geometry

// Copyright (c) 2017-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_ALGORITHMS_DENSIFY_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DENSIFY_HPP


#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/detail/convert_point_to_point.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/exception.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>
#include <boost/geometry/strategies/densify.hpp>
#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/util/range.hpp>

#include <boost/range/size.hpp>
#include <boost/range/value_type.hpp>

#include <boost/throw_exception.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace densify
{

template <typename Range>
struct push_back_policy
{
    typedef typename boost::range_value<Range>::type point_type;

    inline explicit push_back_policy(Range & rng)
        : m_rng(rng)
    {}

    inline void apply(point_type const& p)
    {
        range::push_back(m_rng, p);
    }

private:
    Range & m_rng;
};

template <typename Range, typename Point>
inline void convert_and_push_back(Range & range, Point const& p)
{
    typename boost::range_value<Range>::type p2;
    geometry::detail::conversion::convert_point_to_point(p, p2);
    range::push_back(range, p2);
}

template <bool AppendLastPoint = true>
struct densify_range
{
    template <typename FwdRng, typename MutRng, typename T, typename Strategy>
    static inline void apply(FwdRng const& rng, MutRng & rng_out,
                             T const& len, Strategy const& strategy)
    {
        typedef typename boost::range_iterator<FwdRng const>::type iterator_t;
        typedef typename boost::range_value<FwdRng>::type point_t;

        iterator_t it = boost::begin(rng);
        iterator_t end = boost::end(rng);

        if (it == end) // empty(rng)
        {
            return;
        }
            
        push_back_policy<MutRng> policy(rng_out);

        iterator_t prev = it;
        for ( ++it ; it != end ; prev = it++)
        {
            point_t const& p0 = *prev;
            point_t const& p1 = *it;

            convert_and_push_back(rng_out, p0);

            strategy.apply(p0, p1, policy, len);
        }

        if (BOOST_GEOMETRY_CONDITION(AppendLastPoint))
        {
            convert_and_push_back(rng_out, *prev); // back(rng)
        }
    }
};

template <bool IsClosed1, bool IsClosed2> // false, X
struct densify_ring
{
    template <typename Geometry, typename GeometryOut, typename T, typename Strategy>
    static inline void apply(Geometry const& ring, GeometryOut & ring_out,
                             T const& len, Strategy const& strategy)
    {
        geometry::detail::densify::densify_range<true>
            ::apply(ring, ring_out, len, strategy);

        if (boost::size(ring) <= 1)
            return;

        typedef typename point_type<Geometry>::type point_t;
        point_t const& p0 = range::back(ring);
        point_t const& p1 = range::front(ring);

        push_back_policy<GeometryOut> policy(ring_out);

        strategy.apply(p0, p1, policy, len);

        if (BOOST_GEOMETRY_CONDITION(IsClosed2))
        {
            convert_and_push_back(ring_out, p1);
        }
    }
};

template <>
struct densify_ring<true, true>
    : densify_range<true>
{};

template <>
struct densify_ring<true, false>
    : densify_range<false>
{};


}} // namespace detail::densify
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{


template
<
    typename Geometry,
    typename GeometryOut,
    typename Tag1 = typename tag<Geometry>::type,
    typename Tag2 = typename tag<GeometryOut>::type
>
struct densify
    : not_implemented<Tag1, Tag2>
{};

template <typename Geometry, typename GeometryOut>
struct densify<Geometry, GeometryOut, linestring_tag, linestring_tag>
    : geometry::detail::densify::densify_range<>
{};

template <typename Geometry, typename GeometryOut>
struct densify<Geometry, GeometryOut, multi_linestring_tag, multi_linestring_tag>
{
    template <typename T, typename Strategy>
    static void apply(Geometry const& mls, GeometryOut & mls_out,
                      T const& len, Strategy const& strategy)
    {
        std::size_t count = boost::size(mls);
        range::resize(mls_out, count);

        for (std::size_t i = 0 ; i < count ; ++i)
        {
            geometry::detail::densify::densify_range<>
                ::apply(range::at(mls, i), range::at(mls_out, i),
                        len, strategy);
        }
    }
};

template <typename Geometry, typename GeometryOut>
struct densify<Geometry, GeometryOut, ring_tag, ring_tag>
    : geometry::detail::densify::densify_ring
        <
            geometry::closure<Geometry>::value != geometry::open,
            geometry::closure<GeometryOut>::value != geometry::open
        >
{};

template <typename Geometry, typename GeometryOut>
struct densify<Geometry, GeometryOut, polygon_tag, polygon_tag>
{
    template <typename T, typename Strategy>
    static void apply(Geometry const& poly, GeometryOut & poly_out,
                      T const& len, Strategy const& strategy)
    {
        apply_ring(exterior_ring(poly), exterior_ring(poly_out),
                   len, strategy);

        std::size_t count = boost::size(interior_rings(poly));
        range::resize(interior_rings(poly_out), count);

        for (std::size_t i = 0 ; i < count ; ++i)
        {
            apply_ring(range::at(interior_rings(poly), i),
                       range::at(interior_rings(poly_out), i),
                       len, strategy);
        }
    }

    template <typename Ring, typename RingOut, typename T, typename Strategy>
    static void apply_ring(Ring const& ring, RingOut & ring_out,
                           T const& len, Strategy const& strategy)
    {
        densify<Ring, RingOut, ring_tag, ring_tag>
            ::apply(ring, ring_out, len, strategy);
    }
};

template <typename Geometry, typename GeometryOut>
struct densify<Geometry, GeometryOut, multi_polygon_tag, multi_polygon_tag>
{
    template <typename T, typename Strategy>
    static void apply(Geometry const& mpoly, GeometryOut & mpoly_out,
                      T const& len, Strategy const& strategy)
    {
        std::size_t count = boost::size(mpoly);
        range::resize(mpoly_out, count);

        for (std::size_t i = 0 ; i < count ; ++i)
        {
            apply_poly(range::at(mpoly, i),
                       range::at(mpoly_out, i),
                       len, strategy);
        }
    }

    template <typename Poly, typename PolyOut, typename T, typename Strategy>
    static void apply_poly(Poly const& poly, PolyOut & poly_out,
                           T const& len, Strategy const& strategy)
    {
        densify<Poly, PolyOut, polygon_tag, polygon_tag>::
            apply(poly, poly_out, len, strategy);
    }
};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_strategy
{

struct densify
{
    template <typename Geometry, typename Distance, typename Strategy>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        dispatch::densify<Geometry, Geometry>
            ::apply(geometry, out, max_distance, strategy);
    }

    template <typename Geometry, typename Distance>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             default_strategy)
    {
        typedef typename strategy::densify::services::default_strategy
            <
                typename cs_tag<Geometry>::type
            >::type strategy_type;
        
        /*BOOST_CONCEPT_ASSERT(
            (concepts::DensifyStrategy<strategy_type>)
        );*/

        apply(geometry, out, max_distance, strategy_type());
    }
};

} // namespace resolve_strategy


namespace resolve_variant {

template <typename Geometry>
struct densify
{
    template <typename Distance, typename Strategy>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        resolve_strategy::densify::apply(geometry, out, max_distance, strategy);
    }
};

template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct densify<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> >
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
            densify<Geometry>::apply(geometry, out, m_max_distance, m_strategy);
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
\brief Densify a geometry using a specified strategy
\ingroup densify
\tparam Geometry \tparam_geometry
\tparam Distance A numerical distance measure
\tparam Strategy A type fulfilling a DensifyStrategy concept
\param geometry Input geometry, to be densified
\param out Output geometry, densified version of the input geometry
\param max_distance Distance threshold (in units depending on strategy)
\param strategy Densify strategy to be used for densification

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/densify.qbk]}

\qbk{
[heading Available Strategies]
\* [link geometry.reference.strategies.strategy_densify_cartesian Cartesian]
\* [link geometry.reference.strategies.strategy_densify_spherical Spherical]
\* [link geometry.reference.strategies.strategy_densify_geographic Geographic]

[heading Example]
[densify_strategy]
[densify_strategy_output]
}
*/
template <typename Geometry, typename Distance, typename Strategy>
inline void densify(Geometry const& geometry,
                    Geometry& out,
                    Distance const& max_distance,
                    Strategy const& strategy)
{
    concepts::check<Geometry>();

    if (max_distance <= Distance(0))
    {
        BOOST_THROW_EXCEPTION(geometry::invalid_input_exception());
    }

    geometry::clear(out);

    resolve_variant::densify
        <
            Geometry
        >::apply(geometry, out, max_distance, strategy);
}


/*!
\brief Densify a geometry
\ingroup densify
\tparam Geometry \tparam_geometry
\tparam Distance A numerical distance measure
\param geometry Input geometry, to be densified
\param out Output geometry, densified version of the input geometry
\param max_distance Distance threshold (in units depending on coordinate system)

\qbk{[include reference/algorithms/densify.qbk]}

\qbk{
[heading Example]
[densify]
[densify_output]
}
*/
template <typename Geometry, typename Distance>
inline void densify(Geometry const& geometry,
                    Geometry& out,
                    Distance const& max_distance)
{
    densify(geometry, out, max_distance, default_strategy());
}


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DENSIFY_HPP
