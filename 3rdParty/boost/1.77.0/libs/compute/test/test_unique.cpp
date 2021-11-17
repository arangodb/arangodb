//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestUnique
#include <boost/test/unit_test.hpp>

#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/none_of.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/algorithm/unique.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(unique_int)
{
    int data[] = {1, 6, 6, 4, 2, 2, 4};

    compute::vector<int> input(data, data + 7, queue);

    compute::vector<int>::iterator iter =
        compute::unique(input.begin(), input.end(), queue);

    BOOST_VERIFY(iter == input.begin() + 5);
    CHECK_RANGE_EQUAL(int, 7, input, (1, 6, 4, 2, 4, 2, 4));
}

BOOST_AUTO_TEST_CASE(all_same_float)
{
    compute::vector<float> vec(1024, context);
    compute::fill(vec.begin(), vec.end(), 3.14f, queue);

    compute::vector<float>::iterator iter =
        compute::unique(vec.begin(), vec.end(), queue);

    BOOST_VERIFY(iter == vec.begin() + 1);

    float first;
    compute::copy_n(vec.begin(), 1, &first, queue);
    BOOST_CHECK_EQUAL(first, 3.14f);
}

BOOST_AUTO_TEST_CASE(unique_even_uints)
{
    using compute::uint_;

    // create vector filled with [0, 1, 2, ...]
    compute::vector<uint_> vec(1024, context);
    compute::iota(vec.begin(), vec.end(), 0, queue);

    // all should be unique
    compute::vector<uint_>::iterator iter = compute::unique(
        vec.begin(), vec.end(), queue
    );
    BOOST_VERIFY(iter == vec.end());

    // if odd, return the prior even number, else return the number
    BOOST_COMPUTE_FUNCTION(uint_, odd_to_even, (uint_ x),
    {
        if(x & 1){
            return x - 1;
        }
        else {
            return x;
        }
    });

    // set all odd numbers the previous even number
    compute::transform(
        vec.begin(), vec.end(), vec.begin(), odd_to_even, queue
    );

    // now the vector should contain [0, 0, 2, 2, 4, 4, ...]
    iter = compute::unique(vec.begin(), vec.end(), queue);
    BOOST_VERIFY(iter == vec.begin() + (vec.size() / 2));

    // ensure all of the values are even
    BOOST_COMPUTE_FUNCTION(bool, is_odd, (uint_ x),
    {
        return x & 1;
    });
    BOOST_VERIFY(compute::none_of(vec.begin(), vec.end(), is_odd, queue));
}

BOOST_AUTO_TEST_SUITE_END()
