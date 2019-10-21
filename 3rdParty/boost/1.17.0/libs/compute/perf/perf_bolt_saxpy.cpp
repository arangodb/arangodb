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

#include <bolt/cl/copy.h>
#include <bolt/cl/device_vector.h>
#include <bolt/cl/transform.h>

#include "perf.hpp"

BOLT_FUNCTOR(saxpy_functor,
    struct saxpy_functor
    {
        float _a;
        saxpy_functor(float a) : _a(a) {};

        float operator() (const float &x, const float &y) const
        {
            return _a * x + y;
        };
    };
)

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    bolt::cl::control ctrl = bolt::cl::control::getDefault();
    ::cl::Device device = ctrl.getDevice();
    std::cout << "device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

    // create host vectors
    std::vector<float> host_x(PERF_N);
    std::vector<float> host_y(PERF_N);
    std::generate(host_x.begin(), host_x.end(), rand);
    std::generate(host_y.begin(), host_y.end(), rand);

    // create device vectors
    bolt::cl::device_vector<float> device_x(PERF_N);
    bolt::cl::device_vector<float> device_y(PERF_N);

    // transfer data to the device
    bolt::cl::copy(host_x.begin(), host_x.end(), device_x.begin());
    bolt::cl::copy(host_y.begin(), host_y.end(), device_y.begin());

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        bolt::cl::transform(
            device_x.begin(), device_x.end(),
            device_y.begin(),
            device_y.begin(),
            saxpy_functor(2.5f)
        );
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // transfer data back to host
    bolt::cl::copy(device_x.begin(), device_x.end(), host_x.begin());
    bolt::cl::copy(device_y.begin(), device_y.end(), host_y.begin());

    return 0;
}
