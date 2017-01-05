//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPipe
#include <boost/test/unit_test.hpp>

#include <boost/compute/core.hpp>
#include <boost/compute/pipe.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(empty)
{
}

#ifdef CL_VERSION_2_0
BOOST_AUTO_TEST_CASE(create_pipe)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    compute::pipe pipe(context, 16 * sizeof(float), 128);
    BOOST_CHECK_EQUAL(pipe.get_info<CL_PIPE_PACKET_SIZE>(), 64);
    BOOST_CHECK_EQUAL(pipe.get_info<CL_PIPE_MAX_PACKETS>(), 128);
}
#endif // CL_VERSION_2_0

BOOST_AUTO_TEST_SUITE_END()
