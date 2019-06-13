//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Rastko Anicic <anicic.rastko@gmail.com>
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

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/max_element.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>(rand() % 10000000);
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
    boost::compute::copy(
        host_vector.begin(),
        host_vector.end(),
        device_vector.begin(),
        queue
    );

    boost::compute::vector<int>::iterator device_max_iter
        = device_vector.begin();

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        device_max_iter = boost::compute::max_element(
            device_vector.begin(), device_vector.end(), queue
        );
        queue.finish();
        t.stop();
    }

    int device_max = device_max_iter.read(queue);
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "max: " << device_max << std::endl;

    // verify max is correct
    std::vector<int>::iterator host_max_iter
        = std::max_element(host_vector.begin(), host_vector.end());

    int host_max = *host_max_iter;
    if(device_max != host_max){
         std::cout << "ERROR: "
                   << "device_max (" << device_max << ") "
                   << "!= "
                   << "host_max (" << host_max << ")"
                   << std::endl;
         return -1;
    }

    size_t host_max_idx = std::distance(host_vector.begin(), host_max_iter);
    size_t device_max_idx = std::distance(device_vector.begin(), device_max_iter);
    if(device_max_idx != host_max_idx){
        std::cout << "ERROR: "
                  << "device_max index (" << device_max_idx << ") "
                  << "!= "
                  << "host_max index (" << host_max_idx << ")"
                  << std::endl;
        return -1;
    }

    return 0;
}
