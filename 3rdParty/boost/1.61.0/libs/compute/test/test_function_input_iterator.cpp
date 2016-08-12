//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionInputIterator
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/function_input_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(generate_42_doctest)
{
    boost::compute::vector<int> result(4, context);

//! [generate_42]
BOOST_COMPUTE_FUNCTION(int, ret42, (),
{
    return 42;
});

boost::compute::copy(
    boost::compute::make_function_input_iterator(ret42, 0),
    boost::compute::make_function_input_iterator(ret42, result.size()),
    result.begin(),
    queue
);

// result == { 42, 42, 42, 42 }
//! [generate_42]

    CHECK_RANGE_EQUAL(int, 4, result, (42, 42, 42, 42));
}

BOOST_AUTO_TEST_SUITE_END()
