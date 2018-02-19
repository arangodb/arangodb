//---------------------------------------------------------------------------//
// Copyright (c) 2015 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <algorithm>
#include <cstdlib>
#include <iostream>

#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/generate.h>
#include <thrust/host_vector.h>
#include <thrust/reduce.h>

#include "perf.hpp"

int rand_int()
{
    return static_cast<int>((rand() / double(RAND_MAX)) * 25.0);
}

struct unique_key {
  int current;
  int avgValuesNoPerKey;

  unique_key()
  {
      current = 0;
      avgValuesNoPerKey = 512;
  }

  int operator()()
  {
      double p = double(1.0) / static_cast<double>(avgValuesNoPerKey);
      if((rand() / double(RAND_MAX)) <= p)
          return ++current;
      return current;
  }
} UniqueKey;

int main(int argc, char *argv[])
{
    perf_parse_args(argc, argv);

    std::cout << "size: " << PERF_N << std::endl;
    
    // create vector of keys and random values
    thrust::host_vector<int> host_keys(PERF_N);
    thrust::host_vector<int> host_values(PERF_N);
    std::generate(host_keys.begin(), host_keys.end(), UniqueKey);
    std::generate(host_values.begin(), host_values.end(), rand_int);
    
    // transfer data to the device
    thrust::device_vector<int> device_keys = host_keys;
    thrust::device_vector<int> device_values = host_values;

    // create device vectors for the results
    thrust::device_vector<int> device_keys_results(PERF_N);
    thrust::device_vector<int> device_values_results(PERF_N);

    typedef typename thrust::device_vector<int>::iterator iterType;
    thrust::pair<iterType, iterType> result;

    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        result = thrust::reduce_by_key(device_keys.begin(),
                                       device_keys.end(),
                                       device_values.begin(),
                                       device_keys_results.begin(),
                                       device_values_results.begin());
        cudaDeviceSynchronize();
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;
    
    size_t result_size = thrust::distance(device_keys_results.begin(), result.first);
    if(result_size != static_cast<size_t>(host_keys[PERF_N-1] + 1)){
        std::cout << "ERROR: "
                  << "wrong number of keys"
                  << std::endl;
        return -1;
    }

    return 0;
}
