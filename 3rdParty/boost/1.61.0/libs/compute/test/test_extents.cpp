//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestExtents
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/extents.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(initialize)
{
    compute::extents<1> one(1);
    BOOST_CHECK_EQUAL(one[0], size_t(1));

    compute::extents<3> xyz = compute::dim(1, 2, 3);
    BOOST_CHECK_EQUAL(xyz[0], size_t(1));
    BOOST_CHECK_EQUAL(xyz[1], size_t(2));
    BOOST_CHECK_EQUAL(xyz[2], size_t(3));
}

BOOST_AUTO_TEST_CASE(size)
{
    BOOST_CHECK_EQUAL(compute::extents<1>().size(), size_t(1));
    BOOST_CHECK_EQUAL(compute::extents<2>().size(), size_t(2));
    BOOST_CHECK_EQUAL(compute::extents<3>().size(), size_t(3));
}

BOOST_AUTO_TEST_CASE(subscript_operator)
{
    compute::extents<3> xyz;
    BOOST_CHECK_EQUAL(xyz[0], size_t(0));
    BOOST_CHECK_EQUAL(xyz[1], size_t(0));
    BOOST_CHECK_EQUAL(xyz[2], size_t(0));

    xyz[0] = size_t(10);
    xyz[1] = size_t(20);
    xyz[2] = size_t(30);
    BOOST_CHECK_EQUAL(xyz[0], size_t(10));
    BOOST_CHECK_EQUAL(xyz[1], size_t(20));
    BOOST_CHECK_EQUAL(xyz[2], size_t(30));
}

BOOST_AUTO_TEST_CASE(data)
{
    compute::extents<3> xyz = compute::dim(5, 6, 7);
    BOOST_CHECK_EQUAL(xyz.data()[0], size_t(5));
    BOOST_CHECK_EQUAL(xyz.data()[1], size_t(6));
    BOOST_CHECK_EQUAL(xyz.data()[2], size_t(7));
}

BOOST_AUTO_TEST_CASE(linear)
{
    compute::extents<2> uv = compute::dim(16, 16);
    BOOST_CHECK_EQUAL(uv.linear(), size_t(256));
}

BOOST_AUTO_TEST_CASE(equality_operator)
{
    BOOST_CHECK(compute::dim(10, 20) == compute::dim(10, 20));
    BOOST_CHECK(compute::dim(20, 10) != compute::dim(10, 20));
}

BOOST_AUTO_TEST_CASE(empty_dim)
{
    BOOST_CHECK(compute::dim<0>() == compute::dim());
    BOOST_CHECK(compute::dim<1>() == compute::dim(0));
    BOOST_CHECK(compute::dim<2>() == compute::dim(0, 0));
    BOOST_CHECK(compute::dim<3>() == compute::dim(0, 0, 0));
}

BOOST_AUTO_TEST_CASE(copy_to_vector)
{
    compute::extents<3> exts = compute::dim(4, 5, 6);

    std::vector<size_t> vec(3);
    std::copy(exts.begin(), exts.end(), vec.begin());
    BOOST_CHECK_EQUAL(vec[0], size_t(4));
    BOOST_CHECK_EQUAL(vec[1], size_t(5));
    BOOST_CHECK_EQUAL(vec[2], size_t(6));
}

BOOST_AUTO_TEST_SUITE_END()
