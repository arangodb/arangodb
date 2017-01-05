//---------------------------------------------------------------------------//
// Copyright (c) 2014 Benoit
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
#include <boost/compute/algorithm/exclusive_scan.hpp>
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

    // create vector of random numbers on the host
    std::vector<int> host_vector(PERF_N);
    std::generate(host_vector.begin(), host_vector.end(), rand_int);

    // create vector on the device and copy the data
    boost::compute::vector<int> device_vector(PERF_N, context);
    boost::compute::vector<int> device_res(PERF_N,context);
    boost::compute::copy(
        host_vector.begin(),
        host_vector.end(),
        device_vector.begin(),
        queue
    );

    // sum vector
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        boost::compute::copy(
            host_vector.begin(),
            host_vector.end(),
            device_vector.begin(),
            queue
        );

        t.start();
        boost::compute::exclusive_scan(
            device_vector.begin(),
            device_vector.end(),
            device_res.begin(),
            queue
        );
        queue.finish();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // verify sum is correct
    std::partial_sum(
        host_vector.begin(),
        host_vector.end(),
        host_vector.begin()
    );

    int device_sum = device_res.back();
    // when scan is exclusive values are shifted by one on the left
    // compared to a inclusive scan
    int host_sum = host_vector[host_vector.size()-2];

    if(device_sum != host_sum){
        std::cout << "ERROR: "
                  << "device_sum (" << device_sum << ") "
                  << "!= "
                  << "host_sum (" << host_sum << ")"
                  << std::endl;
        return -1;
    }

    return 0;
}
