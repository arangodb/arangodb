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

#include <bolt/cl/copy.h>
#include <bolt/cl/device_vector.h>
#include <bolt/cl/reduce.h>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    bolt::cl::control ctrl = bolt::cl::control::getDefault();
    ::cl::Device device = ctrl.getDevice();
    std::cout << "device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

    // create host vector
    std::vector<int> host_vec = generate_random_vector<int>(PERF_N);

    // create device vectors
    bolt::cl::device_vector<int> device_vec(PERF_N);

    // transfer data to the device
    bolt::cl::copy(host_vec.begin(), host_vec.end(), device_vec.begin());

    int sum = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        sum = bolt::cl::reduce(device_vec.begin(), device_vec.end());
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "sum: " << sum << std::endl;

    return 0;
}
