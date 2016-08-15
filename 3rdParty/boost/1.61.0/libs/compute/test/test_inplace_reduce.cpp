//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestInplaceReduce
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/detail/inplace_reduce.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(sum_int)
{
    int data[] = { 1, 5, 3, 4, 9, 3, 5, 3 };
    boost::compute::vector<int> vector(data, data + 8, queue);

    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(33));

    vector.assign(data, data + 8);
    vector.push_back(3);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(36));
}

BOOST_AUTO_TEST_CASE(multiply_int)
{
    int data[] = { 1, 5, 3, 4, 9, 3, 5, 3 };
    boost::compute::vector<int> vector(data, data + 8, queue);

    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::multiplies<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(24300));

    vector.assign(data, data + 8);
    vector.push_back(3);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::multiplies<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(72900));
}

BOOST_AUTO_TEST_CASE(reduce_iota)
{
    // 1 value
    boost::compute::vector<int> vector(1, context);
    boost::compute::iota(vector.begin(), vector.end(), int(0), queue);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(0));

    // 1000 values
    vector.resize(1000);
    boost::compute::iota(vector.begin(), vector.end(), int(0), queue);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(499500));

    // 2499 values
    vector.resize(2499);
    boost::compute::iota(vector.begin(), vector.end(), int(0), queue);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(3121251));

    // 4096 values
    vector.resize(4096);
    boost::compute::iota(vector.begin(), vector.end(), int(0), queue);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(8386560));

    // 5000 values
    vector.resize(5000);
    boost::compute::iota(vector.begin(), vector.end(), int(0), queue);
    boost::compute::detail::inplace_reduce(vector.begin(),
                                           vector.end(),
                                           boost::compute::plus<int>(),
                                           queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int(vector[0]), int(12497500));
}

BOOST_AUTO_TEST_SUITE_END()
