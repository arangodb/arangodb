// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2011 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2011 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2011 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_CONVERT_HPP
#define BOOST_GEOMETRY_ALGORITHMS_CONVERT_HPP


#include <cstddef>

#include <boost/numeric/conversion/cast.hpp>
#include <boost/range.hpp>
#include <boost/type_traits/is_array.hpp>

#include <boost/geometry/arithmetic/arithmetic.hpp>
#include <boost/geometry/algorithms/append.hpp>
#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/for_each.hpp>
#include <boost/geometry/algorithms/detail/assign_values.hpp>
#include <boost/geometry/algorithms/detail/assign_box_corners.hpp>
#include <boost/geometry/algorithms/detail/assign_indexed_point.hpp>
#include <boost/geometry/algorithms/detail/convert_point_to_point.hpp>

#include <boost/geometry/views/closeable_view.hpp>
#include <boost/geometry/views/reversible_view.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace conversion
{

template
<
    typename Point,
    typename Box,
    std::size_t Index,
    std::size_t Dimension,
    std::size_t DimensionCount
>
struct point_to_box
{
    static inline void apply(Point const& point, Box& box)
    {
        typedef typename coordinate_type<Box>::type coordinate_type;

        set<Index, Dimension>(box,
                boost::numeric_cast<coordinate_type>(get<Dimension>(point)));
        point_to_box
            <
                Point, Box,
                Index, Dimension + 1, DimensionCount
            >::apply(point, box);
    }
};


template
<
    typename Point,
    typename Box,
    std::size_t Index,
    std::size_t DimensionCount
>
struct point_to_box<Point, Box, Index, DimensionCount, DimensionCount>
{
    static inline void apply(Point const& , Box& )
    {}
};

template <typename Box, typename Range, bool Close, bool Reverse>
struct box_to_range
{
    static inline void apply(Box const& box, Range& range)
    {
        traits::resize<Range>::apply(range, Close ? 5 : 4);
        assign_box_corners_oriented<Reverse>(box, range);
        if (Close)
        {
            range[4] = range[0];
        }
    }
};

template <typename Segment, typename Range>
struct segment_to_range
{
    static inline void apply(Segment const& segment, Range& range)
    {
        traits::resize<Range>::apply(range, 2);

        typename boost::range_iterator<Range>::type it = boost::begin(range);

        assign_point_from_index<0>(segment, *it);
        ++it;
        assign_point_from_index<1>(segment, *it);
    }
};

template 
<
    typename Range1, 
    typename Range2, 
    bool Reverse = false
>
struct range_to_range
{
    typedef typename reversible_view
        <
            Range1 const, 
            Reverse ? iterate_reverse : iterate_forward
        >::type rview_type;
    typedef typename closeable_view
        <
            rview_type const, 
            geometry::closure<Range1>::value
        >::type view_type;

    static inline void apply(Range1 const& source, Range2& destination)
    {
        geometry::clear(destination);

        rview_type rview(source);

        // We consider input always as closed, and skip last
        // point for open output.
        view_type view(rview);

        int n = boost::size(view);
        if (geometry::closure<Range2>::value == geometry::open)
        {
            n--;
        }

        int i = 0;
        for (typename boost::range_iterator<view_type const>::type it
            = boost::begin(view);
            it != boost::end(view) && i < n;
            ++it, ++i)
        {
            geometry::append(destination, *it);
        }
    }
};

template <typename Polygon1, typename Polygon2>
struct polygon_to_polygon
{
    typedef range_to_range
        <
            typename geometry::ring_type<Polygon1>::type, 
            typename geometry::ring_type<Polygon2>::type,
            geometry::point_order<Polygon1>::value
                != geometry::point_order<Polygon2>::value
        > per_ring;

    static inline void apply(Polygon1 const& source, Polygon2& destination)
    {
        // Clearing managed per ring, and in the resizing of interior rings

        per_ring::apply(geometry::exterior_ring(source), 
            geometry::exterior_ring(destination));

        // Container should be resizeable
        traits::resize
            <
                typename boost::remove_reference
                <
                    typename traits::interior_mutable_type<Polygon2>::type
                >::type
            >::apply(interior_rings(destination), num_interior_rings(source));

        typename interior_return_type<Polygon1 const>::type rings_source
                    = interior_rings(source);
        typename interior_return_type<Polygon2>::type rings_dest
                    = interior_rings(destination);

        BOOST_AUTO_TPL(it_source, boost::begin(rings_source));
        BOOST_AUTO_TPL(it_dest, boost::begin(rings_dest));

        for ( ; it_source != boost::end(rings_source); ++it_source, ++it_dest)
        {
            per_ring::apply(*it_source, *it_dest);
        }
    }
};


}} // namespace detail::conversion
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template
<
    bool UseAssignment,
    typename Tag1, typename Tag2,
    std::size_t DimensionCount,
    typename Geometry1, typename Geometry2
>
struct convert
{
    BOOST_MPL_ASSERT_MSG
        (
            false, NOT_OR_NOT_YET_IMPLEMENTED_FOR_THIS_GEOMETRY_TYPES
            , (types<Geometry1, Geometry2>)
        );
};


template
<
    typename Tag,
    std::size_t DimensionCount,
    typename Geometry1, typename Geometry2
>
struct convert<true, Tag, Tag, DimensionCount, Geometry1, Geometry2>
{
    // Same geometry type -> copy whole geometry
    static inline void apply(Geometry1 const& source, Geometry2& destination)
    {
        destination = source;
    }
};


template
<
    std::size_t DimensionCount,
    typename Geometry1, typename Geometry2
>
struct convert<false, point_tag, point_tag, DimensionCount, Geometry1, Geometry2>
    : detail::conversion::point_to_point<Geometry1, Geometry2, 0, DimensionCount>
{};

template <std::size_t DimensionCount, typename Segment, typename LineString>
struct convert<false, segment_tag, linestring_tag, DimensionCount, Segment, LineString>
    : detail::conversion::segment_to_range<Segment, LineString>
{};


template <std::size_t DimensionCount, typename Ring1, typename Ring2>
struct convert<false, ring_tag, ring_tag, DimensionCount, Ring1, Ring2>
    : detail::conversion::range_to_range
        <   
            Ring1, 
            Ring2,
            geometry::point_order<Ring1>::value
                != geometry::point_order<Ring2>::value
        >
{};

template <std::size_t DimensionCount, typename LineString1, typename LineString2>
struct convert<false, linestring_tag, linestring_tag, DimensionCount, LineString1, LineString2>
    : detail::conversion::range_to_range<LineString1, LineString2>
{};

template <std::size_t DimensionCount, typename Polygon1, typename Polygon2>
struct convert<false, polygon_tag, polygon_tag, DimensionCount, Polygon1, Polygon2>
    : detail::conversion::polygon_to_polygon<Polygon1, Polygon2>
{};

template <typename Box, typename Ring>
struct convert<false, box_tag, ring_tag, 2, Box, Ring>
    : detail::conversion::box_to_range
        <
            Box, 
            Ring, 
            geometry::closure<Ring>::value == closed,
            geometry::point_order<Ring>::value == counterclockwise
        >
{};


template <typename Box, typename Polygon>
struct convert<false, box_tag, polygon_tag, 2, Box, Polygon>
{
    static inline void apply(Box const& box, Polygon& polygon)
    {
        typedef typename ring_type<Polygon>::type ring_type;

        convert
            <
                false, box_tag, ring_tag,
                2, Box, ring_type
            >::apply(box, exterior_ring(polygon));
    }
};


template <typename Point, std::size_t DimensionCount, typename Box>
struct convert<false, point_tag, box_tag, DimensionCount, Point, Box>
{
    static inline void apply(Point const& point, Box& box)
    {
        detail::conversion::point_to_box
            <
                Point, Box, min_corner, 0, DimensionCount
            >::apply(point, box);
        detail::conversion::point_to_box
            <
                Point, Box, max_corner, 0, DimensionCount
            >::apply(point, box);
    }
};


template <typename Ring, std::size_t DimensionCount, typename Polygon>
struct convert<false, ring_tag, polygon_tag, DimensionCount, Ring, Polygon>
{
    static inline void apply(Ring const& ring, Polygon& polygon)
    {
        typedef typename ring_type<Polygon>::type ring_type;
        convert
            <
                false, ring_tag, ring_tag, DimensionCount,
                Ring, ring_type
            >::apply(ring, exterior_ring(polygon));
    }
};


template <typename Polygon, std::size_t DimensionCount, typename Ring>
struct convert<false, polygon_tag, ring_tag, DimensionCount, Polygon, Ring>
{
    static inline void apply(Polygon const& polygon, Ring& ring)
    {
        typedef typename ring_type<Polygon>::type ring_type;

        convert
            <
                false,
                ring_tag, ring_tag, DimensionCount,
                ring_type, Ring
            >::apply(exterior_ring(polygon), ring);
    }
};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


/*!
\brief Converts one geometry to another geometry
\details The convert algorithm converts one geometry, e.g. a BOX, to another
geometry, e.g. a RING. This only if it is possible and applicable.
If the point-order is different, or the closure is different between two 
geometry types, it will be converted correctly by explicitly reversing the 
points or closing or opening the polygon rings.
\ingroup convert
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry (source)
\param geometry2 \param_geometry (target)

\qbk{[include reference/algorithms/convert.qbk]}
 */
template <typename Geometry1, typename Geometry2>
inline void convert(Geometry1 const& geometry1, Geometry2& geometry2)
{
    concept::check_concepts_and_equal_dimensions<Geometry1 const, Geometry2>();

    dispatch::convert
        <
            boost::is_same<Geometry1, Geometry2>::value 
                // && boost::has_assign<Geometry2>::value, -- type traits extensions
                && ! boost::is_array<Geometry1>::value,
            typename tag_cast<typename tag<Geometry1>::type, multi_tag>::type,
            typename tag_cast<typename tag<Geometry2>::type, multi_tag>::type,
            dimension<Geometry1>::type::value,
            Geometry1,
            Geometry2
        >::apply(geometry1, geometry2);
}


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_CONVERT_HPP
