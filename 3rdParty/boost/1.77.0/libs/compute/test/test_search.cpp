//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestSearch
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/search.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(search_int)
{
    int data[] = {1, 4, 2, 6, 3, 2, 6, 3, 4, 6, 6};
    bc::vector<bc::int_> vectort(data, data + 11, queue);

    int datap[] = {2, 6};
    bc::vector<bc::int_> vectorp(datap, datap + 2, queue);

    bc::vector<bc::int_>::iterator iter =
        bc::search(vectort.begin(), vectort.end(),
                    vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == vectort.begin() + 2);

    vectorp[1] = 9;

    iter =
        bc::search(vectort.begin(), vectort.end(),
                    vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == vectort.begin() + 11);

    vectorp[0] = 6;
    vectorp[1] = 6;

    iter =
        bc::search(vectort.begin(), vectort.end(),
                    vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == vectort.begin() + 9);
}

BOOST_AUTO_TEST_CASE(search_string)
{
    char text[] = "sdabababacabskjabacab";
    bc::vector<bc::char_> vectort(text, text + 21, queue);

    char pattern[] = "aba";
    bc::vector<bc::char_> vectorp(pattern, pattern + 3, queue);

    bc::vector<bc::char_>::iterator iter =
        bc::search(vectort.begin(), vectort.end(),
                    vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == vectort.begin() + 2);
}

BOOST_AUTO_TEST_SUITE_END()
