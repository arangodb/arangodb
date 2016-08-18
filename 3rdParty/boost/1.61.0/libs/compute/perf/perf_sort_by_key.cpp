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

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort_by_key.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    using boost::compute::int_;
    using boost::compute::long_;

    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    // setup context and queue for the default device
    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    // create vector of random numbers on the host
    std::vector<int> host_keys(PERF_N);
    std::generate(host_keys.begin(), host_keys.end(), rand);
    std::vector<long> host_values(PERF_N);
    std::copy(host_keys.begin(), host_keys.end(), host_values.begin());

    // create vector on the device and copy the data
    boost::compute::vector<int_> device_keys(PERF_N, context);
    boost::compute::vector<long_> device_values(PERF_N, context);

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        boost::compute::copy(
            host_keys.begin(), host_keys.end(), device_keys.begin(), queue
        );
        boost::compute::copy(
            host_values.begin(), host_values.end(), device_values.begin(), queue
        );

        t.start();
        // sort vector
        boost::compute::sort_by_key(
            device_keys.begin(), device_keys.end(), device_values.begin(), queue
        );
        queue.finish();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // verify keys are sorted
    if(!boost::compute::is_sorted(device_keys.begin(), device_keys.end(), queue)){
        std::cout << "ERROR: is_sorted() returned false for the keys" << std::endl;
        return -1;
    }
    // verify values are sorted
    if(!boost::compute::is_sorted(device_values.begin(), device_values.end(), queue)){
        std::cout << "ERROR: is_sorted() returned false for the values" << std::endl;
        return -1;
    }

    return 0;
}
