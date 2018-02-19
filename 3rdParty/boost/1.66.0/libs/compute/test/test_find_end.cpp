//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFindEnd
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/find_end.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(find_end_int)
{
    bc::int_ data[] = {1, 4, 2, 6, 3, 2, 6, 3, 4, 6};
    bc::vector<bc::int_> vectort(data, data + 10, queue);

    bc::int_ datap[] = {2, 6};
    bc::vector<bc::int_> vectorp(datap, datap + 2, queue);

    bc::vector<bc::int_>::iterator iter =
        bc::find_end(vectort.begin(), vectort.end(),
                    vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == vectort.begin() + 5);

    vectorp.insert(vectorp.begin() + 1, bc::int_(9), queue);

    iter =
        bc::find_end(vectort.begin(), vectort.end(),
                     vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == vectort.end());
}

BOOST_AUTO_TEST_CASE(find_end_string)
{
    bc::char_ text[] = "sdabababacabskjabacab";
    bc::vector<bc::char_> vectort(text, text + 21, queue);

    bc::char_ pattern[] = "aba";
    bc::vector<bc::char_> vectorp(pattern, pattern + 3, queue);

    bc::vector<bc::char_>::iterator iter =
        bc::find_end(vectort.begin(), vectort.end(),
                     vectorp.begin(), vectorp.end(), queue);

    BOOST_CHECK(iter == (vectort.begin() + 15));
}

BOOST_AUTO_TEST_SUITE_END()
