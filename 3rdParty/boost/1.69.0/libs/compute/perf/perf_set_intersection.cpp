//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/set_intersection.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);
    std::cout << "size: " << PERF_N << std::endl;

    // setup context and queue for the default device
    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    // create vectors of random numbers on the host
    std::vector<int> v1(std::floor(PERF_N / 2.0));
    std::vector<int> v2(std::ceil(PERF_N / 2.0));
    std::generate(v1.begin(), v1.end(), rand_int);
    std::generate(v2.begin(), v2.end(), rand_int);
    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());

    // create vectors on the device and copy the data
    boost::compute::vector<int> gpu_v1(std::floor(PERF_N / 2.0), context);
    boost::compute::vector<int> gpu_v2(std::ceil(PERF_N / 2.0), context);

    boost::compute::copy(
        v1.begin(), v1.end(), gpu_v1.begin(), queue
    );
    boost::compute::copy(
        v2.begin(), v2.end(), gpu_v2.begin(), queue
    );

    boost::compute::vector<int> gpu_v3(PERF_N, context);
    boost::compute::vector<int>::iterator gpu_v3_end;

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        gpu_v3_end = boost::compute::set_intersection(
            gpu_v1.begin(), gpu_v1.end(),
            gpu_v2.begin(), gpu_v2.end(),
            gpu_v3.begin(), queue
        );
        queue.finish();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "size: " << std::distance(gpu_v3.begin(), gpu_v3_end) << std::endl;

    return 0;
}
