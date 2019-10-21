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

#include <bolt/cl/scan.h>
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
    std::vector<int> h_vec = generate_random_vector<int>(PERF_N);

    // create device vector
    bolt::cl::device_vector<int> d_vec(PERF_N);

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        // transfer data to the device
        bolt::cl::copy(h_vec.begin(), h_vec.end(), d_vec.begin());

        t.start();
        bolt::cl::inclusive_scan(d_vec.begin(), d_vec.end(), d_vec.begin());
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // transfer data back to host
    bolt::cl::copy(d_vec.begin(), d_vec.end(), h_vec.begin());

    return 0;
}

