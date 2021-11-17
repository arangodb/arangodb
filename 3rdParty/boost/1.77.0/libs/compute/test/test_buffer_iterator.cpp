//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestBufferIterator
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/reverse.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(value_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            compute::buffer_iterator<int>::value_type, int
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::is_same<
            compute::buffer_iterator<float>::value_type, float
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(reverse_external_buffer_doctest)
{
    int data[] = { 1, 2, 3 };
    compute::buffer buffer(context, 3 * sizeof(int));
    queue.enqueue_write_buffer(buffer, 0, 3 * sizeof(int), data);

    cl_mem external_mem_obj = buffer.get();

//! [reverse_external_buffer]
// create a buffer object wrapping the cl_mem object
boost::compute::buffer buf(external_mem_obj);

// reverse the values in the buffer
boost::compute::reverse(
    boost::compute::make_buffer_iterator<int>(buf, 0),
    boost::compute::make_buffer_iterator<int>(buf, 3),
    queue
);
//! [reverse_external_buffer]

    queue.enqueue_read_buffer(buf, 0, 3 * sizeof(int), data);
    BOOST_CHECK_EQUAL(data[0], 3);
    BOOST_CHECK_EQUAL(data[1], 2);
    BOOST_CHECK_EQUAL(data[2], 1);
}

BOOST_AUTO_TEST_SUITE_END()
