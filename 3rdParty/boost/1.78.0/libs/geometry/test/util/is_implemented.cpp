// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014 Samuel Debionne, Grenoble, France.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/core/reverse_dispatch.hpp>
#include <boost/geometry/core/tag_cast.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/strategies/tags.hpp>

#include <boost/geometry/algorithms/not_implemented.hpp>

#include <boost/geometry/util/is_implemented.hpp>

#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/bool.hpp>


namespace boost { namespace geometry
{

namespace strategy { namespace services
{


template <typename Strategy> struct tag
{

    typedef not_implemented type;

};

}} // namespace strategy::services

    
template
<
    typename Geometry1, typename Geometry2,
    typename Strategy,
    typename Tag1 = typename tag_cast<typename tag<Geometry1>::type, multi_tag>::type,
    typename Tag2 = typename tag_cast<typename tag<Geometry2>::type, multi_tag>::type,
    typename StrategyTag = typename strategy::services::tag<Strategy>::type,
    bool Reverse = reverse_dispatch<Geometry1, Geometry2>::type::value
>
struct algorithm_archetype
    : not_implemented<>
{};


struct strategy_archetype
{
    template <typename Geometry1, typename Geometry2>
    static void apply(Geometry1, Geometry2) {}
};


}} // namespace boost::geometry


int test_main(int, char* [])
{
    typedef bg::model::d2::point_xy<double> point_type;

    BOOST_MPL_ASSERT((
        std::is_same<
            bg::util::is_implemented2
            <
                point_type, point_type,
                bg::algorithm_archetype<point_type, point_type, bg::strategy_archetype>
            >::type,
            boost::mpl::false_
        >::value
    ));

    return 0;
}
