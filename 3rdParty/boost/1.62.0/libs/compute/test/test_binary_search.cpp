//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestBinarySearch
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/binary_search.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/lower_bound.hpp>
#include <boost/compute/algorithm/upper_bound.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(binary_search_int)
{
    // test data = { 1, ..., 2, ..., 4, 4, 5, 7, ..., 9, ..., 10 }
    boost::compute::vector<int> vector(size_t(4096), int(1), queue);
    boost::compute::vector<int>::iterator first = vector.begin() + 128;
    boost::compute::vector<int>::iterator last = first + (1024 - 128);
    boost::compute::fill(first, last, int(2), queue);
    last.write(4, queue); last++;
    last.write(4, queue); last++;
    last.write(5, queue); last++;
    first = last;
    last = first + 127;
    boost::compute::fill(first, last, 7, queue);
    first = last;
    last = vector.end() - 1;
    boost::compute::fill(first, last, 9, queue);
    last.write(10, queue);
    queue.finish();

    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(0), queue) == false);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(1), queue) == true);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(2), queue) == true);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(3), queue) == false);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(4), queue) == true);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(5), queue) == true);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(6), queue) == false);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(7), queue) == true);
    BOOST_CHECK(boost::compute::binary_search(vector.begin(), vector.end(), int(8), queue) == false);
}

BOOST_AUTO_TEST_CASE(range_bounds_int)
{
    // test data = { 1, ..., 2, ..., 4, 4, 5, 7, ..., 9, ..., 10 }
    boost::compute::vector<int> vector(size_t(4096), int(1), queue);
    boost::compute::vector<int>::iterator first = vector.begin() + 128;
    boost::compute::vector<int>::iterator last = first + (1024 - 128);
    boost::compute::fill(first, last, int(2), queue);
    last.write(4, queue); last++; // 1024
    last.write(4, queue); last++; // 1025
    last.write(5, queue); last++; // 1026
    first = last;
    last = first + 127;
    boost::compute::fill(first, last, 7, queue);
    first = last;
    last = vector.end() - 1;
    boost::compute::fill(first, last, 9, queue);
    last.write(10, queue);
    queue.finish();

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(0), queue) == vector.begin());
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(0), queue) == vector.begin());

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(1), queue) == vector.begin());
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(1), queue) == vector.begin() + 128);

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(2), queue) == vector.begin() + 128);
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(2), queue) == vector.begin() + 1024);

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(4), queue) == vector.begin() + 1024);
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(4), queue) == vector.begin() + 1026);

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(5), queue) == vector.begin() + 1026);
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(5), queue) == vector.begin() + 1027);

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(6), queue) == vector.begin() + 1027);
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(6), queue) == vector.begin() + 1027);

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(7), queue) == vector.begin() + 1027);
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(7), queue) == vector.begin() + (1027 + 127));

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(9), queue) == vector.begin() + (1027 + 127));
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(9), queue) == vector.end() - 1);

    BOOST_CHECK(boost::compute::lower_bound(vector.begin(), vector.end(), int(10), queue) == vector.end() - 1);
    BOOST_CHECK(boost::compute::upper_bound(vector.begin(), vector.end(), int(10), queue) == vector.end());
}

BOOST_AUTO_TEST_SUITE_END()
