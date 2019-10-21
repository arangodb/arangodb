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

#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

template<class T>
struct Sum {
    T value;
    Sum() : value(0) {}
    Sum( Sum& s, tbb::split ) {value = 0;}
    void operator()( const tbb::blocked_range<T*>& r ) {
        T temp = value;
        for( T* a=r.begin(); a!=r.end(); ++a ) {
            temp += *a;
        }
        value = temp;
    }
    void join( Sum& rhs ) {value += rhs.value;}
};

template<class T>
T ParallelSum( T array[], size_t n ) {
    Sum<T> total;
    tbb::parallel_reduce( tbb::blocked_range<T*>( array, array+n ),
                     total );
    return total.value;
}

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);
    std::cout << "size: " << PERF_N << std::endl;

    // create vector of random numbers on the host
    std::vector<int> host_vector(PERF_N);
    std::generate(host_vector.begin(), host_vector.end(), rand_int);

    int sum = 0;
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        sum = ParallelSum<int>(&host_vector[0], host_vector.size());
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    std::cout << "sum: " << sum << std::endl;

    int host_sum = std::accumulate(host_vector.begin(), host_vector.end(), int(0));
    if(sum != host_sum){
        std::cerr << "ERROR: sum (" << sum << ") != (" << host_sum << ")" << std::endl;
        return -1;
    }

    return 0;
}
