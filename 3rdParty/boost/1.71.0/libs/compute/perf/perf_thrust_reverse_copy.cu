//---------------------------------------------------------------------------//
// Copyright (c) 2015 Jakub Szuppe <j.szuppe@gmail.com>
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
#include <thrust/reverse.h>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    thrust::host_vector<int> h_vec = generate_random_vector<int>(PERF_N);

	// transfer data to the device
    thrust::device_vector<int> d_vec;   
    d_vec = h_vec;
    
    // device vector for reversed data
    thrust::device_vector<int> d_reversed_vec(PERF_N);
    
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        thrust::reverse_copy(d_vec.begin(), d_vec.end(), d_reversed_vec.begin());
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
