//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestEqualRange
#include <boost/test/unit_test.hpp>

#include <utility>
#include <iterator>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/equal_range.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(equal_range_int)
{
    int data[] = { 1, 2, 2, 2, 3, 3, 4, 5 };
    boost::compute::vector<int> vector(data, data + 8, queue);

    typedef boost::compute::vector<int>::iterator iterator;

    std::pair<iterator, iterator> range0 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(0), queue);
    BOOST_CHECK(range0.first == vector.begin());
    BOOST_CHECK(range0.second == vector.begin());
    BOOST_CHECK_EQUAL(std::distance(range0.first, range0.second), ptrdiff_t(0));

    std::pair<iterator, iterator> range1 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(1), queue);
    BOOST_CHECK(range1.first == vector.begin());
    BOOST_CHECK(range1.second == vector.begin() + 1);
    BOOST_CHECK_EQUAL(std::distance(range1.first, range1.second), ptrdiff_t(1));

    std::pair<iterator, iterator> range2 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(2), queue);
    BOOST_CHECK(range2.first == vector.begin() + 1);
    BOOST_CHECK(range2.second == vector.begin() + 4);
    BOOST_CHECK_EQUAL(std::distance(range2.first, range2.second), ptrdiff_t(3));

    std::pair<iterator, iterator> range3 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(3), queue);
    BOOST_CHECK(range3.first == vector.begin() + 4);
    BOOST_CHECK(range3.second == vector.begin() + 6);
    BOOST_CHECK_EQUAL(std::distance(range3.first, range3.second), ptrdiff_t(2));

    std::pair<iterator, iterator> range4 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(4), queue);
    BOOST_CHECK(range4.first == vector.begin() + 6);
    BOOST_CHECK(range4.second == vector.begin() + 7);
    BOOST_CHECK_EQUAL(std::distance(range4.first, range4.second), ptrdiff_t(1));

    std::pair<iterator, iterator> range5 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(5), queue);
    BOOST_CHECK(range5.first == vector.begin() + 7);
    BOOST_CHECK(range5.second == vector.end());
    BOOST_CHECK_EQUAL(std::distance(range5.first, range5.second), ptrdiff_t(1));

    std::pair<iterator, iterator> range6 =
        boost::compute::equal_range(vector.begin(), vector.end(), int(6), queue);
    BOOST_CHECK(range6.first == vector.end());
    BOOST_CHECK(range6.second == vector.end());
    BOOST_CHECK_EQUAL(std::distance(range6.first, range6.second), ptrdiff_t(0));
}

BOOST_AUTO_TEST_SUITE_END()
