//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestMultipleObjects
#include <boost/test/unit_test.hpp>
#include <boost/compute.hpp>

bool dummy_function();

BOOST_AUTO_TEST_CASE(multiple_objects)
{
    // It is enough if the test compiles.
    BOOST_CHECK( dummy_function() );
}
