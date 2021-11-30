//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPartitionPoint
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/partition_point.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/system.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(partition_point_int)
{
    int dataset[] = {1, 1, 5, 2, 4, -2, 0, -1, 0, -1};
    bc::vector<bc::int_> vector(dataset, dataset + 10, queue);

    bc::vector<bc::int_>::iterator iter =
        bc::partition_point(vector.begin(), vector.begin() + 10,
                             bc::_1 > 0, queue);

    BOOST_VERIFY(iter == vector.begin()+5);
}

BOOST_AUTO_TEST_SUITE_END()
