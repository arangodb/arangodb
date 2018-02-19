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

#include <bolt/cl/inner_product.h>
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

    // create host vectors
    std::vector<int> host_x = generate_random_vector<int>(PERF_N);
    std::vector<int> host_y = generate_random_vector<int>(PERF_N);

    // create device vectors
    bolt::cl::device_vector<int> device_x(PERF_N);
    bolt::cl::device_vector<int> device_y(PERF_N);

    // transfer data to the device
    bolt::cl::copy(host_x.begin(), host_x.end(), device_x.begin());
    bolt::cl::copy(host_y.begin(), host_y.end(), device_y.begin());

    int product = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        product = bolt::cl::inner_product(
            device_x.begin(), device_x.end(), device_y.begin(), 0
        );
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "product: " << product << std::endl;

    return 0;
}
