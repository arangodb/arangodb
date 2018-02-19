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
#include <thrust/unique.h>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    thrust::host_vector<int> h_vec(PERF_N);
    std::generate(h_vec.begin(), h_vec.end(), rand_int);

    thrust::device_vector<int> d_vec(PERF_N);

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        d_vec = h_vec;

        t.start();
        thrust::unique(d_vec.begin(), d_vec.end());
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
