//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <vector>
#include <algorithm>
#include <iostream>

#include "perf.hpp"

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    std::vector<int> v1 = generate_random_vector<int>(std::floor(PERF_N / 2.0));
    std::vector<int> v2 = generate_random_vector<int>(std::ceil(PERF_N / 2.0));
    std::vector<int> v3(PERF_N);

    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        std::merge(v1.begin(), v1.end(), v2.begin(), v2.end(), v3.begin());
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    return 0;
}
