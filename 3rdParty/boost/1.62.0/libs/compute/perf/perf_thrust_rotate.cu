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
#include <thrust/host_vector.h>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    thrust::host_vector<int> h_vec = generate_random_vector<int>(PERF_N);

    // transfer data to the device
    thrust::device_vector<int> d_vec;

    size_t rotate_distance = PERF_N / 2;

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        d_vec = h_vec;

        t.start();
        // there is no thrust::rotate() so we implement it manually with copy()
        thrust::device_vector<int> tmp(d_vec.begin(), d_vec.begin() + rotate_distance);
        thrust::copy(d_vec.begin() + rotate_distance, d_vec.end(), d_vec.begin());
        thrust::copy(tmp.begin(), tmp.end(), d_vec.begin() + rotate_distance);
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // transfer data back to host
    thrust::copy(d_vec.begin(), d_vec.end(), h_vec.begin());

    return 0;
}
