//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <algorithm>
#include <cstdlib>
#include <iostream>

#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/generate.h>
#include <thrust/host_vector.h>
#include <thrust/scan.h>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    thrust::host_vector<int> h_vec = generate_random_vector<int>(PERF_N);

    // transfer data to the device
    thrust::device_vector<int> d_vec = h_vec;

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        d_vec = h_vec;

        t.start();
        thrust::inclusive_scan(d_vec.begin(), d_vec.end(), d_vec.begin());
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // transfer data back to host
    thrust::copy(d_vec.begin(), d_vec.end(), h_vec.begin());

    return 0;
}
