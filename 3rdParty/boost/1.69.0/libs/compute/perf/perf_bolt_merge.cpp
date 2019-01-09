//---------------------------------------------------------------------------//
// Copyright (c) 2015 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <algorithm>
#include <vector>

#include <bolt/cl/merge.h>
#include <bolt/cl/copy.h>
#include <bolt/cl/device_vector.h>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    bolt::cl::control ctrl = bolt::cl::control::getDefault();
    ::cl::Device device = ctrl.getDevice();
    std::cout << "device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    
    // create vector of random numbers on the host
    std::vector<int> host_vec1 = generate_random_vector<int>(std::floor(PERF_N / 2.0));
    std::vector<int> host_vec2 = generate_random_vector<int>(std::ceil(PERF_N / 2.0));
    // sort them
    std::sort(host_vec1.begin(), host_vec1.end());
    std::sort(host_vec2.begin(), host_vec2.end());

    // create device vectors
    bolt::cl::device_vector<int> device_vec1(PERF_N);
    bolt::cl::device_vector<int> device_vec2(PERF_N);
    bolt::cl::device_vector<int> device_vec3(PERF_N);
    
    // transfer data to the device
    bolt::cl::copy(host_vec1.begin(), host_vec1.end(), device_vec1.begin());
    bolt::cl::copy(host_vec2.begin(), host_vec2.end(), device_vec2.begin());

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        bolt::cl::merge(
            device_vec1.begin(), device_vec1.end(),
            device_vec2.begin(), device_vec2.end(),
            device_vec3.begin()
        );
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
