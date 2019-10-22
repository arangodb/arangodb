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

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

bool less_than_10(int value)
{
    return value < 10;
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);
    std::cout << "size: " << PERF_N << std::endl;

    // create vector of random numbers on the host
    std::vector<int> host_vector(PERF_N);
    std::generate(host_vector.begin(), host_vector.end(), rand_int);

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        std::stable_partition(host_vector.begin(), host_vector.end(),
                                less_than_10);
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
