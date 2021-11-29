// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014, 2015, 2016, 2017.
// Modifications copyright (c) 2014-2017 Oracle and/or its affiliates.

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
#include <boost/geometry/formulas/vincenty_inverse.hpp>
#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/srs/spheroid.hpp>
#include <boost/geometry/strategies/concepts/distance_concept.hpp>
#include <boost/geometry/strategies/geographic/distance_vincenty.hpp>
#include <boost/geometry/strategies/geographic/side_vincenty.hpp>

#include <test_common/test_point.hpp>

template <typename T>
void normalize_deg(T & deg)
{
    while ( deg > T(180) )
        deg -= T(360);
    while ( deg <= T(-180) )
        deg += T(360);
}

template <typename T>
T difference_deg(T const& a1, T const& a2)
{
    T d = a1 - a2;
    normalize_deg(d);
    return d;
}

template <typename T>
void check_deg(std::string const& name, T const& a1, T const& a2, T const& percent, T const& error)
{
    T diff = bg::math::abs(difference_deg(a1, a2));
    
    if ( bg::math::equals(a1, T(0)) || bg::math::equals(a2, T(0)) )
    {
        if ( diff > error )
        {
            BOOST_ERROR(name << " - the difference {" << diff << "} between {" << a1 << "} and {" << a2 << "} exceeds {" << error << "}");
        }
    }
    else
    {
        T greater = (std::max)(bg::math::abs(a1), bg::math::abs(a2));

        if ( diff > greater * percent / T(100) )
        {
            BOOST_ERROR(name << " the difference {" << diff << "} between {" << a1 << "} and {" << a2 << "} exceeds {" << percent << "}%");
        }
    }
}

double azimuth(double deg, double min, double sec)
{
    min = fabs(min);
    sec = fabs(sec);

    if ( deg < 0 )
    {
        min = -min;
        sec = -sec;
    }

    return deg + min/60.0 + sec/3600.0;
}

double azimuth(double deg, double min)
{
    return azimuth(deg, min, 0.0);
}

template <typename P>
bool non_precise_ct()
{
    typedef typename bg::coordinate_type<P>::type ct;
    return boost::is_integral<ct>::value || boost::is_float<ct>::value;
}

template <typename P1, typename P2, typename Spheroid>
void test_vincenty(double lon1, double lat1, double lon2, double lat2,
                   double expected_distance,
                   double expected_azimuth_12,
                   double /*expected_azimuth_21*/,
                   Spheroid const& spheroid)
{
    typedef typename bg::promote_floating_point
        <
            typename bg::select_calculation_type<P1, P2, void>::type
        >::type calc_t;

    calc_t tolerance = non_precise_ct<P1>() || non_precise_ct<P2>() ?
                       5.0 : 0.001;
    calc_t error = non_precise_ct<P1>() || non_precise_ct<P2>() ?
                   1e-5 : 1e-12;

    // formula
    {
        double const d2r = bg::math::d2r<double>();
        double const r2d = bg::math::r2d<double>();

        typedef bg::formula::vincenty_inverse<calc_t, true, true> inverse_formula;
        typename inverse_formula::result_type
            result_i = inverse_formula::apply(lon1 * d2r,
                                             lat1 * d2r,
                                             lon2 * d2r,
                                             lat2 * d2r,
                                             spheroid);
        calc_t dist = result_i.distance;
        calc_t az12 = result_i.azimuth;
        //calc_t az21 = vi.azimuth21();

        calc_t az12_deg = az12 * r2d;
        //calc_t az21_deg = az21 * r2d;
        
        BOOST_CHECK_CLOSE(dist, calc_t(expected_distance), tolerance);
        check_deg("az12_deg", az12_deg, calc_t(expected_azimuth_12), tolerance, error);
        //check_deg("az21_deg", az21_deg, calc_t(expected_azimuth_21), tolerance, error);

        typedef bg::formula::vincenty_direct<calc_t> direct_formula;
        typename direct_formula::result_type
            result_d = direct_formula::apply(lon1 * d2r,
                                             lat1 * d2r,
                                             dist,
                                             az12,
                                             spheroid);
        calc_t direct_lon2 = result_d.lon2;
        calc_t direct_lat2 = result_d.lat2;
        //calc_t direct_az21 = vd.azimuth21();

        calc_t direct_lon2_deg = direct_lon2 * r2d;
        calc_t direct_lat2_deg = direct_lat2 * r2d;
        //calc_t direct_az21_deg = direct_az21 * r2d;
        
        check_deg("direct_lon2_deg", direct_lon2_deg, calc_t(lon2), tolerance, error);
        check_deg("direct_lat2_deg", direct_lat2_deg, calc_t(lat2), tolerance, error);
        //check_deg("direct_az21_deg", direct_az21_deg, az21_deg, tolerance, error);
    }

    // distance strategies
    {
        typedef bg::strategy::distance::vincenty<Spheroid> vincenty_type;
        typedef bg::strategy::distance::geographic<bg::strategy::vincenty, Spheroid> geographic_type;

        BOOST_CONCEPT_ASSERT(
            (
                bg::concepts::PointDistanceStrategy<vincenty_type, P1, P2>)
            );

        vincenty_type vincenty(spheroid);
        geographic_type geographic(spheroid);
        typedef typename bg::strategy::distance::services::return_type<vincenty_type, P1, P2>::type return_type;

        P1 p1;
        P2 p2;

        bg::assign_values(p1, lon1, lat1);
        bg::assign_values(p2, lon2, lat2);
        
        BOOST_CHECK_CLOSE(vincenty.apply(p1, p2), return_type(expected_distance), tolerance);
        BOOST_CHECK_CLOSE(geographic.apply(p1, p2), return_type(expected_distance), tolerance);
        BOOST_CHECK_CLOSE(bg::distance(p1, p2, vincenty), return_type(expected_distance), tolerance);
    }
}

template <typename P1, typename P2>
void test_vincenty(double lon1, double lat1, double lon2, double lat2,
                   double expected_distance,
                   double expected_azimuth_12,
                   double expected_azimuth_21)
{
    test_vincenty<P1, P2>(lon1, lat1, lon2, lat2,
                          expected_distance, expected_azimuth_12, expected_azimuth_21,
                          bg::srs::spheroid<double>());
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

    typedef bg::strategy::side::vincenty<stype> strategy_type;
    typedef bg::strategy::side::geographic<bg::strategy::vincenty, stype> strategy2_type;

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
    // See:
    //  - http://www.ga.gov.au/geodesy/datums/vincenty_inverse.jsp
    //  - http://www.ga.gov.au/geodesy/datums/vincenty_direct.jsp
    // Values in the comments below was calculated using the above pages
    // in some cases distances may be different, previously used values was left

    // use km
    double gda_a = 6378.1370;
    double gda_f = 1.0 / 298.25722210;
    double gda_b = gda_a * ( 1.0 - gda_f );
    bg::srs::spheroid<double> gda_spheroid(gda_a, gda_b);

    // Test fractional coordinates only for non-integral types
    if ( BOOST_GEOMETRY_CONDITION(
            ! boost::is_integral<typename bg::coordinate_type<P1>::type>::value
         && ! boost::is_integral<typename bg::coordinate_type<P2>::type>::value  ) )
    {
        // Flinders Peak -> Buninyong
        test_vincenty<P1, P2>(azimuth(144,25,29.52440), azimuth(-37,57,3.72030),
                              azimuth(143,55,35.38390), azimuth(-37,39,10.15610),
                              54.972271, azimuth(306,52,5.37), azimuth(127,10,25.07),
                              gda_spheroid);
    }

    // Lodz -> Trondheim
    test_vincenty<P1, P2>(azimuth(19,28), azimuth(51,47),
                          azimuth(10,21), azimuth(63,23),
                          1399.032724, azimuth(340,54,25.14), azimuth(153,10,0.19),
                          gda_spheroid);
    // London -> New York
    test_vincenty<P1, P2>(azimuth(0,7,39), azimuth(51,30,26),
                          azimuth(-74,0,21), azimuth(40,42,46),
                          5602.044851, azimuth(288,31,36.82), azimuth(51,10,33.43),
                          gda_spheroid);

    // Shanghai -> San Francisco
    test_vincenty<P1, P2>(azimuth(121,30), azimuth(31,12),
                          azimuth(-122,25), azimuth(37,47),
                          9899.698550, azimuth(45,12,44.76), azimuth(309,50,20.88),
                          gda_spheroid);

    test_vincenty<P1, P2>(0, 0, 0, 50, 5540.847042, 0, 180, gda_spheroid); // N
    test_vincenty<P1, P2>(0, 0, 0, -50, 5540.847042, 180, 0, gda_spheroid); // S
    test_vincenty<P1, P2>(0, 0, 50, 0, 5565.974540, 90, -90, gda_spheroid); // E
    test_vincenty<P1, P2>(0, 0, -50, 0, 5565.974540, -90, 90, gda_spheroid); // W
    
    test_vincenty<P1, P2>(0, 0, 50, 50, 7284.879297, azimuth(32,51,55.87), azimuth(237,24,50.12), gda_spheroid); // NE
    
    // The original distance values, azimuths calculated using the web form mentioned above
    // Using default spheroid units (meters)
    test_vincenty<P1, P2>(0, 89, 1, 80, 1005153.5769, azimuth(178,53,23.85), azimuth(359,53,18.35)); // sub-polar
    test_vincenty<P1, P2>(4, 52, 4, 52, 0.0, 0, 0); // no point difference
    test_vincenty<P1, P2>(4, 52, 3, 40, 1336039.890, azimuth(183,41,29.08), azimuth(2,58,5.13)); // normal case

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
    test_all<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::point<float, 2, bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::point<int, 2, bg::cs::geographic<bg::degree> > >();

    return 0;
}
