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

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/operator.hpp>

namespace compute = boost::compute;

// this example demonstrates how to use Boost.Compute's STL
// implementation to add two vectors on the GPU
int main()
{
    // setup input arrays
    float a[] = { 1, 2, 3, 4 };
    float b[] = { 5, 6, 7, 8 };

    // make space for the output
    float c[] = { 0, 0, 0, 0 };

    // create vectors and transfer data for the input arrays 'a' and 'b'
    compute::vector<float> vector_a(a, a + 4);
    compute::vector<float> vector_b(b, b + 4);

    // create vector for the output array
    compute::vector<float> vector_c(4);

    // add the vectors together
    compute::transform(
        vector_a.begin(),
        vector_a.end(),
        vector_b.begin(),
        vector_c.begin(),
        compute::plus<float>()
    );

    // transfer results back to the host array 'c'
    compute::copy(vector_c.begin(), vector_c.end(), c);

    // print out results in 'c'
    std::cout << "c: [" << c[0] << ", "
                        << c[1] << ", "
                        << c[2] << ", "
                        << c[3] << "]" << std::endl;

    return 0;
}
