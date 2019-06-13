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

#include "perf.hpp"

float rand_float()
{
    return (float(rand()) / float(RAND_MAX)) * 1000.f;
}

// y <- alpha * x + y
void serial_saxpy(size_t n, float alpha, const float *x, float *y)
{
    for(size_t i = 0; i < n; i++){
        y[i] = alpha * x[i] + y[i];
    }
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    float alpha = 2.5f;

    std::vector<float> host_x(PERF_N);
    std::vector<float> host_y(PERF_N);
    std::generate(host_x.begin(), host_x.end(), rand_float);
    std::generate(host_y.begin(), host_y.end(), rand_float);

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        serial_saxpy(PERF_N, alpha, &host_x[0], &host_y[0]);
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
