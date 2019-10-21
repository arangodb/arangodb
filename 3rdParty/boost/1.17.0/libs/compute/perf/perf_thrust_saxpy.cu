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
#include <thrust/functional.h>
#include <thrust/host_vector.h>
#include <thrust/transform.h>

#include "perf.hpp"

struct saxpy_functor : public thrust::binary_function<float,float,float>
{
    const float a;

    saxpy_functor(float _a) : a(_a) {}

    __host__ __device__
    float operator()(const float& x, const float& y) const
    {
        return a * x + y;
    }
};

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

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        thrust::transform(device_x.begin(), device_x.end(), device_y.begin(), device_y.begin(), saxpy_functor(2.5f));
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // transfer data back to host
    thrust::copy(device_x.begin(), device_x.end(), host_x.begin());
    thrust::copy(device_y.begin(), device_y.end(), host_y.begin());

    return 0;
}
