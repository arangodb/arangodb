//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPrevPermutation
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/prev_permutation.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(prev_permutation_int)
{
    int dataset[] = {1, 3, 4, 2, 5};
    bc::vector<bc::int_> vector(dataset, dataset + 5, queue);

    bool result =
        bc::prev_permutation(vector.begin(), vector.begin() + 5, queue);

    CHECK_RANGE_EQUAL(int, 5, vector, (1, 3, 2, 5, 4));
    BOOST_VERIFY(result == true);

    vector[1] = 1; vector[4] = 6;

    result = bc::prev_permutation(vector.begin(), vector.begin() + 5, queue);

    CHECK_RANGE_EQUAL(int, 5, vector, (6, 5, 2, 1, 1));
    BOOST_VERIFY(result == false);
}

BOOST_AUTO_TEST_CASE(prev_permutation_string)
{
    char dataset[] = "baaa";
    bc::vector<bc::char_> vector(dataset, dataset + 4, queue);

    bool result =
        bc::prev_permutation(vector.begin(), vector.begin() + 4, queue);

    CHECK_RANGE_EQUAL(char, 4, vector, ('a', 'b', 'a', 'a'));
    BOOST_VERIFY(result == true);

    result = bc::prev_permutation(vector.begin(), vector.begin() + 4, queue);

    CHECK_RANGE_EQUAL(char, 4, vector, ('a', 'a', 'b', 'a'));
    BOOST_VERIFY(result == true);

    result = bc::prev_permutation(vector.begin(), vector.begin() + 4, queue);

    CHECK_RANGE_EQUAL(char, 4, vector, ('a', 'a', 'a', 'b'));
    BOOST_VERIFY(result == true);

    result = bc::prev_permutation(vector.begin(), vector.begin() + 4, queue);

    CHECK_RANGE_EQUAL(char, 4, vector, ('b', 'a', 'a', 'a'));
    BOOST_VERIFY(result == false);
}

BOOST_AUTO_TEST_SUITE_END()
