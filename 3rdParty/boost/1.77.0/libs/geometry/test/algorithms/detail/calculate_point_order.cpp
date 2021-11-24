// Boost.Geometry
// Unit Test

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/calculate_point_order.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/ring.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/strategies/cartesian/point_order.hpp>
#include <boost/geometry/strategies/geographic/point_order.hpp>
#include <boost/geometry/strategies/spherical/point_order.hpp>

//TEMP
#include <boost/geometry/strategies/area/cartesian.hpp>
#include <boost/geometry/strategies/area/geographic.hpp>
#include <boost/geometry/strategies/area/spherical.hpp>


inline const char * order_str(bg::order_selector o)
{
    if (o == bg::clockwise)
        return "clockwise";
    else if (o == bg::counterclockwise)
        return "counterclockwise";
    else
        return "order_undetermined";
}

inline const char * cs_str(bg::cartesian_tag)
{
    return "cartesian";
}

inline const char * cs_str(bg::spherical_equatorial_tag)
{
    return "spherical_equatorial";
}

inline const char * cs_str(bg::geographic_tag)
{
    return "geographic";
}

template <typename Ring>
inline void test_one(Ring const& ring, bg::order_selector expected)
{
    typedef typename bg::cs_tag<Ring>::type cs_tag;

    bg::order_selector result = bg::detail::calculate_point_order(ring);

    BOOST_CHECK_MESSAGE(result == expected, "Expected: " << order_str(expected) << " for " << bg::wkt(ring) << " in " << cs_str(cs_tag()));

    if (expected != bg::order_undetermined && result != bg::order_undetermined)
    {
        Ring ring_rev = ring;

        std::reverse(boost::begin(ring_rev), boost::end(ring_rev));

        bg::order_selector result_rev = bg::detail::calculate_point_order(ring_rev);

        BOOST_CHECK_MESSAGE(result != result_rev, "Invalid order of reversed: " << bg::wkt(ring) << " in " << cs_str(cs_tag()));
    }
}

template <typename P>
inline void test_one(std::string const& ring_wkt, bg::order_selector expected)
{
    //typedef typename bg::cs_tag<P>::type cs_tag;

    bg::model::ring<P> ring;
    bg::read_wkt(ring_wkt, ring);

    std::size_t n = boost::size(ring);
    for (size_t i = 1; i < n; ++i)
    {
        test_one(ring, expected);
        std::rotate(boost::begin(ring), boost::begin(ring) + 1, boost::end(ring));

        // it seems that area method doesn't work for invalid "opened" polygons
        //if (! boost::is_same<cs_tag, bg::geographic_tag>::value)
        {
            P p = bg::range::front(ring);
            bg::range::push_back(ring, p);
        }
    }
}

template <typename P>
void test_all()
{
    // From correct() test, rings rotated and reversed in test_one()
    test_one<P>("POLYGON((0 0,0 1,1 1,1 0,0 0))", bg::clockwise);
    test_one<P>("POLYGON((0 0,0 1,1 1,1 0))", bg::clockwise);
    test_one<P>("POLYGON((0 0,0 4,4 4,4 0,0 0))", bg::clockwise);
    test_one<P>("POLYGON((1 1,2 1,2 2,1 2,1 1))", bg::counterclockwise);

   test_one<P>("POLYGON((0 5, 5 5, 5 0, 0 0, 0 5))", bg::clockwise);
   test_one<P>("POLYGON((0 5, 0 5, 0 6, 0 6, 0 4, 0 5, 5 5, 5 0, 0 0, 0 6, 0 5))", bg::clockwise);
   test_one<P>("POLYGON((2 0, 2 1, 2 -1, 2 0, 1 0, 1 -1, 0 -1, 0 1, 1 1, 1 0, 2 0))", bg::clockwise);
   test_one<P>("POLYGON((2 0, 2 1, 2 -1, 2 0, 1 0, 1 -1, 0 -1, 0 1, 1 1, 1 0))", bg::clockwise);
   test_one<P>("POLYGON((2 0, 2 1, 3 1, 3 -1, 2 -1, 2 0, 1 0, 1 -1, 0 -1, 0 1, 1 1, 1 0, 2 0))", bg::clockwise);
   test_one<P>("POLYGON((0 85, 5 85, 5 84, 0 84, 0 85))", bg::clockwise);
   test_one<P>("POLYGON((0 2, 170 2, 170 0, 0 0, 0 2))", bg::clockwise);
   test_one<P>("POLYGON((0 2, 170 2, 170 1, 160 1, 10 1, 0 1, 0 2))", bg::clockwise);
   test_one<P>("POLYGON((0 2, 170 2, 170 -2, 0 -2, 0 2))", bg::clockwise);
   test_one<P>("POLYGON((5 5, 6 5, 6 6, 6 4, 6 5, 5 5, 5 4, 4 4, 4 6, 5 6, 5 5))", bg::clockwise);
   test_one<P>("POLYGON((5 5, 6 5, 6 6, 6 4, 6 5, 5 5, 5 6, 4 6, 4 4, 5 4, 5 5))", bg::counterclockwise);
   test_one<P>("POLYGON((5 5, 6 5, 6 5, 6 6, 6 5, 6 4, 6 5, 6 5, 5 5, 5 4, 4 4, 4 6, 5 6, 5 5))", bg::clockwise);

   // https://github.com/boostorg/geometry/pull/554
   test_one<P>("POLYGON((9.8591674311151110 54.602813224425063, 9.8591651519791412 54.602359676428932, 9.8584586199249316 54.602359676428932, 9.8591674311151110 54.602813224425063))",
               bg::clockwise);
}

template <typename P>
void test_cartesian()
{
    test_one<P>("POLYGON((0 5, 1 5, 1 6, 1 4, 2 4, 0 4, 0 3, 0 5))", bg::clockwise);
}

template <typename P>
void test_spheroidal()
{
    test_one<P>("POLYGON((0 5, 1 5, 1 6, 1 4, 0 4, 0 3, 0 5))", bg::clockwise);

    test_one<P>("POLYGON((0 45, 120 45, -120 45, 0 45))", bg::counterclockwise);
}

int test_main(int, char* [])
{
    test_all<bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_all<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

    test_cartesian<bg::model::point<double, 2, bg::cs::cartesian> >();

    test_spheroidal<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_spheroidal<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
    
    return 0;
}
