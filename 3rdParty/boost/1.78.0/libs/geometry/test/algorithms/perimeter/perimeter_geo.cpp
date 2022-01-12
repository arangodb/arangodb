// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016-2017 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithms/test_perimeter.hpp>
#include <algorithms/perimeter/perimeter_polygon_cases.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

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
    for (std::size_t i = 0; i <= 2; ++i)
    {
        test_geometry<bg::model::polygon<P> >(poly_data_geo[i],
                                              1116814.237 + 1116152.605);
    }

    //since the geodesic distance is the shortest path it should go over the pole
    //in this case; thus the correct perimeter is the meridian length (below)
    //and not 40075160 which is the legth of the equator
    test_geometry<bg::model::polygon<P> >(poly_data_geo[3],
                                          40007834.7139);

    //force to use equator path
    test_geometry<bg::model::polygon<P> >(poly_data_geo[4],
                                          40075016.6856);

    // Multipolygon
    test_geometry<bg::model::multi_polygon<bg::model::polygon<P> > >
                                            (multipoly_data[0], 60011752.0709);

    // Geometries with length zero
    test_geometry<P>("POINT(0 0)", 0);
    test_geometry<bg::model::linestring<P> >("LINESTRING(0 0,0 1,1 1,1 0,0 0)",
                                             0);
}

template <typename P, typename N, typename Strategy>
void test_with_strategy(N exp_length, Strategy strategy)
{
    for (std::size_t i = 0; i <= 2; ++i)
    {
        test_geometry<bg::model::polygon<P> >(poly_data_geo[i],
                                              exp_length,
                                              strategy);
    }
    // Geometries with length zero
    test_geometry<P>("POINT(0 0)", 0, strategy);
    test_geometry<bg::model::linestring<P> >("LINESTRING(0 0,0 1,1 1,1 0,0 0)",
                                             0,
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

int test_main(int, char* [])
{
    // Works only for double(?!)
    //test_all<bg::model::d2::point_xy<int,
    //                              bg::cs::geographic<bg::degree> > >();
    //test_all<bg::model::d2::point_xy<float,
    //                              bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::d2::point_xy<double,
                                  bg::cs::geographic<bg::degree> > >();

    return 0;
}

