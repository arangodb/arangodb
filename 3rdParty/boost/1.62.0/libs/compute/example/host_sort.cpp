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

#include <boost/spirit/include/karma.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort.hpp>

namespace compute = boost::compute;
namespace karma = boost::spirit::karma;

int rand_int()
{
    return rand() % 100;
}

// this example demonstrates how to sort a std::vector of ints on the GPU
int main()
{
    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    std::cout << "device: " << gpu.name() << std::endl;

    // create vector of random values on the host
    std::vector<int> vector(8);
    std::generate(vector.begin(), vector.end(), rand_int);

    // print input vector
    std::cout << "input:  [ "
              << karma::format(karma::int_ % ", ", vector)
              << " ]"
              << std::endl;

    // sort vector
    compute::sort(vector.begin(), vector.end(), queue);

    // print sorted vector
    std::cout << "output: [ "
              << karma::format(karma::int_ % ", ", vector)
              << " ]"
              << std::endl;

    return 0;
}
