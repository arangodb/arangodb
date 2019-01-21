//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestSetIntersection
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/set_intersection.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(set_intersection_int)
{
    int dataset1[] = {1, 1, 2, 2, 2, 2, 3, 3, 4, 5, 6, 10};
    bc::vector<bc::int_> set1(dataset1, dataset1 + 12, queue);

    int dataset2[] = {0, 2, 2, 4, 5, 6, 8, 8, 9, 9, 9, 13};
    bc::vector<bc::int_> set2(dataset2, dataset2 + 12, queue);

    bc::vector<bc::uint_>result(10, queue.get_context());

    bc::vector<bc::uint_>::iterator iter =
        bc::set_intersection(set1.begin(), set1.begin() + 12,
                             set2.begin(), set2.begin() + 12,
                             result.begin(), queue);

    CHECK_RANGE_EQUAL(int, 5, result, (2, 2, 4, 5, 6));
    BOOST_VERIFY(iter == result.begin()+5);
}

BOOST_AUTO_TEST_CASE(set_intersection_string)
{
    char string1[] = "abcccdddeeff";
    bc::vector<bc::char_> set1(string1, string1 + 12, queue);

    char string2[] = "bccdfgh";
    bc::vector<bc::char_> set2(string2, string2 + 7, queue);

    bc::vector<bc::char_>result(5, queue.get_context());

    bc::vector<bc::char_>::iterator iter =
        bc::set_intersection(set1.begin(), set1.begin() + 12,
                             set2.begin(), set2.begin() + 7,
                             result.begin(), queue);

    CHECK_RANGE_EQUAL(char, 5, result, ('b', 'c', 'c', 'd', 'f'));
    BOOST_VERIFY(iter == result.begin()+5);
}

BOOST_AUTO_TEST_SUITE_END()
