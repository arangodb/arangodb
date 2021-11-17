//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionalUnpack
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/zip_iterator.hpp>
#include <boost/compute/functional/detail/unpack.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(plus_int)
{
    int data1[] = { 1, 3, 5, 7 };
    int data2[] = { 2, 4, 6, 8 };

    compute::vector<int> input1(4, context);
    compute::vector<int> input2(4, context);

    compute::copy_n(data1, 4, input1.begin(), queue);
    compute::copy_n(data2, 4, input2.begin(), queue);

    compute::vector<int> output(4, context);
    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(input1.begin(), input2.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(input1.end(), input2.end())
        ),
        output.begin(),
        compute::detail::unpack(compute::plus<int>()),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (3, 7, 11, 15));
}

BOOST_AUTO_TEST_CASE(fma_float)
{
    float data1[] = { 1, 3, 5, 7 };
    float data2[] = { 2, 4, 6, 8 };
    float data3[] = { 0, 9, 1, 2 };

    compute::vector<float> input1(4, context);
    compute::vector<float> input2(4, context);
    compute::vector<float> input3(4, context);

    compute::copy_n(data1, 4, input1.begin(), queue);
    compute::copy_n(data2, 4, input2.begin(), queue);
    compute::copy_n(data3, 4, input3.begin(), queue);

    compute::vector<int> output(4, context);
    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(input1.begin(), input2.begin(), input3.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(input1.end(), input2.end(), input3.begin())
        ),
        output.begin(),
        compute::detail::unpack(compute::fma<float>()),
        queue
    );
}

BOOST_AUTO_TEST_CASE(subtract_int2)
{
    using compute::int2_;

    int data[] = { 4, 2, 5, 1, 6, 3, 7, 0 };
    compute::vector<int2_> input(4, context);
    compute::copy_n(reinterpret_cast<int2_ *>(data), 4, input.begin(), queue);

    compute::vector<int> output(4, context);
    compute::transform(
        input.begin(),
        input.end(),
        output.begin(),
        compute::detail::unpack(compute::minus<int>()),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (2, 4, 3, 7));
}

BOOST_AUTO_TEST_SUITE_END()
