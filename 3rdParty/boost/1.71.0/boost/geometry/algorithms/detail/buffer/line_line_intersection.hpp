// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2012-2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_BUFFER_LINE_LINE_INTERSECTION_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_BUFFER_LINE_LINE_INTERSECTION_HPP


#include <boost/geometry/util/math.hpp>
#include <boost/geometry/strategies/buffer.hpp>
#include <boost/geometry/algorithms/detail/buffer/parallel_continue.hpp>

namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace buffer
{


// TODO: once change this to proper strategy
// It is different from current segment intersection because these are not segments but lines
// If we have the Line concept, we can create a strategy
// Assumes a convex corner
struct line_line_intersection
{

    template <typename Point>
    static inline strategy::buffer::join_selector apply(Point const& pi, Point const& pj,
        Point const& qi, Point const& qj, Point& ip)
    {
        typedef typename coordinate_type<Point>::type ct;

        // Construct lines in general form (ax + by + c = 0),
        // (will be replaced by a general_form structure in a next PR)
        ct const pa = get<1>(pi) - get<1>(pj);
        ct const pb = get<0>(pj) - get<0>(pi);
        ct const pc = -pa * get<0>(pi) - pb * get<1>(pi);

        ct const qa = get<1>(qi) - get<1>(qj);
        ct const qb = get<0>(qj) - get<0>(qi);
        ct const qc = -qa * get<0>(qi) - qb * get<1>(qi);

        ct const denominator = pb * qa - pa * qb;

        // Even if the corner was checked before (so it is convex now), that
        // was done on the original geometry. This function runs on the buffered
        // geometries, where sides are generated and might be slightly off. In
        // Floating Point, that slightly might just exceed the limit and we have
        // to check it again.

        // For round joins, it will not be used at all.
        // For miter joins, there is a miter limit
        // If segments are parallel/collinear we must be distinguish two cases:
        // they continue each other, or they form a spike
        ct const zero = ct();
        if (math::equals(denominator, zero))
        {
            return parallel_continue(qb, -qa, pb, -pa)
                ? strategy::buffer::join_continue
                : strategy::buffer::join_spike
                ;
        }

        set<0>(ip, (pc * qb - pb * qc) / denominator);
        set<1>(ip, (pa * qc - pc * qa) / denominator);

        return strategy::buffer::join_convex;
    }
};


}} // namespace detail::buffer
#endif // DOXYGEN_NO_DETAIL


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_BUFFER_LINE_LINE_INTERSECTION_HPP
