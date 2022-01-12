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
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/concepts/multi_linestring_concept.hpp>
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
bg::model::linestring<P> create_linestring()
{   
    bg::model::linestring<P> l1;
    P p1(1, 2);
    bg::append(l1, p1);
    return l1;
}

template <typename P, typename L>
bg::model::multi_linestring<L> create_multi_linestring()
{   
    bg::model::multi_linestring<L> ml1;
    L l1(create_linestring<P>());
    ml1.push_back(l1);
    ml1.push_back(l1);
    return ml1;
}

template <typename ML, typename L>
void check_multi_linestring(ML& to_check, L l1)
{   
    ML cur;
    cur.push_back(l1);
    cur.push_back(l1);

    std::ostringstream out1, out2;
    out1 << bg::dsv(to_check);
    out2 << bg::dsv(cur);
    BOOST_CHECK_EQUAL(out1.str(), out2.str());
}

template <typename P, typename L>
void test_default_constructor()
{
    bg::model::multi_linestring<L> ml1(create_multi_linestring<P, L>());
    check_multi_linestring(ml1, L(create_linestring<P>()));
}

template <typename P, typename L>
void test_copy_constructor()
{
    bg::model::multi_linestring<L> ml1 = create_multi_linestring<P, L>();
    check_multi_linestring(ml1, L(create_linestring<P>()));
}

template <typename P, typename L>
void test_copy_assignment()
{
    bg::model::multi_linestring<L> ml1(create_multi_linestring<P, L>()), ml2;
    ml2 = ml1;
    check_multi_linestring(ml2, L(create_linestring<P>()));
}

template <typename L>
void test_concept()
{   
    typedef bg::model::multi_linestring<L> ML;

    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstMultiLinestring<ML>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::MultiLinestring<ML>) );

    typedef typename bg::coordinate_type<ML>::type T;
    typedef typename bg::point_type<ML>::type PML;
    boost::ignore_unused<T, PML>();
}

template <typename P>
void test_all()
{   
    typedef bg::model::linestring<P> L;

    test_default_constructor<P, L>();
    test_copy_constructor<P, L>();
    test_copy_assignment<P, L>();
    test_concept<L>();
}

template <typename P>
void test_custom_multi_linestring(bg::model::linestring<P> IL)
{   
    typedef bg::model::linestring<P> L;
    
    std::initializer_list<L> LIL = {IL};
    bg::model::multi_linestring<L> ml1(LIL);
    std::ostringstream out;
    out << bg::dsv(ml1);
    BOOST_CHECK_EQUAL(out.str(), "(((1, 1), (2, 2), (3, 3), (0, 0), (0, 2), (0, 3)))");
}

template <typename P>
void test_custom()
{   
#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
    std::initializer_list<P> IL1 = {P(1, 1), P(2, 2), P(3, 3)};
    std::initializer_list<P> IL2 = {P(0, 0), P(0, 2), P(0, 3)};
    bg::model::linestring<P> l1;
    bg::append(l1, IL1);
    bg::append(l1, IL2);
    test_custom_multi_linestring<P>(l1);
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
