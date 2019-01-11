//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestIncludes
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/includes.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(includes_int)
{
    int dataset1[] = {1, 1, 2, 2, 2, 2, 3, 3, 4, 5, 6, 10};
    bc::vector<bc::int_> set1(dataset1, dataset1 + 12, queue);

    int dataset2[] = {2, 4, 5, 6};
    bc::vector<bc::int_> set2(dataset2, dataset2 + 4, queue);

    bool includes = bc::includes(set1.begin(), set1.begin() + 12,
                                set2.begin(), set2.begin() + 4, queue);
    BOOST_VERIFY(includes == true);

    set2[3] = 7;
    includes = bc::includes(set1.begin(), set1.begin() + 12,
                            set2.begin(), set2.begin() + 4, queue);
    BOOST_VERIFY(includes == false);
}

BOOST_AUTO_TEST_CASE(includes_string)
{
    char string1[] = "abcccdddeeff";
    bc::vector<bc::char_> set1(string1, string1 + 12, queue);

    char string2[] = "bccdf";
    bc::vector<bc::char_> set2(string2, string2 + 5, queue);

    bool includes = bc::includes(set1.begin(), set1.begin() + 12,
                                set2.begin(), set2.begin() + 5, queue);
    BOOST_VERIFY(includes == true);

    set2[0] = 'g';
    includes = bc::includes(set1.begin(), set1.begin() + 12,
                            set2.begin(), set2.begin() + 4, queue);
    BOOST_VERIFY(includes == false);
}

BOOST_AUTO_TEST_SUITE_END()
