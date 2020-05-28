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
#include <iostream>
#include <vector>

#include <thrust/count.h>
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);
    std::cout << "size: " << PERF_N << std::endl;

    // create vector of random numbers on the host
    thrust::host_vector<int> host_vector(PERF_N);
    thrust::generate(host_vector.begin(), host_vector.end(), rand_int);

    thrust::device_vector<int> v = host_vector;

    size_t count = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        count = thrust::count(v.begin(), v.end(), 4);
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "count: " << count << std::endl;

    return 0;
}
