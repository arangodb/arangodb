// Boost.Geometry

// Copyright (c) 2016-2017 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/strategies/geographic/envelope_segment.hpp>
#include <boost/geometry/strategies/spherical/envelope_segment.hpp>

template
<
    typename FormulaPolicy,
    typename P,
    typename CT
>
void test_strategies_lat(P p1, P p2, CT expected_max, CT expected_min,
                         CT expected_max_sph, CT expected_min_sph, CT error = 0.0001)
{
    bg::model::box<P> box;

    bg::strategy::envelope::geographic_segment
        <
            FormulaPolicy,
            bg::srs::spheroid<CT>,
            CT
        > strategy_geo;

    strategy_geo.apply(p1, p2, box);
    
    CT p_min_degree_geo = bg::get<0, 1>(box);
    CT p_max_degree_geo = bg::get<1, 1>(box);
    
    BOOST_CHECK_CLOSE(p_max_degree_geo, expected_max, error);
    BOOST_CHECK_CLOSE(p_min_degree_geo, expected_min, error);
    
    bg::strategy::envelope::spherical_segment<CT> strategy_sph;

    strategy_sph.apply(p1, p2, box);

    CT p_min_degree_sph = bg::get<0, 1>(box);
    CT p_max_degree_sph = bg::get<1, 1>(box);

    BOOST_CHECK_CLOSE(p_max_degree_sph, expected_max_sph, error);
    BOOST_CHECK_CLOSE(p_min_degree_sph, expected_min_sph, error);
}


template <typename CT>
void test_all()
{
    typedef bg::model::point<CT, 2, bg::cs::geographic<bg::degree> > Pg;

    // Short segments
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 1), Pg(10, 5), 5.0, 1.0, 5.0, 1.0);

    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 1), Pg(10, 1), 1.0031124506594733, 1.0, 1.0030915676477881, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(-5, 1), Pg(4, 1), 1.0031124506594733, 1.0, 1.0030915676477881, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(175, 1), Pg(184, 1), 1.0031124506594733, 1.0, 1.0030915676477881, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(355, 1), Pg(4, 1), 1.0031124506594733, 1.0, 1.0030915676477881, 1.0);

    // Reverse direction
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 2), Pg(70, 1), 2.0239716998355468, 1.0, 2.0228167431951536, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(70, 1), Pg(1, 2), 2.0239716998351849, 1.0, 2.022816743195063, 1.0);

    // Long segments
    test_strategies_lat<bg::strategy::thomas>
            (Pg(0, 1), Pg(170, 1), 11.975026023950877, 1.0, 11.325049479775814, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(0, 1), Pg(179, 1), 68.452669316418039, 1.0, 63.437566893227093, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(0, 1), Pg(179.5, 1), 78.84050225214871, 1.0, 75.96516822754981, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(0, 1), Pg(180.5, 1), 78.84050225214871, 1.0, 75.965168227550194, 1.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(0, 1), Pg(180, 1), 90.0, 1.0, 90.0, 1.0);

    // South hemisphere
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, -1), Pg(10, -5), -1.0, -5.0, -1.0, -5.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, -1), Pg(10, -1), -1.0, -1.0031124506594733, -1.0, -1.0030915676477881);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, -1), Pg(170, -1), -1.0, -10.85834257048573, -1.0, -10.321374780571153);

    // Different strategies for inverse
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 1), Pg(10, 1), 1.0031124506594733, 1.0,
             1.0030915676477881, 1.0, 0.00000001);
    test_strategies_lat<bg::strategy::andoyer>
            (Pg(1, 1), Pg(10, 1), 1.0031124504591062, 1.0,
             1.0030915676477881, 1.0, 0.00000001);
    test_strategies_lat<bg::strategy::vincenty>
            (Pg(1, 1), Pg(10, 1), 1.0031124508942098, 1.0,
             1.0030915676477881, 1.0, 0.00000001);

    // Meridian and equator
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 10), Pg(1, -10), 10.0, -10.0, 10.0, -10.0);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 0), Pg(10, 0), 0.0, 0.0, 0.0, 0.0);

    // One endpoint in northern hemisphere and the other in southern hemisphere
    test_strategies_lat<bg::strategy::thomas>
            (Pg(1, 1), Pg(150, -5), 1.0, -8.1825389632359933, 1.0, -8.0761230625567588);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(150, -5), Pg(1, 1), 1.0, -8.1825389632359933, 1.0, -8.0761230625568015);
    test_strategies_lat<bg::strategy::thomas>
            (Pg(150, 5), Pg(1, -1), 8.1825389632359933, -1.0, 8.0761230625568015, -1.0);
}



int test_main( int , char* [] )
{
    test_all<double>();

    return 0;
}

