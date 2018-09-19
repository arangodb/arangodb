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
#include <thrust/set_operations.h>
#include <thrust/sort.h>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    thrust::host_vector<int> v1(std::floor(PERF_N / 2.0));
    thrust::host_vector<int> v2(std::ceil(PERF_N / 2.0));
    std::generate(v1.begin(), v1.end(), rand_int);
    std::generate(v2.begin(), v2.end(), rand_int);
    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());

    // transfer data to the device
    thrust::device_vector<int> gpu_v1 = v1;
    thrust::device_vector<int> gpu_v2 = v2;
    thrust::device_vector<int> gpu_v3(PERF_N);

    thrust::device_vector<int>::iterator gpu_v3_end;

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        gpu_v3_end = thrust::set_difference(
            gpu_v1.begin(), gpu_v1.end(),
            gpu_v2.begin(), gpu_v2.end(),
            gpu_v3.begin()
        );
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "size: " << thrust::distance(gpu_v3.begin(), gpu_v3_end) << std::endl;

    return 0;
}
