//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestCountingIterator
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/counting_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(value_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::counting_iterator<int>::value_type,
            int
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::counting_iterator<float>::value_type,
            float
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(distance)
{
    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_counting_iterator(0),
            boost::compute::make_counting_iterator(10)
        ),
        std::ptrdiff_t(10)
    );
    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_counting_iterator(5),
            boost::compute::make_counting_iterator(10)
        ),
        std::ptrdiff_t(5)
    );
    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_counting_iterator(-5),
            boost::compute::make_counting_iterator(5)
        ),
        std::ptrdiff_t(10)
    );
}

BOOST_AUTO_TEST_CASE(copy)
{
    boost::compute::vector<int> vector(10, context);

    boost::compute::copy(
        boost::compute::make_counting_iterator(1),
        boost::compute::make_counting_iterator(11),
        vector.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(
        int, 10, vector,
        (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    );
}

BOOST_AUTO_TEST_CASE(iota_with_copy_doctest)
{
//! [iota_with_copy]
using boost::compute::make_counting_iterator;

boost::compute::vector<int> result(5, context);

boost::compute::copy(
    make_counting_iterator(1), make_counting_iterator(6), result.begin(), queue
);

// result == { 1, 2, 3, 4, 5 }
//! [iota_with_copy]

    CHECK_RANGE_EQUAL(int, 5, result, (1, 2, 3, 4, 5));
}

BOOST_AUTO_TEST_SUITE_END()
