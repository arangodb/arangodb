// Boost.Geometry
// Unit Test

// Copyright (c) 2019-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/tupled_output.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/util/range.hpp>

namespace bgd = boost::geometry::detail;
namespace bgt = boost::geometry::tuples;
namespace bgr = boost::geometry::range;

template <typename MultiPoint>
void add_points(MultiPoint & mp)
{
    typedef typename bg::point_type<MultiPoint>::type point_type;

    bg::range::push_back(mp, point_type(1, 2));
    bg::range::push_back(mp, point_type(2, 3));
    bg::range::push_back(mp, point_type(3, 4));
}

template <typename TupleM, typename TupleS>
void test_range_values()
{
    typedef typename bgd::tupled_range_values<TupleM>::type tuple_s;
    BOOST_CHECK_EQUAL((std::is_same<tuple_s, TupleS>::value), true);
}

template <typename TupleM, typename TupleBI>
void test_back_inserters()
{
    typedef typename bgd::tupled_back_inserters<TupleM>::type tuple_bi;
    BOOST_CHECK_EQUAL((std::is_same<tuple_bi, TupleBI>::value), true);

    TupleM tup;
    bgd::tupled_back_inserters<TupleM>::apply(tup);
}

template <typename TuplePoLs, typename TupleLsMPt>
void test_all()
{
    BOOST_CHECK_EQUAL((bgd::is_tupled_output<TuplePoLs>::value), false);
    BOOST_CHECK_EQUAL((bgd::is_tupled_output<TupleLsMPt>::value), true);

    BOOST_CHECK_EQUAL((bgd::tupled_output_has<TuplePoLs, bg::multi_point_tag>::value), false);
    BOOST_CHECK_EQUAL((bgd::tupled_output_has<TupleLsMPt, bg::multi_point_tag>::value), true);

    BOOST_CHECK_EQUAL((bgd::tupled_output_has<TuplePoLs, bg::multi_polygon_tag>::value), false);
    BOOST_CHECK_EQUAL((bgd::tupled_output_has<TupleLsMPt, bg::multi_polygon_tag>::value), false);

    TupleLsMPt tup;
    add_points(bgd::tupled_output_get<bg::multi_point_tag>(tup));
    BOOST_CHECK_EQUAL((boost::size(bgt::get<1>(tup))), 3u);
}

int test_main(int, char* [])
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point;
    typedef bg::model::linestring<point> linestring;
    typedef bg::model::polygon<point> polygon;
    typedef bg::model::multi_point<point> multi_point;
    typedef bg::model::multi_linestring<linestring> multi_linestring;
    //typedef bg::model::multi_polygon<polygon> multi_polygon;

    BOOST_CHECK_EQUAL((bg::range::detail::is_range<int>::value), false);
    BOOST_CHECK_EQUAL((bg::range::detail::is_range<linestring>::value), true);
    BOOST_CHECK_EQUAL((bg::range::detail::is_range<multi_point>::value), true);
    
    BOOST_CHECK_EQUAL((bgd::is_tupled_output_element<int>::value), false);
    BOOST_CHECK_EQUAL((bgd::is_tupled_output_element<linestring>::value), false);
    BOOST_CHECK_EQUAL((bgd::is_tupled_output_element<multi_point>::value), true);

    test_all<boost::tuple<polygon, linestring>, boost::tuple<linestring, multi_point> >();
    test_all<std::pair<polygon, linestring>, std::pair<linestring, multi_point> >();

    test_range_values<boost::tuple<multi_linestring, multi_point>,
                      boost::tuples::cons<linestring,
                        boost::tuples::cons<point,
                            boost::tuples::null_type> > >();
    test_range_values<std::pair<multi_linestring, multi_point>,
                      std::pair<linestring, point> >();

    test_back_inserters<boost::tuple<multi_linestring, multi_point>,
                        boost::tuples::cons<bgr::back_insert_iterator<multi_linestring>,
                            boost::tuples::cons<bgr::back_insert_iterator<multi_point>,
                                boost::tuples::null_type> > >();
    test_back_inserters<std::pair<multi_linestring, multi_point>,
                        std::pair<bgr::back_insert_iterator<multi_linestring>,
                                  bgr::back_insert_iterator<multi_point> > >();

#if !defined(BOOST_NO_CXX11_HDR_TUPLE) && !defined(BOOST_NO_VARIADIC_TEMPLATES)

    test_all<std::tuple<polygon, linestring>, std::tuple<linestring, multi_point> >();

    test_range_values<std::tuple<multi_linestring, multi_point>,
                      std::tuple<linestring, point> >();

    test_back_inserters<std::tuple<multi_linestring, multi_point>,
                        std::tuple<bgr::back_insert_iterator<multi_linestring>,
                                   bgr::back_insert_iterator<multi_point> > >();

#endif
    
    return 0;
}
