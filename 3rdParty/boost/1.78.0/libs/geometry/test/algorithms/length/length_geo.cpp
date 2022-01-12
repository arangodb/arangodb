// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithms/test_length.hpp>
#include <algorithms/length/linestring_cases.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/adapted/std_pair_as_segment.hpp>

#include <test_geometries/all_custom_linestring.hpp>
#include <test_geometries/wrapped_boost_array.hpp>

template <typename P>
struct geo_strategies
{
    // Set radius type, but for integer coordinates we want to have floating
    // point radius type
    typedef typename bg::promote_floating_point
    <
    typename bg::coordinate_type<P>::type
    >::type rtype;

    typedef bg::srs::spheroid<rtype> stype;
    typedef bg::strategy::distance::andoyer<stype> andoyer_type;
    typedef bg::strategy::distance::thomas<stype> thomas_type;
    typedef bg::strategy::distance::vincenty<stype> vincenty_type;
};

template <typename P>
void test_default() //this should use andoyer strategy
{
    for(std::size_t i = 0; i < 2; ++i)
    {
        test_geometry<bg::model::linestring<P> >(Ls_data_geo[i],
                                                 1116814.237 + 1116152.605);
    }
    // Geometries with length zero
    test_geometry<P>("POINT(0 0)", 0);
    test_geometry<bg::model::polygon<P> >("POLYGON((0 0,0 1,1 1,1 0,0 0))", 0);
}

template <typename P, typename N, typename Strategy>
void test_with_strategy(N exp_length, Strategy strategy)
{
    for(std::size_t i = 0; i < 2; ++i)
    {
        test_geometry<bg::model::linestring<P> >(Ls_data_geo[i],
                                                 exp_length,
                                                 strategy);
    }
    // Geometries with length zero
    test_geometry<P>("POINT(0 0)", 0, strategy);
    test_geometry<bg::model::polygon<P> >("POLYGON((0 0,0 1,1 1,1 0,0 0))", 0,
                                          strategy);
}

template <typename P>
void test_andoyer()
{
    typename geo_strategies<P>::andoyer_type andoyer;
    test_with_strategy<P>(1116814.237 + 1116152.605, andoyer);
}

template <typename P>
void test_thomas()
{
    typename geo_strategies<P>::thomas_type thomas;
    test_with_strategy<P>(1116825.795 + 1116158.7417, thomas);
}

template <typename P>
void test_vincenty()
{
    typename geo_strategies<P>::vincenty_type vincenty;
    test_with_strategy<P>(1116825.857 + 1116159.144, vincenty);
}

template <typename P>
void test_all()
{
    test_default<P>();
    test_andoyer<P>();
    test_thomas<P>();
    test_vincenty<P>();
}

template <typename P>
void test_empty_input()
{
    test_empty_input(bg::model::linestring<P>());
    test_empty_input(bg::model::multi_linestring<P>());
}

int test_main(int, char* [])
{
    // Works only for double(?!)
    //test_all<bg::model::d2::point_xy<int,
    //                              bg::cs::geographic<bg::degree> > >();
    //test_all<bg::model::d2::point_xy<float,
    //                              bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::d2::point_xy<double,
                                  bg::cs::geographic<bg::degree> > >();

    //test_empty_input<bg::model::d2::point_xy<double,
    //                              bg::cs::geographic<bg::degree> > >();

    return 0;
}

