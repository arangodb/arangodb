//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/container/vector.hpp>

namespace compute = boost::compute;

// this example demonstrates how to print the values in a vector
int main()
{
    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    std::cout << "device: " << gpu.name() << std::endl;

    // create vector on the device and fill with the sequence 1..10
    compute::vector<int> vector(10, context);
    compute::iota(vector.begin(), vector.end(), 1, queue);

//[print_vector_example
    std::cout << "vector: [ ";
    boost::compute::copy(
        vector.begin(), vector.end(),
        std::ostream_iterator<int>(std::cout, ", "),
        queue
    );
    std::cout << "]" << std::endl;
//]

    return 0;
}
