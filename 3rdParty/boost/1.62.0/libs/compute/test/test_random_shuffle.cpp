//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestRandomShuffle
#include <boost/test/unit_test.hpp>

#include <set>
#include <iterator>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/random_shuffle.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(shuffle_int_vector)
{
    bc::vector<int> vector(context);
    vector.push_back(1, queue);
    vector.push_back(9, queue);
    vector.push_back(19, queue);
    vector.push_back(29, queue);

    std::set<int> original_values;
    for(size_t i = 0; i < vector.size(); i++){
        original_values.insert(vector[i]);
    }
    BOOST_CHECK_EQUAL(original_values.size(), size_t(4));

    bc::random_shuffle(vector.begin(), vector.end(), queue);

    std::set<int> shuffled_values;
    bc::copy(
        vector.begin(),
        vector.end(),
        std::inserter(shuffled_values, shuffled_values.begin()),
        queue
    );
    BOOST_CHECK_EQUAL(shuffled_values.size(), size_t(4));
    BOOST_VERIFY(original_values == shuffled_values);
}

BOOST_AUTO_TEST_SUITE_END()
