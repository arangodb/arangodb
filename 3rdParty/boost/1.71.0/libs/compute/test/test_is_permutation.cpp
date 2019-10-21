//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestIsPermutation
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/is_permutation.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(is_permutation_int)
{
    bc::int_ dataset1[] = {1, 3, 1, 2, 5};
    bc::vector<bc::int_> vector1(dataset1, dataset1 + 5, queue);

    bc::int_ dataset2[] = {3, 1, 5, 1, 2};
    bc::vector<bc::int_> vector2(dataset2, dataset2 + 5, queue);

    bool result =
        bc::is_permutation(vector1.begin(), vector1.begin() + 5,
                           vector2.begin(), vector2.begin() + 5, queue);

    BOOST_VERIFY(result == true);

    vector2.begin().write(bc::int_(1), queue);

    result = bc::is_permutation(vector1.begin(), vector1.begin() + 5,
                                vector2.begin(), vector2.begin() + 5,
                                queue);

    BOOST_VERIFY(result == false);
}

BOOST_AUTO_TEST_CASE(is_permutation_string)
{
    bc::char_ dataset1[] = "abade";
    bc::vector<bc::char_> vector1(dataset1, dataset1 + 5, queue);

    bc::char_ dataset2[] = "aadeb";
    bc::vector<bc::char_> vector2(dataset2, dataset2 + 5, queue);

    bool result =
        bc::is_permutation(vector1.begin(), vector1.begin() + 5,
                           vector2.begin(), vector2.begin() + 5, queue);

    BOOST_VERIFY(result == true);

    vector2.begin().write(bc::char_('b'), queue);

    result = bc::is_permutation(vector1.begin(), vector1.begin() + 5,
                                vector2.begin(), vector2.begin() + 5,
                                queue);

    BOOST_VERIFY(result == false);
}

BOOST_AUTO_TEST_SUITE_END()
