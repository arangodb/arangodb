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

#include <bolt/cl/count.h>
#include <bolt/cl/copy.h>
#include <bolt/cl/device_vector.h>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    bolt::cl::control ctrl = bolt::cl::control::getDefault();
    ::cl::Device device = ctrl.getDevice();
    std::cout << "device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

    // create vector of random numbers on the host
    std::vector<int> h_vec(PERF_N);
    std::generate(h_vec.begin(), h_vec.end(), rand_int);

    // create device vector
    bolt::cl::device_vector<int> d_vec(PERF_N);

    // transfer data to the device
    bolt::cl::copy(h_vec.begin(), h_vec.end(), d_vec.begin());

    size_t count = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        count = bolt::cl::count(ctrl, d_vec.begin(), d_vec.end(), 4);
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "count: " << count << std::endl;

    return 0;
}
