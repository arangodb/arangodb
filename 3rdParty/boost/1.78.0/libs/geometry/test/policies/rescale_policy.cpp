// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/detail/recalculate.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/iterators/point_iterator.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/policies/robustness/get_rescale_policy.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include <geometry_test_common.hpp>


template
<
    typename RescalePolicy,
    typename Geometry1,
    typename Geometry2
>
void test_one(std::string const& wkt1, std::string const& wkt2,
        std::string const& expected_coordinates)
{
    Geometry1 geometry1;
    bg::read_wkt(wkt1, geometry1);

    Geometry2 geometry2;
    bg::read_wkt(wkt2, geometry2);

    RescalePolicy rescale_policy
            = bg::get_rescale_policy<RescalePolicy>(geometry1, geometry2);

    typedef typename bg::point_type<Geometry1>::type point_type;
    typedef typename bg::robust_point_type
        <
            point_type, RescalePolicy
        >::type robust_point_type;

    {
        robust_point_type robust_point;
        bg::recalculate(robust_point, *bg::points_begin(geometry1), rescale_policy);

        std::ostringstream out;
        out << bg::get<0>(robust_point) << " " << bg::get<1>(robust_point);
        BOOST_CHECK_EQUAL(expected_coordinates, out.str());
    }

    {
        // Assuming Geometry1 is a polygon:
        typedef bg::model::polygon<robust_point_type> polygon_type;
        polygon_type geometry_out;
        bg::recalculate(geometry_out, geometry1, rescale_policy);
        robust_point_type p = *bg::points_begin(geometry_out);

        std::ostringstream out;
        out << bg::get<0>(p) << " " << bg::get<1>(p);
        BOOST_CHECK_EQUAL(expected_coordinates, out.str());
    }
}



static std::string simplex_normal[2] =
    {"POLYGON((0 1,2 5,5 3,0 1))",
    "POLYGON((3 0,0 3,4 5,3 0))"};

static std::string simplex_large[2] =
    {"POLYGON((0 1000,2000 5000,5000 3000,0 1000))",
    "POLYGON((3000 0,0 3000,4000 5000,3000 0))"};


template <bool Rescale, typename P>
void test_rescale(std::string const& expected_normal, std::string const& expected_large)
{
    using polygon = bg::model::polygon<P>;

    using rescale_policy_type = std::conditional_t
        <
            Rescale,
            typename bg::rescale_policy_type<P>::type,
            bg::detail::no_rescale_policy
        >;

    test_one<rescale_policy_type, polygon, polygon>(
        simplex_normal[0], simplex_normal[1],
        expected_normal);
    test_one<rescale_policy_type, polygon, polygon>(
        simplex_large[0], simplex_large[1],
        expected_large);
}

template <typename T>
void test_all(std::string const& expected_normal, std::string const& expected_large)
{
    typedef bg::model::d2::point_xy<T> point_type;
    test_rescale<true, point_type>(expected_normal, expected_large);
    //test_rescale<false, point_type>();
}


int test_main(int, char* [])
{
    test_all<double>("-5000000 -3000000", "-5000000 -3000000");
    test_all<long double>("-5000000 -3000000", "-5000000 -3000000");
    test_all<int>("0 1", "0 1000");
    test_all<long long>("0 1", "0 1000");
    //    test_all<short int>(); // compiles but overflows

    return 0;
}

