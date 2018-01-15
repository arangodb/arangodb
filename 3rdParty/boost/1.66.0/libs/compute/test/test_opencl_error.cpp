//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestOpenCLError
#include <boost/test/unit_test.hpp>

#include <boost/compute/exception/opencl_error.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(error_to_string)
{
    using boost::compute::opencl_error;

    BOOST_CHECK_EQUAL(opencl_error::to_string(CL_SUCCESS), "Success");
    BOOST_CHECK_EQUAL(opencl_error::to_string(CL_INVALID_VALUE), "Invalid Value");
    BOOST_CHECK_EQUAL(opencl_error::to_string(-123456), "Unknown OpenCL Error (-123456)");
}

BOOST_AUTO_TEST_CASE(error_code)
{
    boost::compute::opencl_error e(CL_INVALID_DEVICE);
    BOOST_CHECK_EQUAL(e.error_code(), CL_INVALID_DEVICE);
    BOOST_CHECK_EQUAL(e.error_string(), "Invalid Device");
}

BOOST_AUTO_TEST_SUITE_END()
