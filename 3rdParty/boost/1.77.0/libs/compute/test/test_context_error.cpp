//---------------------------------------------------------------------------//
// Copyright (c) 2014 Fabian Köhler <fabian2804@googlemail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestContextError
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/exception/context_error.hpp>

BOOST_AUTO_TEST_CASE(what)
{
    boost::compute::context context = boost::compute::system::default_context();
    boost::compute::context_error error(&context, "Test", 0, 0);
    BOOST_CHECK_EQUAL(std::string(error.what()), std::string("Test"));
    BOOST_CHECK(*error.get_context_ptr() == context);
    BOOST_CHECK(error.get_private_info_ptr() == 0);
    BOOST_CHECK(error.get_private_info_size() == 0);
}
