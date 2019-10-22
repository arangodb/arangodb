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
#include <iterator>
#include <algorithm>

#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/inner_product.h>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    thrust::host_vector<int> host_x(PERF_N);
    thrust::host_vector<int> host_y(PERF_N);
    std::generate(host_x.begin(), host_x.end(), rand);
    std::generate(host_y.begin(), host_y.end(), rand);

    // transfer data to the device
    thrust::device_vector<int> device_x = host_x;
    thrust::device_vector<int> device_y = host_y;

    int product = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        product = thrust::inner_product(
            device_x.begin(), device_x.end(), device_y.begin(), 0
        );
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "product: " << product << std::endl;

    return 0;
}
