// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <iterator>

#include <geometry_test_common.hpp>

#include <boost/geometry/iterators/closing_iterator.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/ring.hpp>

#include <boost/range/adaptor/transformed.hpp>


void test_concept()
{
    std::vector<int> v = {1, 2, 3};

    using iterator = bg::closing_iterator<std::vector<int>>;
    using const_iterator = bg::closing_iterator<std::vector<int> const>;

    iterator it, it2(v);
    const_iterator cit, cit2(v);

    BOOST_CHECK((std::is_same<decltype(*it2), const int&>::value));
    BOOST_CHECK((std::is_same<decltype(*cit2), const int&>::value));

    it = it2;
    cit = cit2;
    BOOST_CHECK_EQUAL(*it, 1);
    BOOST_CHECK_EQUAL(*cit, 1);
    cit = ++it;
    BOOST_CHECK_EQUAL(*it, 2);
    BOOST_CHECK_EQUAL(*cit, 2);
    // This is ok because both of these iterators are in fact const iterators
    it = ++cit;
    BOOST_CHECK_EQUAL(*it, 3);
    BOOST_CHECK_EQUAL(*cit, 3);

    // Check compilations, it is (since Oct 2010) random access
    it--;
    --it;
    it += 2;
    it -= 2;
}

// The closing iterator should also work on normal std:: containers
void test_empty_non_geometry()
{
    std::vector<int> v;

    typedef bg::closing_iterator
        <
            std::vector<int> const
        > closing_iterator;

    closing_iterator it(v);
    closing_iterator end(v, true);

    BOOST_CHECK(it == end);
}

void test_non_geometry()
{
    std::vector<int> v = {1, 2, 3};
    std::vector<int> closed_v = { 1, 2, 3, 1 };
    
    typedef bg::closing_iterator
        <
            std::vector<int> const
        > closing_iterator;

    closing_iterator it(v);
    closing_iterator end(v, true);
    
    BOOST_CHECK(std::equal(it, end, closed_v.begin(), closed_v.end()));
}

void test_transformed_non_geometry()
{
    std::vector<int> v = {-1, -2, -3};
    std::vector<int> closed_v = { 1, 2, 3, 1 };
    
    typedef boost::transformed_range
        <
            std::negate<int>,
            std::vector<int>
        > transformed_range;

    typedef bg::closing_iterator
        <
            transformed_range const
        > closing_iterator;

    transformed_range v2 = v | boost::adaptors::transformed(std::negate<int>());
    closing_iterator it(v2);
    closing_iterator end(v2, true);

    BOOST_CHECK(std::equal(it, end, closed_v.begin(), closed_v.end()));
}

template <typename P>
void test_geometry()
{
    bg::model::ring<P> geometry = { {1, 1}, {1, 4}, {4, 4}, {4, 1} };
    bg::model::ring<P> closed = { {1, 1}, {1, 4}, {4, 4}, {4, 1}, {1, 1} };
    
    using closing_iterator = bg::closing_iterator<bg::model::ring<P> const>;

    auto pred = [](P const& p1, P const& p2)
        {
            return bg::get<0>(p1) == bg::get<0>(p2) && bg::get<1>(p1) == bg::get<1>(p2);
        };

    // 1. Test normal behaviour
    {
        closing_iterator it(geometry);
        closing_iterator end(geometry, true);

        BOOST_CHECK((std::equal(it, end, closed.begin(), closed.end(), pred)));
    }

    // 2: check copy behaviour
    {
        bg::model::ring<P> copy;

        std::copy(closing_iterator(geometry), closing_iterator(geometry, true),
                  std::back_inserter(copy));

        BOOST_CHECK((std::equal(copy.begin(), copy.end(), closed.begin(), closed.end(), pred)));
    }
}


int test_main(int, char* [])
{
    test_concept();
    test_empty_non_geometry();
    test_non_geometry();
    test_transformed_non_geometry();

    test_geometry<bg::model::d2::point_xy<double>>();

    return 0;
}
