//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPermutationIterator
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/types.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>
#include <boost/compute/iterator/permutation_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(value_type)
{
    using boost::compute::float4_;

    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::permutation_iterator<
                boost::compute::buffer_iterator<float>,
                boost::compute::buffer_iterator<int>
            >::value_type,
            float
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::permutation_iterator<
                boost::compute::buffer_iterator<float4_>,
                boost::compute::buffer_iterator<short>
            >::value_type,
            float4_
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(base_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::permutation_iterator<
                boost::compute::buffer_iterator<int>,
                boost::compute::buffer_iterator<int>
            >::base_type,
            boost::compute::buffer_iterator<int>
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(copy)
{
    int input_data[] = { 3, 4, 2, 1, 5 };
    boost::compute::vector<int> input(input_data, input_data + 5, queue);

    int map_data[] = { 3, 2, 0, 1, 4 };
    boost::compute::vector<int> map(map_data, map_data + 5, queue);

    boost::compute::vector<int> output(5, context);
    boost::compute::copy(
        boost::compute::make_permutation_iterator(input.begin(), map.begin()),
        boost::compute::make_permutation_iterator(input.end(), map.end()),
        output.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 5, output, (1, 2, 3, 4, 5));
}

BOOST_AUTO_TEST_CASE(reverse_range_doctest)
{
    int values_data[] = { 10, 20, 30, 40 };
    int indices_data[] = { 3, 2, 1, 0 };

    boost::compute::vector<int> values(values_data, values_data + 4, queue);
    boost::compute::vector<int> indices(indices_data, indices_data + 4, queue);

    boost::compute::vector<int> result(4, context);

//! [reverse_range]
// values =  { 10, 20, 30, 40 }
// indices = { 3, 2, 1, 0 }

boost::compute::copy(
    boost::compute::make_permutation_iterator(values.begin(), indices.begin()),
    boost::compute::make_permutation_iterator(values.end(), indices.end()),
    result.begin(),
    queue
);

// result == { 40, 30, 20, 10 }
//! [reverse_range]

    CHECK_RANGE_EQUAL(int, 4, result, (40, 30, 20, 10));
}

BOOST_AUTO_TEST_SUITE_END()
