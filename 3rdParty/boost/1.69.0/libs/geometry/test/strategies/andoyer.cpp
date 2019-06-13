// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2016 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2016 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2016 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014-2017.
// Modifications copyright (c) 2014-2017 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/concept_check.hpp>

#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/srs/spheroid.hpp>
#include <boost/geometry/strategies/concepts/distance_concept.hpp>
#include <boost/geometry/strategies/geographic/distance_andoyer.hpp>
#include <boost/geometry/strategies/geographic/side_andoyer.hpp>

#include <test_common/test_point.hpp>

#ifdef HAVE_TTMATH
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif



double make_deg(double deg, double min, double sec)
{
    return deg + min / 60.0 + sec / 3600.0;
}

double to_rad(double deg)
{
    return bg::math::pi<double>() * deg / 180.0;
}

double to_deg(double rad)
{
    return 180.0 * rad / bg::math::pi<double>();
}

double normlized_deg(double deg)
{
    if (deg > 180)
        return deg - 360;
    else if (deg < -180)
        return deg + 360;
    else
        return deg;
}


template <typename P1, typename P2>
void test_distance(double lon1, double lat1, double lon2, double lat2, double expected_km)
{
    // Set radius type, but for integer coordinates we want to have floating point radius type
    typedef typename bg::promote_floating_point
        <
            typename bg::coordinate_type<P1>::type
        >::type rtype;

    typedef bg::srs::spheroid<rtype> stype;

    typedef bg::strategy::distance::andoyer<stype> andoyer_type;
    typedef bg::strategy::distance::geographic<bg::strategy::andoyer, stype> geographic_type;
    typedef bg::formula::andoyer_inverse<rtype, true, false> andoyer_inverse_type;

    BOOST_CONCEPT_ASSERT
        ( 
            (bg::concepts::PointDistanceStrategy<andoyer_type, P1, P2>) 
        );

    andoyer_type andoyer;
    geographic_type geographic;
    typedef typename bg::strategy::distance
        ::services::return_type<andoyer_type, P1, P2>::type return_type;

    P1 p1;
    P2 p2;

    bg::assign_values(p1, lon1, lat1);
    bg::assign_values(p2, lon2, lat2);

    return_type d_strategy = andoyer.apply(p1, p2);
    return_type d_strategy2 = geographic.apply(p1, p2);
    return_type d_function = bg::distance(p1, p2, andoyer);

    double diff = bg::math::longitude_distance_signed<bg::degree>(lon1, lon2);
    return_type d_formula;

    // if the points lay on a meridian, distance strategy calls the special formula
    // for meridian distance that returns different result than andoyer formula
    // for nearly antipodal points
    if (bg::math::equals(diff, 0.0)
       || bg::math::equals(bg::math::abs(diff), 180.0))
    {
        d_formula = d_strategy;
    }
    else
    {
        d_formula = andoyer_inverse_type::apply(to_rad(lon1), to_rad(lat1),
                                                to_rad(lon2), to_rad(lat2),
                                                stype()).distance;
    }

    BOOST_CHECK_CLOSE(d_strategy / 1000.0, expected_km, 0.001);
    BOOST_CHECK_CLOSE(d_strategy2 / 1000.0, expected_km, 0.001);
    BOOST_CHECK_CLOSE(d_function / 1000.0, expected_km, 0.001);
    BOOST_CHECK_CLOSE(d_formula / 1000.0, expected_km, 0.001);
}

template <typename PS, typename P>
void test_azimuth(double lon1, double lat1,
                  double lon2, double lat2,
                  double expected_azimuth_deg)
{
    // Set radius type, but for integer coordinates we want to have floating point radius type
    typedef typename bg::promote_floating_point
        <
            typename bg::coordinate_type<PS>::type
        >::type rtype;

    typedef bg::srs::spheroid<rtype> stype;
    typedef bg::formula::andoyer_inverse<rtype, false, true> andoyer_inverse_type;

    rtype a_formula = andoyer_inverse_type::apply(to_rad(lon1), to_rad(lat1), to_rad(lon2), to_rad(lat2), stype()).azimuth;

    rtype azimuth_deg = to_deg(a_formula);

    if (bg::math::equals(azimuth_deg, -180.0))
        azimuth_deg = 180.0;
    if (bg::math::equals(expected_azimuth_deg, -180.0))
        expected_azimuth_deg = 180.0;

    if (bg::math::equals(expected_azimuth_deg, 0.0))
    {
        BOOST_CHECK(bg::math::equals(azimuth_deg, expected_azimuth_deg));
    }
    else
    {
        BOOST_CHECK_CLOSE(azimuth_deg, expected_azimuth_deg, 0.001);
    }
}

template <typename P1, typename P2>
void test_distazi(double lon1, double lat1, double lon2, double lat2,
                  double expected_km, double expected_azimuth_deg)
{
    test_distance<P1, P2>(lon1, lat1, lon2, lat2, expected_km);
    test_azimuth<P1, P2>(lon1, lat1, lon2, lat2, expected_azimuth_deg);
}

// requires SW->NE
template <typename P1, typename P2>
void test_distazi_symm(double lon1, double lat1, double lon2, double lat2,
                       double expected_km, double expected_azimuth_deg,
                       bool is_antipodal = false)
{
    double d180 = is_antipodal ? 0 : 180;
    test_distazi<P1, P2>(lon1, lat1, lon2, lat2, expected_km, expected_azimuth_deg);
    test_distazi<P1, P2>(-lon1, lat1, -lon2, lat2, expected_km, -expected_azimuth_deg);
    test_distazi<P1, P2>(lon1, -lat1, lon2, -lat2, expected_km, d180 - expected_azimuth_deg);
    test_distazi<P1, P2>(-lon1, -lat1, -lon2, -lat2, expected_km, -d180 + expected_azimuth_deg);
}

template <typename P1, typename P2>
void test_distazi_symmNS(double lon1, double lat1, double lon2, double lat2,
                         double expected_km, double expected_azimuth_deg)
{
    test_distazi<P1, P2>(lon1, lat1, lon2, lat2, expected_km, expected_azimuth_deg);
    test_distazi<P1, P2>(lon1, -lat1, lon2, -lat2, expected_km, 180 - expected_azimuth_deg);
}


template <typename PS, typename P>
void test_side(double lon1, double lat1,
               double lon2, double lat2,
               double lon, double lat,
               int expected_side)
{
    // Set radius type, but for integer coordinates we want to have floating point radius type
    typedef typename bg::promote_floating_point
        <
            typename bg::coordinate_type<PS>::type
        >::type rtype;

    typedef bg::srs::spheroid<rtype> stype;

    typedef bg::strategy::side::andoyer<stype> strategy_type;
    typedef bg::strategy::side::geographic<bg::strategy::andoyer, stype> strategy2_type;

    strategy_type strategy;
    strategy2_type strategy2;

    PS p1, p2;
    P p;

    bg::assign_values(p1, lon1, lat1);
    bg::assign_values(p2, lon2, lat2);
    bg::assign_values(p, lon, lat);

    int side = strategy.apply(p1, p2, p);
    int side2 = strategy2.apply(p1, p2, p);

    BOOST_CHECK_EQUAL(side, expected_side);
    BOOST_CHECK_EQUAL(side2, expected_side);
}

template <typename P1, typename P2>
void test_all()
{
    // polar
    test_distazi<P1, P2>(0, 90, 1, 80,
                         1116.814237, 179);
    
    // no point difference
    test_distazi<P1, P2>(4, 52, 4, 52,
                         0.0, 0.0);

    // normal cases
    test_distazi<P1, P2>(4, 52, 3, 40,
                         1336.039890, -176.3086);
    test_distazi<P1, P2>(3, 52, 4, 40,
                         1336.039890, 176.3086);
    test_distazi<P1, P2>(make_deg(17, 19, 43.28),
                         make_deg(40, 30, 31.151),
                         18, 40,
                         80.323245,
                         make_deg(134, 27, 50.05));

    // antipodal
    // ok? in those cases shorter path would pass through a pole
    // but 90 or -90 would be consistent with distance?
    test_distazi<P1, P2>(0, 0,  180, 0, 20003.9, 0.0);
    test_distazi<P1, P2>(0, 0, -180, 0, 20003.9, 0.0);
    test_distazi<P1, P2>(-90, 0, 90, 0, 20003.9, 0.0);
    test_distazi<P1, P2>(90, 0, -90, 0, 20003.9, 0.0);

    // 0, 45, 90 ...
    for (int i = 0 ; i < 360 ; i += 45)
    {
        // 0 45 90 ...
        double l = normlized_deg(i);
        // -1 44 89 ...
        double l1 = normlized_deg(i - 1);
        // 1 46 91 ...
        double l2 = normlized_deg(i + 1);

        // near equator
        test_distazi_symm<P1, P2>(l1, -1, l2, 1, 313.7956, 45.1964);

        // near poles
        test_distazi_symmNS<P1, P2>(l, -89.5, l, 89.5, 19892.2, 0.0);
        test_distazi_symmNS<P1, P2>(l, -89.6, l, 89.6, 19914.6, 0.0);
        test_distazi_symmNS<P1, P2>(l, -89.7, l, 89.7, 19936.9, 0.0);
        test_distazi_symmNS<P1, P2>(l, -89.8, l, 89.8, 19959.2, 0.0);
        test_distazi_symmNS<P1, P2>(l, -89.9, l, 89.9, 19981.6, 0.0);
        test_distazi_symmNS<P1, P2>(l, -89.99, l, 89.99, 20001.7, 0.0);
        test_distazi_symmNS<P1, P2>(l, -89.999, l, 89.999, 20003.7, 0.0);
        // antipodal
        test_distazi_symmNS<P1, P2>(l, -90, l, 90, 20003.9, 0.0);

        test_distazi_symm<P1, P2>(normlized_deg(l-10.0), -10.0, normlized_deg(l+135), 45, 14892.1, 34.1802);
        test_distazi_symm<P1, P2>(normlized_deg(l-30.0), -30.0, normlized_deg(l+135), 45, 17890.7, 33.7002);
        test_distazi_symm<P1, P2>(normlized_deg(l-40.0), -40.0, normlized_deg(l+135), 45, 19319.7, 33.4801);
        test_distazi_symm<P1, P2>(normlized_deg(l-41.0), -41.0, normlized_deg(l+135), 45, 19459.1, 33.2408);
        test_distazi_symm<P1, P2>(normlized_deg(l-42.0), -42.0, normlized_deg(l+135), 45, 19597.8, 32.7844);
        test_distazi_symm<P1, P2>(normlized_deg(l-43.0), -43.0, normlized_deg(l+135), 45, 19735.8, 31.7784);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.0), -44.0, normlized_deg(l+135), 45, 19873.0, 28.5588);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.1), -44.1, normlized_deg(l+135), 45, 19886.7, 27.8304);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.2), -44.2, normlized_deg(l+135), 45, 19900.4, 26.9173);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.3), -44.3, normlized_deg(l+135), 45, 19914.1, 25.7401);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.4), -44.4, normlized_deg(l+135), 45, 19927.7, 24.1668);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.5), -44.5, normlized_deg(l+135), 45, 19941.4, 21.9599);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.6), -44.6, normlized_deg(l+135), 45, 19955.0, 18.6438);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.7), -44.7, normlized_deg(l+135), 45, 19968.6, 13.1096);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.8), -44.8, normlized_deg(l+135), 45, 19982.3, 2.0300);
        // nearly antipodal
        test_distazi_symm<P1, P2>(normlized_deg(l-44.9), -44.9, normlized_deg(l+135), 45, 19995.9, 0.0);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.95), -44.95, normlized_deg(l+135), 45, 20002.7, 0.0);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.99), -44.99, normlized_deg(l+135), 45, 20008.1, 0.0);
        test_distazi_symm<P1, P2>(normlized_deg(l-44.999), -44.999, normlized_deg(l+135), 45, 20009.4, 0.0);
        // antipodal
        test_distazi_symm<P1, P2>(normlized_deg(l-45), -45, normlized_deg(l+135), 45, 20003.92, 0.0, true);
    }

    /* SQL Server gives:
        1116.82586908528, 0, 1336.02721932545

       with:
SELECT 0.001 * geography::STGeomFromText('POINT(0 90)', 4326).STDistance(geography::STGeomFromText('POINT(1 80)', 4326))
union SELECT 0.001 * geography::STGeomFromText('POINT(4 52)', 4326).STDistance(geography::STGeomFromText('POINT(4 52)', 4326))
union SELECT 0.001 * geography::STGeomFromText('POINT(4 52)', 4326).STDistance(geography::STGeomFromText('POINT(3 40)', 4326))
     */


    test_side<P1, P2>(0, 0, 0, 1, 0, 2, 0);
    test_side<P1, P2>(0, 0, 0, 1, 0, -2, 0);
    test_side<P1, P2>(10, 0, 10, 1, 10, 2, 0);
    test_side<P1, P2>(10, 0, 10, -1, 10, 2, 0);

    test_side<P1, P2>(10, 0, 10, 1, 0, 2, 1); // left
    test_side<P1, P2>(10, 0, 10, -1, 0, 2, -1); // right

    test_side<P1, P2>(-10, -10, 10, 10, 10, 0, -1); // right
    test_side<P1, P2>(-10, -10, 10, 10, -10, 0, 1); // left
    test_side<P1, P2>(170, -10, -170, 10, -170, 0, -1); // right
    test_side<P1, P2>(170, -10, -170, 10, 170, 0, 1); // left
}

template <typename P>
void test_all()
{
    test_all<P, P>();
}

int test_main(int, char* [])
{
    //test_all<float[2]>();
    //test_all<double[2]>();
    //test_all<bg::model::point<int, 2, bg::cs::geographic<bg::degree> > >();
    //test_all<bg::model::point<float, 2, bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::point<ttmath::Big<1,4>, 2, bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::point<ttmath_big, 2, bg::cs::geographic<bg::degree> > >();
#endif

    return 0;
}
