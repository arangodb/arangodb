//---------------------------------------------------------------------------//
// Copyright (c) 2014 Fabian Köhler <fabian2804@googlemail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//
#define BOOST_TEST_MODULE TestUnsupportedExtension
#include <boost/test/unit_test.hpp>
#include <boost/compute/exception/unsupported_extension_error.hpp>

BOOST_AUTO_TEST_CASE(unsupported_extension_error_what)
{
    boost::compute::unsupported_extension_error error("CL_DUMMY_EXTENSION");
    BOOST_CHECK_EQUAL(std::string(error.what()), std::string("OpenCL extension CL_DUMMY_EXTENSION not supported"));
}
