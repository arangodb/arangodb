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

#include <boost/timer/timer.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

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
    std::vector<int> random_vector(PERF_N);
    std::generate(random_vector.begin(), random_vector.end(), rand);

    // create input vector for gpu
    std::vector<int> gpu_vector = random_vector;

    // sort vector on gpu
    boost::timer::cpu_timer t;
    boost::compute::sort(
        gpu_vector.begin(), gpu_vector.end(), queue
    );
    queue.finish();
    std::cout << "time: " << t.elapsed().wall / 1e6 << " ms" << std::endl;

    // create input vector for host
    std::vector<int> host_vector = random_vector;

    // sort vector on host
    t.start();
    std::sort(host_vector.begin(), host_vector.end());
    std::cout << "host time: " << t.elapsed().wall / 1e6 << " ms" << std::endl;

    // ensure that both sorted vectors are equal
    if(!std::equal(gpu_vector.begin(), gpu_vector.end(), host_vector.begin())){
        std::cerr << "ERROR: sorted vectors not the same" << std::endl;
        return -1;
    }

    return 0;
}
