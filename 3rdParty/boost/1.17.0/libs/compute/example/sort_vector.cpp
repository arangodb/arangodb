//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <algorithm>
#include <iostream>
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/container/vector.hpp>

namespace compute = boost::compute;

int rand_int()
{
    return rand() % 100;
}

// this example demonstrates how to sort a vector of ints on the GPU
int main()
{
    // create vector of random values on the host
    std::vector<int> host_vector(10);
    std::generate(host_vector.begin(), host_vector.end(), rand_int);

    // print out input vector
    std::cout << "input:  [ ";
    for(size_t i = 0; i < host_vector.size(); i++){
        std::cout << host_vector[i];

        if(i != host_vector.size() - 1){
            std::cout << ", ";
        }
    }
    std::cout << " ]" << std::endl;

    // transfer the values to the device
    compute::vector<int> device_vector = host_vector;

    // sort the values on the device
    compute::sort(device_vector.begin(), device_vector.end());

    // transfer the values back to the host
    compute::copy(device_vector.begin(),
                  device_vector.end(),
                  host_vector.begin());

    // print out the sorted vector
    std::cout << "output: [ ";
    for(size_t i = 0; i < host_vector.size(); i++){
        std::cout << host_vector[i];

        if(i != host_vector.size() - 1){
            std::cout << ", ";
        }
    }
    std::cout << " ]" << std::endl;

    return 0;
}
