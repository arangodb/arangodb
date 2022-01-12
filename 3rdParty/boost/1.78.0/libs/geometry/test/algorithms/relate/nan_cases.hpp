// Boost.Geometry

// Copyright (c) 2015-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/range/value_type.hpp>

template <typename Container>
struct pusher
    : public Container
{
    typedef typename Container::value_type value_type;
    pusher(value_type const& val)
    {
        this->push_back(val);
    }

    pusher & operator()(value_type const& val)
    {
        this->push_back(val);
        return *this;
    }
};

template <typename Geometry,
          typename Tag = typename bg::tag<Geometry>::type,
          typename Coord = typename bg::coordinate_type<Geometry>::type>
struct nan_case_generator
{
    static void apply(Geometry & , std::string & )
    {}
};

template <typename Geometry>
struct nan_case_generator<Geometry, bg::multi_linestring_tag, double>
{
    static void apply(Geometry & geometry, std::string & wkt)
    {
        typedef typename boost::range_value<Geometry>::type ls;
        typedef typename bg::point_type<Geometry>::type P;
        typedef typename bg::coordinate_type<Geometry>::type coord_t;
        coord_t nan = std::numeric_limits<coord_t>::quiet_NaN();

        wkt = "MULTILINESTRING((3.1e+307 1,-nan -nan),(3.1e+307 1,-nan -nan),(3.1e+307 1,5.1e+307 6e+307,nan nan),(-nan -nan,nan nan),(-nan -nan,-1 -3.97843,-1 3.28571,-1 47.6364),(3 78.5455,3 2),(nan nan,3 12.9689),(3 -2,-nan -nan),(3 2,3 12.9689,3 78.5455),(-1 4.29497e+09,-1 7,-nan -nan),(9 5,-nan -nan),(-nan -nan,9 60.8755,9 124.909),(nan nan,1 6.87195e+10,-nan -nan),(nan nan,4 86.2727,4 20.9533),(4 -5,-nan -nan),(4 -5,-nan -nan),(4 -5,-nan -nan),(4 -5,1.1e+308 2.1e+307,nan nan),(-nan -nan,-1 -8),(-nan -nan,-9 -4))";

        typedef pusher<Geometry> ml;
        typedef pusher<ls> l;
        geometry = ml(l(P(3.1e+307, 1))(P(-nan, -nan)))
                     (l(P(3.1e+307, 1))(P(-nan, -nan)))
                     (l(P(3.1e+307, 1))(P(5.1e+307, 6e+307))(P(nan, nan)))
                     (l(P(-nan, -nan))(P(nan, nan)))
                     (l(P(-nan, -nan))(P(-1, -3.97843))(P(-1, 3.28571))(P(-1, 47.6364)))
                     (l(P(3, 78.5455))(P(3, 2)))
                     (l(P(nan, nan))(P(3, 12.9689)))
                     (l(P(3, -2))(P(-nan, -nan)))
                     (l(P(3, 2))(P(3, 12.9689))(P(3, 78.5455)))
                     (l(P(-1, 4.29497e+09))(P(-1, 7))(P(-nan, -nan)))
                     (l(P(9, 5))(P(-nan, -nan)))
                     (l(P(-nan, -nan))(P(9, 60.8755))(P(9, 124.909)))
                     (l(P(nan, nan))(P(1, 6.87195e+10))(P(-nan, -nan)))
                     (l(P(nan, nan))(P(4, 86.2727))(P(4, 20.9533)))
                     (l(P(4, -5))(P(-nan, -nan)))
                     (l(P(4, -5))(P(-nan, -nan)))
                     (l(P(4, -5))(P(-nan, -nan)))
                     (l(P(4, -5))(P(1.1e+308, 2.1e+307))(P(nan, nan)))
                     (l(P(-nan, -nan))(P(-1, -8)))
                     (l(P(-nan, -nan))(P(-9, -4)));
    }
};

template <typename Geometry>
struct nan_case_generator<Geometry, bg::multi_point_tag, double>
{
    static void apply(Geometry & geometry, std::string & wkt)
    {
        typedef typename bg::point_type<Geometry>::type P;
        typedef typename bg::coordinate_type<Geometry>::type coord_t;
        coord_t nan = std::numeric_limits<coord_t>::quiet_NaN();

        wkt = "MULTIPOINT((3.1e+307 1),(-nan -nan),(3.1e+307 1),(-nan -nan),(3.1e+307 1),(5.1e+307 6e+307),(nan nan),(-nan -nan),(nan nan),(-nan -nan),(-1 -3.97843),(-1 3.28571),(-1 47.6364),(3 78.5455),(3 2),(nan nan),(3 12.9689),(3 -2),(-nan -nan),(3 2),(3 12.9689),(3 78.5455),(-1 4.29497e+09),(-1 7),(-nan -nan),(9 5),(-nan -nan),(-nan -nan),(9 60.8755),(9 124.909),(nan nan),(1 6.87195e+10),(-nan -nan),(nan nan),(4 86.2727),(4 20.9533),(4 -5),(-nan -nan),(4 -5),(-nan -nan),(4 -5),(-nan -nan),(4 -5),(1.1e+308 2.1e+307),(nan nan),(-nan -nan),(-1 -8),(-nan -nan),(-9 -4))";

        typedef pusher<Geometry> mp;
        geometry = mp(P(3.1e+307, 1))(P(-nan, -nan))
                     (P(3.1e+307, 1))(P(-nan, -nan))
                     (P(3.1e+307, 1))(P(5.1e+307, 6e+307))(P(nan, nan))
                     (P(-nan, -nan))(P(nan, nan))
                     (P(-nan, -nan))(P(-1, -3.97843))(P(-1, 3.28571))(P(-1, 47.6364))
                     (P(3, 78.5455))(P(3, 2))
                     (P(nan, nan))(P(3, 12.9689))
                     (P(3, -2))(P(-nan, -nan))
                     (P(3, 2))(P(3, 12.9689))(P(3, 78.5455))
                     (P(-1, 4.29497e+09))(P(-1, 7))(P(-nan, -nan))
                     (P(9, 5))(P(-nan, -nan))
                     (P(-nan, -nan))(P(9, 60.8755))(P(9, 124.909))
                     (P(nan, nan))(P(1, 6.87195e+10))(P(-nan, -nan))
                     (P(nan, nan))(P(4, 86.2727))(P(4, 20.9533))
                     (P(4, -5))(P(-nan, -nan))
                     (P(4, -5))(P(-nan, -nan))
                     (P(4, -5))(P(-nan, -nan))
                     (P(4, -5))(P(1.1e+308, 2.1e+307))(P(nan, nan))
                     (P(-nan, -nan))(P(-1, -8))
                     (P(-nan, -nan))(P(-9, -4));
    }
};

template <typename Geometry>
void nan_case(Geometry & geometry, std::string & wkt)
{
    nan_case_generator<Geometry>::apply(geometry, wkt);
}

template <typename Geometry>
struct is_nan_case_supported
{
    typedef typename bg::coordinate_type<Geometry>::type coord_t;

    static const bool value = std::is_same<coord_t, double>::value
                           && std::numeric_limits<coord_t>::has_quiet_NaN;
};
