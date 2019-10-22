// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2019, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_HAS_SPIKES_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_HAS_SPIKES_HPP

#include <algorithm>

#include <boost/core/ignore_unused.hpp>
#include <boost/range.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/policies/is_valid/default_policy.hpp>

#include <boost/geometry/util/range.hpp>

#include <boost/geometry/views/closeable_view.hpp>

#include <boost/geometry/algorithms/detail/equals/point_point.hpp>
#include <boost/geometry/algorithms/validity_failure_type.hpp>
#include <boost/geometry/algorithms/detail/point_is_spike_or_equal.hpp>
#include <boost/geometry/io/dsv/write.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace is_valid
{

template <typename Point, typename Strategy>
struct equal_to
{
    Point const& m_point;

    equal_to(Point const& point)
        : m_point(point)
    {}

    template <typename OtherPoint>
    inline bool operator()(OtherPoint const& other) const
    {
        return geometry::detail::equals::equals_point_point(m_point, other, Strategy());
    }
};

template <typename Point, typename Strategy>
struct not_equal_to
{
    Point const& m_point;

    not_equal_to(Point const& point)
        : m_point(point)
    {}

    template <typename OtherPoint>
    inline bool operator()(OtherPoint const& other) const
    {
        return ! geometry::detail::equals::equals_point_point(other, m_point, Strategy());
    }
};



template <typename Range, closure_selector Closure>
struct has_spikes
{
    template <typename Iterator, typename SideStrategy>
    static inline Iterator find_different_from_first(Iterator first,
                                                     Iterator last,
                                                     SideStrategy const& )
    {
        typedef not_equal_to
            <
                typename point_type<Range>::type,
                typename SideStrategy::equals_point_point_strategy_type
            > not_equal;

        BOOST_GEOMETRY_ASSERT(first != last);

        Iterator second = first;
        ++second;
        return std::find_if(second, last, not_equal(*first));
    }

    template <typename View, typename VisitPolicy, typename SideStrategy>
    static inline bool apply_at_closure(View const& view, VisitPolicy& visitor,
                                        SideStrategy const& strategy,
                                        bool is_linear)
    {
        boost::ignore_unused(visitor);

        typedef typename boost::range_iterator<View const>::type iterator;

        iterator cur = boost::begin(view);
        typename boost::range_reverse_iterator
            <
                View const
            >::type prev = find_different_from_first(boost::rbegin(view),
                                                     boost::rend(view),
                                                     strategy);

        iterator next = find_different_from_first(cur, boost::end(view),
                                                  strategy);
        if (detail::is_spike_or_equal(*next, *cur, *prev, strategy))
        {
            return
                ! visitor.template apply<failure_spikes>(is_linear, *cur);
        }
        else
        {
            return ! visitor.template apply<no_failure>();
        }
    }


    template <typename VisitPolicy, typename SideStrategy>
    static inline bool apply(Range const& range, VisitPolicy& visitor,
                             SideStrategy const& strategy)
    {
        boost::ignore_unused(visitor);

        typedef typename closeable_view<Range const, Closure>::type view_type;
        typedef typename boost::range_iterator<view_type const>::type iterator; 

        bool const is_linear
            = boost::is_same<typename tag<Range>::type, linestring_tag>::value;

        view_type const view(range);

        iterator prev = boost::begin(view);

        iterator cur = find_different_from_first(prev, boost::end(view), strategy);
        if (cur == boost::end(view))
        {
            // the range has only one distinct point, so it
            // cannot have a spike
            return ! visitor.template apply<no_failure>();
        }

        iterator next = find_different_from_first(cur, boost::end(view), strategy);
        if (next == boost::end(view))
        {
            // the range has only two distinct points, so it
            // cannot have a spike
            return ! visitor.template apply<no_failure>();
        }

        while (next != boost::end(view))
        {
            // Verify spike. TODO: this is a reverse order from expected
            // in is_spike_or_equal, but this order calls the side
            // strategy in the way to correctly detect the spikes,
            // also in geographic cases going over the pole
            if (detail::is_spike_or_equal(*next, *cur, *prev, strategy))
            {
                return
                    ! visitor.template apply<failure_spikes>(is_linear, *cur);
            }
            prev = cur;
            cur = next;
            next = find_different_from_first(cur, boost::end(view), strategy);
        }

        if (geometry::detail::equals::
                equals_point_point(range::front(view), range::back(view),
                                   strategy.get_equals_point_point_strategy()))
        {
            return apply_at_closure(view, visitor, strategy, is_linear);
        }

        return ! visitor.template apply<no_failure>();
    }
};



}} // namespace detail::is_valid
#endif // DOXYGEN_NO_DETAIL


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_HAS_SPIKES_HPP
