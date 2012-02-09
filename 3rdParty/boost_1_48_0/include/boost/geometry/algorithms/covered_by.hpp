// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2011 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2011 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2011 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_COVERED_BY_HPP
#define BOOST_GEOMETRY_ALGORITHMS_COVERED_BY_HPP


#include <cstddef>


#include <boost/geometry/algorithms/within.hpp>


#include <boost/geometry/strategies/cartesian/point_in_box.hpp>
#include <boost/geometry/strategies/cartesian/box_in_box.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template
<
    typename Tag1,
    typename Tag2,
    typename Geometry1,
    typename Geometry2,
    typename Strategy
>
struct covered_by
{
    BOOST_MPL_ASSERT_MSG
        (
            false, NOT_OR_NOT_YET_IMPLEMENTED_FOR_THIS_GEOMETRY_TYPE
            , (types<Geometry1, Geometry2>)
        );
};


template <typename Point, typename Box, typename Strategy>
struct covered_by<point_tag, box_tag, Point, Box, Strategy>
{
    static inline bool apply(Point const& point, Box const& box, Strategy const& strategy)
    {
        return strategy.apply(point, box);
    }
};

template <typename Box1, typename Box2, typename Strategy>
struct covered_by<box_tag, box_tag, Box1, Box2, Strategy>
{
    static inline bool apply(Box1 const& box1, Box2 const& box2, Strategy const& strategy)
    {
        assert_dimension_equal<Box1, Box2>();
        return strategy.apply(box1, box2);
    }
};



template <typename Point, typename Ring, typename Strategy>
struct covered_by<point_tag, ring_tag, Point, Ring, Strategy>
{
    static inline bool apply(Point const& point, Ring const& ring, Strategy const& strategy)
    {
        return detail::within::point_in_ring
            <
                Point,
                Ring,
                order_as_direction<geometry::point_order<Ring>::value>::value,
                geometry::closure<Ring>::value,
                Strategy
            >::apply(point, ring) >= 0;
    }
};

template <typename Point, typename Polygon, typename Strategy>
struct covered_by<point_tag, polygon_tag, Point, Polygon, Strategy>
{
    static inline bool apply(Point const& point, Polygon const& polygon, Strategy const& strategy)
    {
        return detail::within::point_in_polygon
        <
            Point,
            Polygon,
            order_as_direction<geometry::point_order<Polygon>::value>::value,
            geometry::closure<Polygon>::value,
            Strategy
        >::apply(point, polygon, strategy) >= 0;
    }
};

} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


/*!
\brief \brief_check12{is inside or on border}
\ingroup covered_by
\details \details_check12{covered_by, is inside or on border}.
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param geometry1 geometry which might be covered_by the second geometry
\param geometry2 geometry which might contain the first geometry
\return true if geometry1 is completely contained covered_by geometry2,
    else false
\note The default strategy is used for covered_by detection

 */
template<typename Geometry1, typename Geometry2>
inline bool covered_by(Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    concept::check<Geometry1 const>();
    concept::check<Geometry2 const>();
    assert_dimension_equal<Geometry1, Geometry2>();

    typedef typename point_type<Geometry1>::type point_type1;
    typedef typename point_type<Geometry2>::type point_type2;

    typedef typename strategy::covered_by::services::default_strategy
        <
            typename tag<Geometry1>::type,
            typename tag<Geometry2>::type,
            typename tag<Geometry1>::type,
            typename tag_cast<typename tag<Geometry2>::type, areal_tag>::type,
            typename tag_cast
                <
                    typename cs_tag<point_type1>::type, spherical_tag
                >::type,
            typename tag_cast
                <
                    typename cs_tag<point_type2>::type, spherical_tag
                >::type,
            Geometry1,
            Geometry2
        >::type strategy_type;

    return dispatch::covered_by
        <
            typename tag<Geometry1>::type,
            typename tag<Geometry2>::type,
            Geometry1,
            Geometry2,
            strategy_type
        >::apply(geometry1, geometry2, strategy_type());
}

/*!
\brief \brief_check12{is inside or on border} \brief_strategy
\ingroup covered_by
\details \details_check12{covered_by, is inside or on border}, \brief_strategy. \details_strategy_reasons
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param geometry1 \param_geometry geometry which might be covered_by the second geometry
\param geometry2 \param_geometry which might contain the first geometry
\param strategy strategy to be used
\return true if geometry1 is completely contained covered_by geometry2,
    else false

\qbk{distinguish,with strategy}

*/
template<typename Geometry1, typename Geometry2, typename Strategy>
inline bool covered_by(Geometry1 const& geometry1, Geometry2 const& geometry2,
        Strategy const& strategy)
{
    concept::within::check
        <
            typename tag<Geometry1>::type, 
            typename tag<Geometry2>::type, 
            typename tag_cast<typename tag<Geometry2>::type, areal_tag>::type,
            Strategy
        >();
    concept::check<Geometry1 const>();
    concept::check<Geometry2 const>();
    assert_dimension_equal<Geometry1, Geometry2>();

    return dispatch::covered_by
        <
            typename tag<Geometry1>::type,
            typename tag<Geometry2>::type,
            Geometry1,
            Geometry2,
            Strategy
        >::apply(geometry1, geometry2, strategy);
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_COVERED_BY_HPP
