//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionalPopcount
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/popcount.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

typedef boost::mpl::list<
    compute::uchar_, compute::ushort_, compute::uint_, compute::ulong_
> popcount_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(popcount, T, popcount_test_types)
{
    compute::vector<T> vec(context);
    vec.push_back(0x00, queue);
    vec.push_back(0x01, queue);
    vec.push_back(0x03, queue);
    vec.push_back(0xff, queue);

    compute::vector<int> popcounts(vec.size(), context);
    compute::transform(
        vec.begin(), vec.end(), popcounts.begin(), compute::popcount<T>(), queue
    );
    CHECK_RANGE_EQUAL(int, 4, popcounts, (0, 1, 2, 8));
}

BOOST_AUTO_TEST_SUITE_END()
