//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestBufferAllocator
#include <boost/test/unit_test.hpp>

#include <boost/compute/allocator/buffer_allocator.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(allocate)
{
    compute::buffer_allocator<int> allocator(context);

    typedef compute::buffer_allocator<int>::pointer pointer;
    pointer x = allocator.allocate(10);
    allocator.deallocate(x, 10);
}

BOOST_AUTO_TEST_SUITE_END()
