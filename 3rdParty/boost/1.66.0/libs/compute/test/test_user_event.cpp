//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestUserEvent
#include <boost/test/unit_test.hpp>

#include <boost/compute/user_event.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(empty){}

#ifdef BOOST_COMPUTE_CL_VERSION_1_1
BOOST_AUTO_TEST_CASE(user_event)
{
    REQUIRES_OPENCL_VERSION(1, 1);

    boost::compute::user_event event(context);
    BOOST_CHECK(event.get() != cl_event());
    BOOST_CHECK(event.status() != CL_COMPLETE);

    event.set_status(CL_COMPLETE);
    event.wait();
    BOOST_CHECK(event.status() == CL_COMPLETE);
}
#endif // BOOST_COMPUTE_CL_VERSION_1_1

BOOST_AUTO_TEST_SUITE_END()
