// Boost.Geometry

// Copyright (c) 2017 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/concept_check.hpp>

#include <boost/geometry/core/srs.hpp>
#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include <test_common/test_point.hpp>

#ifdef HAVE_TTMATH
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif

typedef bg::srs::spheroid<double> stype;

typedef bg::strategy::andoyer andoyer_formula;
typedef bg::strategy::thomas thomas_formula;
typedef bg::strategy::vincenty vincenty_formula;

template <typename P>
bool non_precise_ct()
{
    typedef typename bg::coordinate_type<P>::type ct;
    return boost::is_integral<ct>::value || boost::is_float<ct>::value;
}

template <typename P1, typename P2, typename FormulaPolicy>
void test_distance(double lon1, double lat1, double lon2, double lat2)
{
    typedef typename bg::promote_floating_point
        <
            typename bg::select_calculation_type<P1, P2, void>::type
        >::type calc_t;

    calc_t tolerance = non_precise_ct<P1>() || non_precise_ct<P2>() ?
                       5.0 : 0.001;

    P1 p1;
    P2 p2;

    bg::assign_values(p1, lon1, lat1);
    bg::assign_values(p2, lon2, lat2);

    // Test strategy that implements meridian distance against formula
    // that implements general distance
    // That may change in the future but in any case these calls must return
    // the same result

    calc_t dist_formula = FormulaPolicy::template inverse
            <
                double, true, false, false, false, false
            >::apply(lon1 * bg::math::d2r<double>(),
                     lat1 * bg::math::d2r<double>(),
                     lon2 * bg::math::d2r<double>(),
                     lat2 * bg::math::d2r<double>(),
                     stype()).distance;

    bg::strategy::distance::geographic<FormulaPolicy, stype> strategy;
    calc_t dist_strategy = strategy.apply(p1, p2);
    BOOST_CHECK_CLOSE(dist_formula, dist_strategy, tolerance);    
}

template <typename P1, typename P2, typename FormulaPolicy>
void test_distance_reverse(double lon1, double lat1,
                           double lon2, double lat2)
{
    test_distance<P1, P2, FormulaPolicy>(lon1, lat1, lon2, lat2);
    test_distance<P1, P2, FormulaPolicy>(lon2, lat2, lon1, lat1);
}

template <typename P1, typename P2, typename FormulaPolicy>
void test_meridian()
{
    test_distance_reverse<P1, P2, FormulaPolicy>(0., 70., 0., 80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(0, 70, 0., -80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(0., -70., 0., 80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(0., -70., 0., -80.);

    test_distance_reverse<P1, P2, FormulaPolicy>(0., 70., 180., 80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(0., 70., 180., -80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(0., -70., 180., 80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(0., -70., 180., -80.);

    test_distance_reverse<P1, P2, FormulaPolicy>(350., 70., 170., 80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(350., 70., 170., -80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(350., -70., 170., 80.);
    test_distance_reverse<P1, P2, FormulaPolicy>(350., -70., 170., -80.);
}

template <typename P>
void test_all()
{
    test_meridian<P, P, andoyer_formula>();
    test_meridian<P, P, thomas_formula>();
    test_meridian<P, P, vincenty_formula>();
}

int test_main(int, char* [])
{
    test_all<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::point<float, 2, bg::cs::geographic<bg::degree> > >();
    test_all<bg::model::point<int, 2, bg::cs::geographic<bg::degree> > >();

    return 0;
}
