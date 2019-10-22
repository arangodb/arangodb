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
#include <numeric>
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/inner_product.hpp>
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

    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    std::vector<int> h1(PERF_N);
    std::vector<int> h2(PERF_N);
    std::generate(h1.begin(), h1.end(), rand_int);
    std::generate(h2.begin(), h2.end(), rand_int);

    // create vector on the device and copy the data
    boost::compute::vector<int> d1(PERF_N, context);
    boost::compute::vector<int> d2(PERF_N, context);
    boost::compute::copy(h1.begin(), h1.end(), d1.begin(), queue);
    boost::compute::copy(h2.begin(), h2.end(), d2.begin(), queue);

    int product = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        product = boost::compute::inner_product(
            d1.begin(), d1.end(), d2.begin(), int(0), queue
        );
        queue.finish();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    // verify product is correct
    int host_product = std::inner_product(
        h1.begin(), h1.end(), h2.begin(), int(0)
    );
    if(product != host_product){
        std::cout << "ERROR: "
                  << "device_product (" << product << ") "
                  << "!= "
                  << "host_product (" << host_product << ")"
                  << std::endl;
        return -1;
    }

    return 0;
}
