//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestNoDeviceFound
#include <boost/test/unit_test.hpp>

#include <boost/compute/exception/no_device_found.hpp>

void throw_no_device_found()
{
    throw boost::compute::no_device_found();
}

BOOST_AUTO_TEST_CASE(what)
{
    try {
        throw_no_device_found();

        BOOST_REQUIRE(false); // should not get here
    }
    catch(boost::compute::no_device_found& e){
        BOOST_CHECK_EQUAL(std::string(e.what()), "No OpenCL device found");
    }
}
