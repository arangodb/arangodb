//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAsyncWaitGuard
#include <boost/test/unit_test.hpp>

#include <boost/compute/async/future.hpp>
#include <boost/compute/async/wait_guard.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

static size_t wait_num = 0;

struct waitable_object
{
    void wait()
    {
        wait_num++;
    }
};

BOOST_AUTO_TEST_CASE(wait_guard_test)
{
    waitable_object waitable;
    
    BOOST_CHECK(wait_num == 0);
    {
        compute::wait_guard<waitable_object> waitable_object_guard(waitable);
    }
    BOOST_CHECK(wait_num == 1);
    {
        compute::wait_guard<waitable_object> waitable_object_guard1(waitable);
        compute::wait_guard<waitable_object> waitable_object_guard2(waitable);
    }
    BOOST_CHECK(wait_num == 3);    
}

BOOST_AUTO_TEST_SUITE_END()
