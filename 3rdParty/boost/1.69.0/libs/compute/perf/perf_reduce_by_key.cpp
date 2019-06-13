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
#include <iostream>
#include <numeric>
#include <vector>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/reduce_by_key.hpp>
#include <boost/compute/container/vector.hpp>

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

    // setup context and queue for the default device
    boost::compute::device device = boost::compute::system::default_device();
    boost::compute::context context(device);
    boost::compute::command_queue queue(context, device);
    std::cout << "device: " << device.name() << std::endl;

    // create vector of keys and random values
    std::vector<int> host_keys(PERF_N);
    std::vector<int> host_values(PERF_N);
    std::generate(host_keys.begin(), host_keys.end(), UniqueKey);
    std::generate(host_values.begin(), host_values.end(), rand_int);

    // create vectors for keys and values on the device and copy the data
    boost::compute::vector<int> device_keys(PERF_N, context);
    boost::compute::vector<int> device_values(PERF_N,context);
    boost::compute::copy(
        host_keys.begin(),
        host_keys.end(),
        device_keys.begin(),
        queue
    );
    boost::compute::copy(
        host_values.begin(),
        host_values.end(),
        device_values.begin(),
        queue
    );

    // vectors for the results
    boost::compute::vector<int> device_keys_results(PERF_N, context);
    boost::compute::vector<int> device_values_results(PERF_N,context);

    typedef boost::compute::vector<int>::iterator iterType;
    std::pair<iterType, iterType> result(
        device_keys_results.begin(),
        device_values_results.begin()
    );

    // reduce by key
    perf_timer t;
    for(size_t trial = 0; trial < PERF_TRIALS; trial++){
        t.start();
        result = boost::compute::reduce_by_key(device_keys.begin(),
                                               device_keys.end(),
                                               device_values.begin(),
                                               device_keys_results.begin(),
                                               device_values_results.begin(),
                                               queue);
        t.stop();
    }
    std::cout << "time: " << t.min_time() / 1e6 << " ms" << std::endl;

    size_t result_size = std::distance(device_keys_results.begin(), result.first);
    if(result_size != static_cast<size_t>(host_keys[PERF_N-1] + 1)){
        std::cout << "ERROR: "
                  << "wrong number of keys" << result_size << "\n" << (host_keys[PERF_N-1] + 1)
                  << std::endl;
        return -1;
    }

    return 0;
}
