//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestDiscardIterator
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/copy_if.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/discard_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(value_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::discard_iterator::value_type, void
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(distance)
{
    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_discard_iterator(0),
            boost::compute::make_discard_iterator(10)
        ),
        std::ptrdiff_t(10)
    );
    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_discard_iterator(5),
            boost::compute::make_discard_iterator(10)
        ),
        std::ptrdiff_t(5)
    );
}

BOOST_AUTO_TEST_CASE(discard_copy)
{
    boost::compute::vector<int> vector(10, context);
    boost::compute::fill(vector.begin(), vector.end(), 42, queue);

    boost::compute::copy(
        vector.begin(), vector.end(),
        boost::compute::make_discard_iterator(),
        queue
    );
}

BOOST_AUTO_TEST_CASE(discard_copy_if)
{
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    boost::compute::vector<int> vector(data, data + 8, queue);

    using boost::compute::lambda::_1;

    boost::compute::discard_iterator end = boost::compute::copy_if(
        vector.begin(), vector.end(),
        boost::compute::make_discard_iterator(),
        _1 > 4,
        queue
    );
    BOOST_CHECK(std::distance(boost::compute::discard_iterator(), end) == 4);
}

BOOST_AUTO_TEST_CASE(discard_fill)
{
    boost::compute::fill(
        boost::compute::make_discard_iterator(0),
        boost::compute::make_discard_iterator(100),
        42,
        queue
    );
}

BOOST_AUTO_TEST_SUITE_END()
