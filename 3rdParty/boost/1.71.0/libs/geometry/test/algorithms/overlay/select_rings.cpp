// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
//
// This file was modified by Oracle on 2017.
// Modifications copyright (c) 2017 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/foreach.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/typeof/typeof.hpp>

#include <algorithms/test_overlay.hpp>

#include <boost/geometry/geometry.hpp>
#include <boost/geometry/algorithms/detail/overlay/select_rings.hpp>
#include <boost/geometry/algorithms/detail/overlay/assign_parents.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/read.hpp>

#include <boost/assign/list_of.hpp>


template
<
    typename Geometry1,
    typename Geometry2,
    bg::overlay_type OverlayType,
    typename RingIdVector
>
void test_geometry(std::string const& wkt1, std::string const& wkt2,
    RingIdVector const& expected_ids)
{
    typedef bg::detail::overlay::ring_properties
        <
            typename bg::point_type<Geometry1>::type,
            double
        > properties;

    Geometry1 geometry1;
    Geometry2 geometry2;

    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);

    typedef std::map<bg::ring_identifier, properties> map_type;
    map_type selected;
    std::map<bg::ring_identifier, bg::detail::overlay::ring_turn_info> empty;

    typedef typename bg::strategy::intersection::services::default_strategy
        <
            typename bg::cs_tag<Geometry1>::type
        >::type strategy_type;

    bg::detail::overlay::select_rings<OverlayType>(geometry1, geometry2, empty, selected, strategy_type());

    BOOST_CHECK_EQUAL(selected.size(), expected_ids.size());

    if (selected.size() <= expected_ids.size())
    {
        BOOST_AUTO(eit, expected_ids.begin());
        for(typename map_type::const_iterator it = selected.begin(); it != selected.end(); ++it, ++eit)
        {
            bg::ring_identifier const ring_id = it->first;
            BOOST_CHECK_EQUAL(ring_id.source_index, eit->source_index);
            BOOST_CHECK_EQUAL(ring_id.multi_index, eit->multi_index);
            BOOST_CHECK_EQUAL(ring_id.ring_index, eit->ring_index);
        }
    }
}




template <typename P>
void test_all()
{
    // Point in correct clockwise ring -> should return true
    typedef bg::ring_identifier rid;

    test_geometry<bg::model::polygon<P>, bg::model::polygon<P>, bg::overlay_union>(
        winded[0], winded[1],
        boost::assign::list_of
                (rid(0,-1,-1))
                (rid(0,-1, 0))
                (rid(0,-1, 1))
                (rid(0,-1, 3))
                (rid(1,-1, 1))
                (rid(1,-1, 2)));

    test_geometry<bg::model::polygon<P>, bg::model::polygon<P>, bg::overlay_intersection>(
            winded[0], winded[1],
        boost::assign::list_of
                (rid(0,-1, 2))
                (rid(1,-1,-1))
                (rid(1,-1, 0))
                (rid(1,-1, 3)));
}




int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
