//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
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

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;

    std::vector<int> v1(std::floor(PERF_N / 2.0));
    std::vector<int> v2(std::ceil(PERF_N / 2.0));

    std::generate(v1.begin(), v1.end(), rand_int);
    std::generate(v2.begin(), v2.end(), rand_int);

    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());

    std::vector<int> v3(PERF_N);
    std::vector<int>::iterator v3_end;

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        v3_end = std::set_union(
            v1.begin(), v1.end(),
            v2.begin(), v2.end(),
            v3.begin()
        );
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "size: " << std::distance(v3.begin(), v3_end) << std::endl;

    return 0;
}
