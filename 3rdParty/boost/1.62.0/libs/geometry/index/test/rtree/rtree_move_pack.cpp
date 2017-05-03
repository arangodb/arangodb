// Boost.Geometry Index
// Unit Test

// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>

#include <algorithm>
#include <boost/container/vector.hpp>
#include <boost/move/move.hpp>
#include <boost/move/iterator.hpp>

#include <boost/geometry/geometries/register/point.hpp>

class point_cm
{
    BOOST_COPYABLE_AND_MOVABLE(point_cm)

public:
    point_cm(double xx = 0, double yy = 0)
        : x(xx)
        , y(yy)
        , moved(false)
    {}
    point_cm(point_cm const& other)
        : x(other.x)
        , y(other.y)
        , moved(false)
    {
        BOOST_CHECK_MESSAGE(false, "copy not allowed");
    }
    point_cm & operator=(BOOST_COPY_ASSIGN_REF(point_cm) other)
    {
        BOOST_CHECK_MESSAGE(false, "copy not allowed");
        x = other.x;
        y = other.y;
        moved = false;
        return *this;
    }
    point_cm(BOOST_RV_REF(point_cm) other)
        : x(other.x)
        , y(other.y)
        , moved(false)
    {
        BOOST_CHECK_MESSAGE(!other.moved, "only one move allowed");
        other.moved = true;
    }
    point_cm & operator=(BOOST_RV_REF(point_cm) other)
    {
        BOOST_CHECK_MESSAGE(!other.moved, "only one move allowed");
        x = other.x;
        y = other.y;
        moved = false;
        other.moved = true;
        return *this;
    }

    double x, y;
    bool moved;
};

template <typename Point>
struct indexable
{
    typedef Point const& result_type;
    result_type operator()(Point const& p) const
    {
        BOOST_CHECK_MESSAGE(!p.moved, "can't access indexable of moved Value");
        return p;
    }
};

BOOST_GEOMETRY_REGISTER_POINT_2D(point_cm, double, bg::cs::cartesian, x, y)

template <typename Vector>
void append(Vector & vec, double x, double y)
{
    point_cm pt(x, y);
    BOOST_CHECK(!pt.moved);
    vec.push_back(boost::move(pt));
    BOOST_CHECK(pt.moved);
}

struct test_moved
{
    test_moved(bool ex)
        : expected(ex)
    {}
    template <typename Point>
    void operator()(Point const& p) const
    {
        BOOST_CHECK_EQUAL(p.moved, expected);
    }
    bool expected;
};

template <typename Point, typename Params>
void test_rtree(Params const& params = Params())
{
    // sanity check
    boost::container::vector<Point> vec;
    append(vec, 0, 0); append(vec, 0, 1); append(vec, 0, 2);
    append(vec, 1, 0); append(vec, 1, 1); append(vec, 1, 2);
    append(vec, 2, 0); append(vec, 2, 1); append(vec, 2, 2);

    std::for_each(vec.begin(), vec.end(), test_moved(false));

    bgi::rtree<Point, Params, indexable<Point> > rt(
        boost::make_move_iterator(vec.begin()),
        boost::make_move_iterator(vec.end()),
        params);

    std::for_each(vec.begin(), vec.end(), test_moved(true));

    BOOST_CHECK_EQUAL(rt.size(), vec.size());
}


int test_main(int, char* [])
{
    test_rtree< point_cm, bgi::linear<4> >();
    test_rtree< point_cm, bgi::quadratic<4> >();
    test_rtree< point_cm, bgi::rstar<4> >();

    test_rtree<point_cm>(bgi::dynamic_linear(4));
    test_rtree<point_cm>(bgi::dynamic_quadratic(4));
    test_rtree<point_cm>(bgi::dynamic_rstar(4));

    return 0;
}
