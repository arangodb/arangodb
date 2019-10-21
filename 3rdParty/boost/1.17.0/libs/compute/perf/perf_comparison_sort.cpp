//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jakub Szuppe <j.szuppe@gmail.com>
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

#include <boost/program_options.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

namespace po = boost::program_options;
namespace compute = boost::compute;

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);
    std::cout << "size: " << PERF_N << std::endl;

    // setup context and queue for the default device
    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    using boost::compute::int_;

    // create vector of random numbers on the host
    std::vector<int_> host_vector(PERF_N);
    std::generate(host_vector.begin(), host_vector.end(), rand);

    // create vector on the device and copy the data
    boost::compute::vector<int_> device_vector(PERF_N, context);

    // less function for float
    BOOST_COMPUTE_FUNCTION(bool, comp, (int_ a, int_ b),
    {
        return a < b;
    });

    // sort vector
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        boost::compute::copy(
            host_vector.begin(),
            host_vector.end(),
            device_vector.begin(),
            queue
        );
        queue.finish();

        t.start();
        boost::compute::sort(
            device_vector.begin(),
            device_vector.end(),
            comp,
            queue
        );
        queue.finish();
        t.stop();
    };
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // verify vector is sorted
    if(!boost::compute::is_sorted(device_vector.begin(),
                                  device_vector.end(),
                                  comp,
                                  queue)){
        std::cout << "ERROR: is_sorted() returned false" << std::endl;
        return -1;
    }

    return 0;
}
