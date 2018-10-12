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
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/random/default_random_engine.hpp>
#include <boost/compute/random/discrete_distribution.hpp>

#include "perf.hpp"

namespace compute = boost::compute;

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);
    std::cout << "size: " << PERF_N << std::endl;

    compute::device device = compute::system::default_device();
    compute::context context(device);
    compute::command_queue queue(context, device);

    compute::vector<compute::uint_> vector(PERF_N, context);

    int weights[] = {1, 1};

    compute::default_random_engine rng(queue);
    compute::discrete_distribution<compute::uint_> dist(weights, weights+2);

    perf_timer t;
    t.start();
    dist.generate(vector.begin(), vector.end(), rng, queue);
    queue.finish();
    t.stop();
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
