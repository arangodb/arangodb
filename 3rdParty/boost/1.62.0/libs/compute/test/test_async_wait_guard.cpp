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

BOOST_AUTO_TEST_CASE(wait_for_fill)
{
    compute::vector<int> vector(8192, context);

    compute::event fill_event =
        compute::fill_async(vector.begin(), vector.end(), 9, queue).get_event();

    BOOST_CHECK(fill_event.status() != CL_COMPLETE);

    {
        compute::wait_guard<compute::event> fill_guard(fill_event);
    }

    BOOST_CHECK(fill_event.status() == CL_COMPLETE);
}

BOOST_AUTO_TEST_SUITE_END()
