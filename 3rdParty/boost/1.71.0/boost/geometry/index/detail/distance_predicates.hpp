// Boost.Geometry Index
//
// Spatial index distance predicates, calculators and checkers
// used in nearest query - specialized for envelopes
//
// Copyright (c) 2011-2015 Adam Wulkiewicz, Lodz, Poland.
//
// This file was modified by Oracle on 2019.
// Modifications copyright (c) 2019 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_INDEX_DETAIL_DISTANCE_PREDICATES_HPP
#define BOOST_GEOMETRY_INDEX_DETAIL_DISTANCE_PREDICATES_HPP

#include <boost/geometry/index/detail/algorithms/comparable_distance_near.hpp>
#include <boost/geometry/index/detail/algorithms/comparable_distance_far.hpp>
#include <boost/geometry/index/detail/algorithms/comparable_distance_centroid.hpp>
#include <boost/geometry/index/detail/algorithms/path_intersection.hpp>

#include <boost/geometry/index/detail/tags.hpp>

namespace boost { namespace geometry { namespace index { namespace detail {

// ------------------------------------------------------------------ //
// relations
// ------------------------------------------------------------------ //

template <typename T>
struct to_nearest
{
    to_nearest(T const& v) : value(v) {}
    T value;
};

template <typename T>
struct to_centroid
{
    to_centroid(T const& v) : value(v) {}
    T value;
};

template <typename T>
struct to_furthest
{
    to_furthest(T const& v) : value(v) {}
    T value;
};

// tags

struct to_nearest_tag {};
struct to_centroid_tag {};
struct to_furthest_tag {};

// ------------------------------------------------------------------ //
// relation traits and access
// ------------------------------------------------------------------ //

template <typename T>
struct relation
{
    typedef T value_type;
    typedef to_nearest_tag tag;
    static inline T const& value(T const& v) { return v; }
    static inline T & value(T & v) { return v; }
};

template <typename T>
struct relation< to_nearest<T> >
{
    typedef T value_type;
    typedef to_nearest_tag tag;
    static inline T const& value(to_nearest<T> const& r) { return r.value; }
    static inline T & value(to_nearest<T> & r) { return r.value; }
};

template <typename T>
struct relation< to_centroid<T> >
{
    typedef T value_type;
    typedef to_centroid_tag tag;
    static inline T const& value(to_centroid<T> const& r) { return r.value; }
    static inline T & value(to_centroid<T> & r) { return r.value; }
};

template <typename T>
struct relation< to_furthest<T> >
{
    typedef T value_type;
    typedef to_furthest_tag tag;
    static inline T const& value(to_furthest<T> const& r) { return r.value; }
    static inline T & value(to_furthest<T> & r) { return r.value; }
};

// ------------------------------------------------------------------ //

template
<
    typename G1, typename G2, typename Strategy,
    typename Tag1 = typename geometry::tag<G1>::type,
    typename Tag2 = typename geometry::tag<G2>::type
>
struct comparable_distance_call_base
{
    typedef typename geometry::default_comparable_distance_result
        <
            G1, G2
        >::type result_type;

    static inline result_type apply(G1 const& g1, G2 const& g2, Strategy const&)
    {
        return geometry::comparable_distance(g1, g2);
    }
};

template
<
    typename G1, typename G2, typename Strategy
>
struct comparable_distance_call_base<G1, G2, Strategy, point_tag, point_tag>
{
    typedef typename geometry::comparable_distance_result
        <
            G1, G2,
            typename Strategy::comparable_distance_point_point_strategy_type
        >::type result_type;

    static inline result_type apply(G1 const& g1, G2 const& g2, Strategy const& s)
    {
        return geometry::comparable_distance(g1, g2,
                s.get_comparable_distance_point_point_strategy());
    }
};

template
<
    typename G1, typename G2, typename Strategy
>
struct comparable_distance_call_base<G1, G2, Strategy, point_tag, box_tag>
{
    typedef typename geometry::comparable_distance_result
        <
            G1, G2,
            typename Strategy::comparable_distance_point_box_strategy_type
        >::type result_type;

    static inline result_type apply(G1 const& g1, G2 const& g2, Strategy const& s)
    {
        return geometry::comparable_distance(g1, g2,
                s.get_comparable_distance_point_box_strategy());
    }
};

template
<
    typename G1, typename G2, typename Strategy
>
struct comparable_distance_call_base<G1, G2, Strategy, segment_tag, point_tag>
{
    typedef typename geometry::comparable_distance_result
        <
            G1, G2,
            typename Strategy::comparable_distance_point_segment_strategy_type
        >::type result_type;

    static inline result_type apply(G1 const& g1, G2 const& g2, Strategy const& s)
    {
        return geometry::comparable_distance(g1, g2,
                s.get_comparable_distance_point_segment_strategy());
    }
};

template
<
    typename G1, typename G2, typename Strategy
>
struct comparable_distance_call_base<G1, G2, Strategy, segment_tag, box_tag>
{
    typedef typename geometry::comparable_distance_result
        <
            G1, G2,
            typename Strategy::comparable_distance_segment_box_strategy_type
        >::type result_type;

    static inline result_type apply(G1 const& g1, G2 const& g2, Strategy const& s)
    {
        return geometry::comparable_distance(g1, g2,
                s.get_comparable_distance_segment_box_strategy());
    }
};

template
<
    typename G1, typename G2, typename Strategy
>
struct comparable_distance_call_base<G1, G2, Strategy, segment_tag, segment_tag>
{
    typedef typename geometry::comparable_distance_result
        <
            G1, G2,
            typename Strategy::comparable_distance_point_segment_strategy_type
        >::type result_type;

    static inline result_type apply(G1 const& g1, G2 const& g2, Strategy const& s)
    {
        return geometry::comparable_distance(g1, g2,
                s.get_comparable_distance_point_segment_strategy());
    }
};

template
<
    typename G1, typename G2, typename Strategy
>
struct comparable_distance_call
    : comparable_distance_call_base<G1, G2, Strategy>
{};

template
<
    typename G1, typename G2
>
struct comparable_distance_call<G1, G2, default_strategy>
    : comparable_distance_call_base<G1, G2, default_strategy, void, void>
{};

// ------------------------------------------------------------------ //
// calculate_distance
// ------------------------------------------------------------------ //

template <typename Predicate, typename Indexable, typename Strategy, typename Tag>
struct calculate_distance
{
    BOOST_MPL_ASSERT_MSG((false), INVALID_PREDICATE_OR_TAG, (calculate_distance));
};

// this handles nearest() with default Point parameter, to_nearest() and bounds
template <typename PointRelation, typename Indexable, typename Strategy, typename Tag>
struct calculate_distance< predicates::nearest<PointRelation>, Indexable, Strategy, Tag>
{
    typedef detail::relation<PointRelation> relation;
    typedef comparable_distance_call
        <
            typename relation::value_type,
            Indexable,
            Strategy
        > call_type;
    typedef typename call_type::result_type result_type;

    static inline bool apply(predicates::nearest<PointRelation> const& p, Indexable const& i,
                             Strategy const& s, result_type & result)
    {
        result = call_type::apply(relation::value(p.point_or_relation), i, s);
        return true;
    }
};

template <typename Point, typename Indexable, typename Strategy>
struct calculate_distance< predicates::nearest< to_centroid<Point> >, Indexable, Strategy, value_tag>
{
    typedef Point point_type;
    typedef typename geometry::default_comparable_distance_result
        <
            point_type, Indexable
        >::type result_type;

    static inline bool apply(predicates::nearest< to_centroid<Point> > const& p, Indexable const& i,
                             Strategy const& , result_type & result)
    {
        result = index::detail::comparable_distance_centroid(p.point_or_relation.value, i);
        return true;
    }
};

template <typename Point, typename Indexable, typename Strategy>
struct calculate_distance< predicates::nearest< to_furthest<Point> >, Indexable, Strategy, value_tag>
{
    typedef Point point_type;
    typedef typename geometry::default_comparable_distance_result
        <
            point_type, Indexable
        >::type result_type;

    static inline bool apply(predicates::nearest< to_furthest<Point> > const& p, Indexable const& i,
                             Strategy const& , result_type & result)
    {
        result = index::detail::comparable_distance_far(p.point_or_relation.value, i);
        return true;
    }
};

template <typename SegmentOrLinestring, typename Indexable, typename Strategy, typename Tag>
struct calculate_distance< predicates::path<SegmentOrLinestring>, Indexable, Strategy, Tag>
{
    typedef typename index::detail::default_path_intersection_distance_type<
        Indexable, SegmentOrLinestring
    >::type result_type;

    static inline bool apply(predicates::path<SegmentOrLinestring> const& p, Indexable const& i,
                             Strategy const& , result_type & result)
    {
        return index::detail::path_intersection(i, p.geometry, result);
    }
};

}}}} // namespace boost::geometry::index::detail

#endif // BOOST_GEOMETRY_INDEX_RTREE_DISTANCE_PREDICATES_HPP
