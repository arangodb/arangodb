//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestEqual
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/equal.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/container/string.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(equal_int)
{
    int data1[] = { 1, 2, 3, 4, 5, 6 };
    int data2[] = { 1, 2, 3, 7, 5, 6 };

    boost::compute::vector<int> vector1(data1, data1 + 6, queue);
    boost::compute::vector<int> vector2(data2, data2 + 6, queue);

    BOOST_CHECK(boost::compute::equal(vector1.begin(), vector1.end(),
                                      vector2.begin(), queue) == false);
    BOOST_CHECK(boost::compute::equal(vector1.begin(), vector1.begin() + 2,
                                      vector2.begin(), queue) == true);
    BOOST_CHECK(boost::compute::equal(vector1.begin() + 4, vector1.end(),
                                      vector2.begin() + 4, queue) == true);
}

BOOST_AUTO_TEST_CASE(equal_string)
{
    boost::compute::string a = "abcdefghijk";
    boost::compute::string b = "abcdefghijk";
    boost::compute::string c = "abcdezghijk";

    BOOST_CHECK(boost::compute::equal(a.begin(), a.end(), b.begin(), queue) == true);
    BOOST_CHECK(boost::compute::equal(a.begin(), a.end(), c.begin(), queue) == false);
}

BOOST_AUTO_TEST_CASE(equal_different_range_sizes)
{
    boost::compute::vector<int> a(10, context);
    boost::compute::vector<int> b(20, context);

    boost::compute::fill(a.begin(), a.end(), 3, queue);
    boost::compute::fill(b.begin(), b.end(), 3, queue);

    BOOST_CHECK(boost::compute::equal(a.begin(), a.end(), b.begin(), b.end(), queue) == false);
    BOOST_CHECK(boost::compute::equal(a.begin(), a.end(), a.begin(), a.end(), queue) == true);
    BOOST_CHECK(boost::compute::equal(b.begin(), b.end(), a.begin(), a.end(), queue) == false);
    BOOST_CHECK(boost::compute::equal(b.begin(), b.end(), b.begin(), b.end(), queue) == true);
}

BOOST_AUTO_TEST_SUITE_END()
