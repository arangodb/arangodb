// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/append.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/concepts/multi_polygon_concept.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <test_common/test_point.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)

#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
#include <initializer_list>
#endif//BOOST_NO_CXX11_HDR_INITIALIZER_LIST

template <typename P>
bg::model::polygon<P> create_polygon()
{   
    bg::model::polygon<P> pl1;
    P p1;
    P p2;
    P p3;
    bg::assign_values(p1, 1, 2);
    bg::assign_values(p2, 2, 0);
    bg::assign_values(p3, 0, 0);
    
    bg::append(pl1, p1);
    bg::append(pl1, p2);
    bg::append(pl1, p3);
    bg::append(pl1, p1);
    return pl1;
}

template <typename P, typename PL>
bg::model::multi_polygon<PL> create_multi_polygon()
{
    bg::model::multi_polygon<PL> mpl1;
    PL pl1(create_polygon<P>());
    mpl1.push_back(pl1);
    mpl1.push_back(pl1);
    return mpl1;
}

template <typename MPL, typename PL>
void check_multi_polygon(MPL& to_check, PL pl1)
{   
    MPL cur;
    cur.push_back(pl1);
    cur.push_back(pl1);

    std::ostringstream out1, out2;
    out1 << bg::dsv(to_check);
    out2 << bg::dsv(cur);
    BOOST_CHECK_EQUAL(out1.str(), out2.str());
}

template <typename P, typename PL>
void test_default_constructor()
{
    bg::model::multi_polygon<PL> mpl1(create_multi_polygon<P, PL>());
    check_multi_polygon(mpl1, PL(create_polygon<P>()));
}

template <typename P, typename PL>
void test_copy_constructor()
{
    bg::model::multi_polygon<PL> mpl1 = create_multi_polygon<P, PL>();
    check_multi_polygon(mpl1, PL(create_polygon<P>()));
}

template <typename P, typename PL>
void test_copy_assignment()
{
    bg::model::multi_polygon<PL> mpl1(create_multi_polygon<P, PL>()), mpl2;
    mpl2 = mpl1;
    check_multi_polygon(mpl2, PL(create_polygon<P>()));
}

template <typename PL>
void test_concept()
{   
    typedef bg::model::multi_polygon<PL> MPL;

    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstMultiPolygon<MPL>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::MultiPolygon<MPL>) );

    typedef typename bg::coordinate_type<MPL>::type T;
    typedef typename bg::point_type<MPL>::type PMPL;
    boost::ignore_unused<T, PMPL>();
}

template <typename P>
void test_all()
{   
    typedef bg::model::polygon<P> PL;

    test_default_constructor<P, PL>();
    test_copy_constructor<P, PL>();
    test_copy_assignment<P, PL>();
    test_concept<PL>();
}

template <typename P>
void test_custom_multi_polygon(bg::model::polygon<P> IL)
{   
    typedef bg::model::polygon<P> PL;

    std::initializer_list<PL> PIL = {IL};
    bg::model::multi_polygon<PL> mpl1(PIL);
    std::ostringstream out;
    out << bg::dsv(mpl1);
    BOOST_CHECK_EQUAL(out.str(), "((((3, 3), (3, 0), (0, 0), (0, 3), (3, 3))))");
}

template <typename P>
void test_custom()
{   
#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
    std::initializer_list<P> IL = {P(3, 3), P(3, 0), P(0, 0), P(0, 3), P(3, 3)};
    bg::model::ring<P> r1(IL);
    std::initializer_list<bg::model::ring<P> > RIL = {r1};
    test_custom_multi_polygon<P>(RIL);
#endif//BOOST_NO_CXX11_HDR_INITIALIZER_LIST
}

template <typename CS>
void test_cs()
{
    test_all<bg::model::point<int, 2, CS> >();
    test_all<bg::model::point<float, 2, CS> >();
    test_all<bg::model::point<double, 2, CS> >();

    test_custom<bg::model::point<double, 2, CS> >();
}


int test_main(int, char* [])
{   
    test_cs<bg::cs::cartesian>();
    test_cs<bg::cs::spherical<bg::degree> >();
    test_cs<bg::cs::spherical_equatorial<bg::degree> >();
    test_cs<bg::cs::geographic<bg::degree> >();

    test_custom<bg::model::d2::point_xy<double> >();

    return 0;
}
