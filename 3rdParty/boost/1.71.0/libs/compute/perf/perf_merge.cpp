//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/merge.hpp>
#include <boost/compute/container/vector.hpp>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    std::vector<int> v1 = generate_random_vector<int>(std::floor(PERF_N / 2.0));
    std::vector<int> v2 = generate_random_vector<int>(std::ceil(PERF_N / 2.0));
    std::vector<int> v3(PERF_N);

    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());

    boost::compute::vector<int> gpu_v1(v1.begin(), v1.end(), queue);
    boost::compute::vector<int> gpu_v2(v2.begin(), v2.end(), queue);
    boost::compute::vector<int> gpu_v3(PERF_N, context);

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        boost::compute::merge(gpu_v1.begin(), gpu_v1.end(),
                              gpu_v2.begin(), gpu_v2.end(),
                              gpu_v3.begin(),
                              queue
        );
        queue.finish();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    std::vector<int> check_v3(PERF_N);
    boost::compute::copy(gpu_v3.begin(), gpu_v3.end(), check_v3.begin(), queue);
    queue.finish();

    std::merge(v1.begin(), v1.end(), v2.begin(), v2.end(), v3.begin());
    bool ok = std::equal(check_v3.begin(), check_v3.end(), v3.begin());
    if(!ok){
        std::cerr << "ERROR: merged ranges different" << std::endl;
        return -1;
    }

    return 0;
}
