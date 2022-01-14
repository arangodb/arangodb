// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>

#include <boost/geometry/geometries/concepts/segment_concept.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/segment.hpp>

#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>


#include <test_common/test_point.hpp>
#include <test_geometries/custom_segment.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)


template <typename P>
void test_all()
{
    typedef bg::model::referring_segment<P> S;

    P p1;
    P p2;
    S s(p1, p2);
    BOOST_CHECK_EQUAL(&s.first, &p1);
    BOOST_CHECK_EQUAL(&s.second, &p2);

    // Compilation tests, all things should compile.
    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstSegment<S>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::Segment<S>) );

    typedef typename bg::coordinate_type<S>::type T;
    typedef typename bg::point_type<S>::type SP;
    boost::ignore_unused<T, SP>();

    //std::cout << sizeof(typename coordinate_type<S>::type) << std::endl;

    typedef bg::model::referring_segment<P const> refseg_t;
    //BOOST_CONCEPT_ASSERT( (concepts::ConstSegment<refseg_t>) );

    refseg_t seg(p1, p2);

    typedef typename bg::coordinate_type<refseg_t>::type CT;
    typedef typename bg::point_type<refseg_t>::type CSP;
    boost::ignore_unused<CT, CSP>();
}



template <typename S>
void test_custom()
{
    S seg;
    bg::set<0,0>(seg, 1);
    bg::set<0,1>(seg, 2);
    bg::set<1,0>(seg, 3);
    bg::set<1,1>(seg, 4);

    BOOST_CHECK_EQUAL((bg::get<0, 0>(seg)), 1);
    BOOST_CHECK_EQUAL((bg::get<0, 1>(seg)), 2);
    BOOST_CHECK_EQUAL((bg::get<1, 0>(seg)), 3);
    BOOST_CHECK_EQUAL((bg::get<1, 1>(seg)), 4);
}


template <typename P>
void test_constexpr()
{
    typedef bg::model::segment<P> S;
    constexpr S s = S{ {1, 2, 3}, {4, 5} };
    constexpr auto c1 = bg::get<0, 0>(s);
    constexpr auto c2 = bg::get<1, 2>(s);
    BOOST_CHECK_EQUAL(c1, 1);
    BOOST_CHECK_EQUAL(c2, 0);
    BOOST_CHECK_EQUAL((bg::get<0, 0>(s)), 1);
    BOOST_CHECK_EQUAL((bg::get<0, 1>(s)), 2);
    BOOST_CHECK_EQUAL((bg::get<0, 2>(s)), 3);
    BOOST_CHECK_EQUAL((bg::get<1, 0>(s)), 4);
    BOOST_CHECK_EQUAL((bg::get<1, 1>(s)), 5);
    BOOST_CHECK_EQUAL((bg::get<1, 2>(s)), 0);
}


int test_main(int, char* [])
{
    test_all<int[3]>();
    test_all<float[3]>();
    test_all<double[3]>();
    //test_all<test_point>();
    test_all<bg::model::point<int, 3, bg::cs::cartesian> >();
    test_all<bg::model::point<float, 3, bg::cs::cartesian> >();
    test_all<bg::model::point<double, 3, bg::cs::cartesian> >();

    test_custom<test::custom_segment>();
    test_custom<test::custom_segment_of<bg::model::point<double, 2, bg::cs::cartesian> > >();
    test_custom<test::custom_segment_of<test::custom_point_for_segment> >();
    test_custom<test::custom_segment_4>();

    test_constexpr<bg::model::point<double, 3, bg::cs::cartesian> >();

    return 0;
}

