//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPinnedAllocator
#include <boost/test/unit_test.hpp>

#include <boost/compute/allocator/pinned_allocator.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(vector_with_pinned_allocator)
{
    compute::vector<int, compute::pinned_allocator<int> > vector(context);
    vector.push_back(12, queue);
}

BOOST_AUTO_TEST_SUITE_END()
