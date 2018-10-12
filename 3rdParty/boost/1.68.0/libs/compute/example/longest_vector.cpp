//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <iterator>

#include <boost/compute/algorithm/max_element.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/geometry.hpp>
#include <boost/compute/iterator/transform_iterator.hpp>
#include <boost/compute/types/fundamental.hpp>

namespace compute = boost::compute;

// this example shows how to use the max_element() algorithm along with
// a transform_iterator and the length() function to find the longest
// 4-component vector in an array of vectors
int main()
{
    using compute::float4_;

    // vectors data
    float data[] = { 1.0f, 2.0f, 3.0f, 0.0f,
                     4.0f, 5.0f, 6.0f, 0.0f,
                     7.0f, 8.0f, 9.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 0.0f };

    // create device vector with the vector data
    compute::vector<float4_> vector(
        reinterpret_cast<float4_ *>(data),
        reinterpret_cast<float4_ *>(data) + 4
    );

    // find the longest vector
    compute::vector<float4_>::const_iterator iter =
        compute::max_element(
            compute::make_transform_iterator(
                vector.begin(), compute::length<float4_>()
            ),
            compute::make_transform_iterator(
                vector.end(), compute::length<float4_>()
            )
        ).base();

    // print the index of the longest vector
    std::cout << "longest vector index: "
              << std::distance(vector.begin(), iter)
              << std::endl;

    return 0;
}
