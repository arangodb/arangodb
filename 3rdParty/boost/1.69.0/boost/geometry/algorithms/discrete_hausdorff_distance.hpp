// Boost.Geometry

// Copyright (c) 2018 Yaghyavardhan Singh Khangarot, Hyderabad, India.
// Contributed and/or modified by Yaghyavardhan Singh Khangarot,
//   as part of Google Summer of Code 2018 program.

// This file was modified by Oracle on 2018.
// Modifications copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DISCRETE_HAUSDORFF_DISTANCE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DISCRETE_HAUSDORFF_DISTANCE_HPP

#include <algorithm>

#ifdef BOOST_GEOMETRY_DEBUG_HAUSDORFF_DISTANCE
#include <iostream>
#endif

#include <iterator>
#include <utility>
#include <vector>
#include <limits>

#include <boost/geometry/geometry.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/range.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace bgi = boost::geometry::index;

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace discrete_hausdorff_distance
{

struct point_range
{
    template <typename Point, typename Range, typename Strategy>
    static inline
    typename distance_result
        <
            typename point_type<Point>::type,
            typename point_type<Range>::type,
            Strategy
        >::type
    apply(Point const& pnt, Range const& rng, Strategy const& strategy)
    {
        typedef typename distance_result
            <
                typename point_type<Point>::type,
                typename point_type<Range>::type,
                Strategy
            >::type result_type;
        typedef typename boost::range_size<Range>::type size_type;

        size_type const n = boost::size(rng);
        result_type dis_min = 0;
        bool is_dis_min_set = false;

        for (size_type i = 0 ; i < n ; i++)
        {
            result_type dis_temp = geometry::distance(pnt, range::at(rng, i), strategy);
            if (! is_dis_min_set || dis_temp < dis_min)
            {
                dis_min = dis_temp;
                is_dis_min_set = true;
            }
        }
        return dis_min;
    }
};

struct range_range
{
    template <typename Range1, typename Range2, typename Strategy>
    static inline
    typename distance_result
        <
            typename point_type<Range1>::type,
            typename point_type<Range2>::type,
            Strategy
        >::type
    apply(Range1 const& r1, Range2 const& r2, Strategy const& strategy)
    {

        typedef typename distance_result
            <
                typename point_type<Range1>::type,
                typename point_type<Range2>::type,
                Strategy
            >::type result_type;

        typedef typename boost::range_size<Range1>::type size_type;

        boost::geometry::detail::throw_on_empty_input(r1);
        boost::geometry::detail::throw_on_empty_input(r2);

        size_type const n = boost::size(r1);
        result_type dis_max = 0;

#ifdef BOOST_GEOMETRY_ENABLE_SIMILARITY_RTREE
        typedef typename point_type<Range1>::type point_t;
        typedef bgi::rtree<point_t, bgi::linear<4> > rtree_type;
        rtree_type rtree(boost::begin(r2), boost::end(r2));
        point_t res;
#endif

        for (size_type i = 0 ; i < n ; i++)
        {
#ifdef BOOST_GEOMETRY_ENABLE_SIMILARITY_RTREE
            size_type found = rtree.query(bgi::nearest(range::at(r1, i), 1), &res);
            result_type dis_min = geometry::distance(range::at(r1,i), res);
#else
            result_type dis_min = point_range::apply(range::at(r1, i), r2, strategy);
#endif
            if (dis_min > dis_max )
            {
                dis_max = dis_min;
            }
        }
        return dis_max;
    }
};


struct range_multi_range
{
    template <typename Range, typename Multi_range, typename Strategy>
    static inline
    typename distance_result
        <
            typename point_type<Range>::type,
            typename point_type<Multi_range>::type,
            Strategy
        >::type
    apply(Range const& rng, Multi_range const& mrng, Strategy const& strategy)
    {
        typedef typename distance_result
            <
                typename point_type<Range>::type,
                typename point_type<Multi_range>::type,
                Strategy
            >::type result_type;
        typedef typename boost::range_size<Multi_range>::type size_type;

        boost::geometry::detail::throw_on_empty_input(rng);
        boost::geometry::detail::throw_on_empty_input(mrng);

        size_type b = boost::size(mrng);
        result_type haus_dis = 0;

        for (size_type j = 0 ; j < b ; j++)
        {
            result_type dis_max = range_range::apply(rng, range::at(mrng, j), strategy);
            if (dis_max > haus_dis)
            {
                haus_dis = dis_max;
            }
        }

        return haus_dis;
    }
};


struct multi_range_multi_range
{
    template <typename Multi_Range1, typename Multi_range2, typename Strategy>
    static inline
    typename distance_result
        <
            typename point_type<Multi_Range1>::type,
            typename point_type<Multi_range2>::type,
            Strategy
        >::type
    apply(Multi_Range1 const& mrng1, Multi_range2 const& mrng2, Strategy const& strategy)
    {
        typedef typename distance_result
            <
                typename point_type<Multi_Range1>::type,
                typename point_type<Multi_range2>::type,
                Strategy
            >::type result_type;
        typedef typename boost::range_size<Multi_Range1>::type size_type;

        boost::geometry::detail::throw_on_empty_input(mrng1);
        boost::geometry::detail::throw_on_empty_input(mrng2);
        
        size_type n = boost::size(mrng1);
        result_type haus_dis = 0;

        for (size_type i = 0 ; i < n ; i++)
        {
            result_type dis_max = range_multi_range::apply(range::at(mrng1, i), mrng2, strategy);
            if (dis_max > haus_dis)
            {
                haus_dis = dis_max;
            }
        }
        return haus_dis;
    }
};

}} // namespace detail::hausdorff_distance
#endif // DOXYGEN_NO_DETAIL

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{
template
<
    typename Geometry1,
    typename Geometry2,
    typename Tag1 = typename tag<Geometry1>::type,
    typename Tag2 = typename tag<Geometry2>::type
>
struct discrete_hausdorff_distance : not_implemented<Tag1, Tag2>
{};

// Specialization for point and multi_point
template <typename Point, typename MultiPoint>
struct discrete_hausdorff_distance<Point, MultiPoint, point_tag, multi_point_tag>
    : detail::discrete_hausdorff_distance::point_range
{};

// Specialization for linestrings
template <typename Linestring1, typename Linestring2>
struct discrete_hausdorff_distance<Linestring1, Linestring2, linestring_tag, linestring_tag>
    : detail::discrete_hausdorff_distance::range_range
{};

// Specialization for multi_point-multi_point
template <typename MultiPoint1, typename MultiPoint2>
struct discrete_hausdorff_distance<MultiPoint1, MultiPoint2, multi_point_tag, multi_point_tag>
    : detail::discrete_hausdorff_distance::range_range
{};

// Specialization for linestring and multi_linestring
template <typename linestring, typename multi_linestring>
struct discrete_hausdorff_distance<linestring, multi_linestring, linestring_tag, multi_linestring_tag>
    : detail::discrete_hausdorff_distance::range_multi_range
{};

// Specialization for multi_linestring and multi_linestring
template <typename multi_linestring1, typename multi_linestring2>
struct discrete_hausdorff_distance<multi_linestring1, multi_linestring2, multi_linestring_tag, multi_linestring_tag>
    : detail::discrete_hausdorff_distance::multi_range_multi_range
{};

} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH

// Algorithm overload using explicitly passed Pt-Pt distance strategy

/*!
\brief Calculate discrete Hausdorff distance between two geometries (currently
    works for LineString-LineString, MultiPoint-MultiPoint, Point-MultiPoint,
    MultiLineString-MultiLineString) using specified strategy.
\ingroup discrete_hausdorff_distance
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\tparam Strategy A type fulfilling a DistanceStrategy concept
\param geometry1 Input geometry
\param geometry2 Input geometry
\param strategy Distance strategy to be used to calculate Pt-Pt distance

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/discrete_hausdorff_distance.qbk]}

\qbk{
[heading Available Strategies]
\* [link geometry.reference.strategies.strategy_distance_pythagoras Pythagoras (cartesian)]
\* [link geometry.reference.strategies.strategy_distance_haversine Haversine (spherical)]
[/ \* more (currently extensions): Vincenty\, Andoyer (geographic) ]

[heading Example]
[discrete_hausdorff_distance_strategy]
[discrete_hausdorff_distance_strategy_output]
}
*/
template <typename Geometry1, typename Geometry2, typename Strategy>
inline
typename distance_result
    <
        typename point_type<Geometry1>::type,
        typename point_type<Geometry2>::type,
        Strategy
    >::type
discrete_hausdorff_distance(Geometry1 const& geometry1,
                            Geometry2 const& geometry2,
                            Strategy const& strategy)
{
    return dispatch::discrete_hausdorff_distance
        <
            Geometry1, Geometry2
        >::apply(geometry1, geometry2, strategy);
}

/*!
\brief Calculate discrete Hausdorff distance between two geometries (currently
    works for LineString-LineString, MultiPoint-MultiPoint, Point-MultiPoint,
    MultiLineString-MultiLineString).
\ingroup discrete_hausdorff_distance
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 Input geometry
\param geometry2 Input geometry

\qbk{[include reference/algorithms/discrete_hausdorff_distance.qbk]}

\qbk{
[heading Example]
[discrete_hausdorff_distance]
[discrete_hausdorff_distance_output]
}
*/
template <typename Geometry1, typename Geometry2>
inline
typename distance_result
    <
        typename point_type<Geometry1>::type,
        typename point_type<Geometry2>::type
    >::type
discrete_hausdorff_distance(Geometry1 const& geometry1,
                            Geometry2 const& geometry2)
{
    typedef typename strategy::distance::services::default_strategy
        <
            point_tag, point_tag,
            typename point_type<Geometry1>::type,
            typename point_type<Geometry2>::type
        >::type strategy_type;

    return discrete_hausdorff_distance(geometry1, geometry2, strategy_type());
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DISCRETE_HAUSDORFF_DISTANCE_HPP
