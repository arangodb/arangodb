// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <sstream>

#include <boost/variant/variant.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/transform.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include <test_common/test_point.hpp>

template <typename Geometry1, typename Geometry2>
void check_transform(Geometry1 const& geometry1,
                     Geometry2 const& expected)
{
    Geometry2 geometry2;
    BOOST_CHECK(bg::transform(geometry1, geometry2));

    std::ostringstream result_wkt, expected_wkt;
    result_wkt << bg::wkt(geometry2);
    expected_wkt << bg::wkt(expected);
    BOOST_CHECK_EQUAL(result_wkt.str(), expected_wkt.str());
}

template <typename P1, typename P2, typename Value>
void test_transform_point(Value value)
{
    P1 p1;
    bg::set<0>(p1, 1);
    bg::set<1>(p1, 2);
    boost::variant<P1> v(p1);

    P2 expected;
    bg::assign(expected, p1);
    bg::multiply_value(expected, value);

    check_transform(p1, expected);
    check_transform(v, expected);
}

template <typename P1, typename P2, typename Value>
void test_transform_linestring(Value value)
{
    typedef bg::model::linestring<P1> line1_type;
    typedef bg::model::linestring<P2> line2_type;

    line1_type line1;
    line1.push_back(bg::make<P1>(1, 1));
    line1.push_back(bg::make<P1>(2, 2));
    boost::variant<line1_type> v(line1);

    line2_type expected;
    for (auto it = line1.begin(); it != line1.end(); ++it)
    {
        P2 new_point;
        bg::assign(new_point, *it);
        bg::multiply_value(new_point, value);
        expected.push_back(new_point);
    }

    check_transform(line1, expected);
    check_transform(v, expected);
}


template <typename P1, typename P2, typename Value>
void test_all(Value value)
{
    test_transform_point<P1, P2>(value);
    test_transform_linestring<P1, P2>(value);
}

template <typename T, typename DegreeOrRadian>
void test_transformations(double phi, double theta, double r)
{
    typedef bg::model::point<T, 3, bg::cs::cartesian> cartesian_type;
    cartesian_type p;

    // 1: using spherical coordinates
    {
        typedef bg::model::point<T, 3, bg::cs::spherical<DegreeOrRadian> >  spherical_type;
        spherical_type sph1;
        assign_values(sph1, phi, theta, r);
        BOOST_CHECK(transform(sph1, p));

        spherical_type sph2;
        BOOST_CHECK(transform(p, sph2));

        BOOST_CHECK_CLOSE(bg::get<0>(sph1), bg::get<0>(sph2), 0.001);
        BOOST_CHECK_CLOSE(bg::get<1>(sph1), bg::get<1>(sph2), 0.001);
    }

    // 2: using spherical coordinates on unit sphere
    {
        typedef bg::model::point<T, 2, bg::cs::spherical<DegreeOrRadian> >  spherical_type;
        spherical_type sph1, sph2;
        assign_values(sph1, phi, theta);
        BOOST_CHECK(transform(sph1, p));
        BOOST_CHECK(transform(p, sph2));

        BOOST_CHECK_CLOSE(bg::get<0>(sph1), bg::get<0>(sph2), 0.001);
        BOOST_CHECK_CLOSE(bg::get<1>(sph1), bg::get<1>(sph2), 0.001);
    }
}

int test_main(int, char* [])
{
    typedef bg::model::d2::point_xy<double > P;
    test_all<P, P>(1.0);
    test_all<bg::model::d2::point_xy<int>, bg::model::d2::point_xy<float> >(1.0);

    test_all<bg::model::point<double, 2, bg::cs::spherical<bg::degree> >,
        bg::model::point<double, 2, bg::cs::spherical<bg::radian> > >(bg::math::d2r<double>());
    test_all<bg::model::point<double, 2, bg::cs::spherical<bg::radian> >,
        bg::model::point<double, 2, bg::cs::spherical<bg::degree> > >(bg::math::r2d<double>());

    test_all<bg::model::point<int, 2, bg::cs::spherical<bg::degree> >,
        bg::model::point<float, 2, bg::cs::spherical<bg::radian> > >(bg::math::d2r<float>());

    test_transformations<float, bg::degree>(4, 52, 1);
    test_transformations<double, bg::degree>(4, 52, 1);

    test_transformations
        <
            float, bg::radian
        >(3 * bg::math::d2r<float>(), 51 * bg::math::d2r<float>(), 1);

    test_transformations
        <
            double, bg::radian
        >(3 * bg::math::d2r<double>(), 51 * bg::math::d2r<double>(), 1);

    return 0;
}
