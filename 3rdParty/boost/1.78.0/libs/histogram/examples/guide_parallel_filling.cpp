// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_parallel_filling

#include <boost/histogram.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <cassert>
#include <functional>
#include <thread>
#include <vector>

// dummy fill function, to be executed in parallel by several threads
template <typename Histogram>
void fill(Histogram& h) {
  for (unsigned i = 0; i < 1000; ++i) { h(i % 10); }
}

int main() {
  using namespace boost::histogram;

  /*
    Create histogram with container of thread-safe counters for parallel filling in
    several threads. Only filling is thread-safe, other guarantees are not given.
  */
  auto h = make_histogram_with(dense_storage<accumulators::count<unsigned, true>>(),
                               axis::integer<>(0, 10));

  /*
    Run the fill function in parallel from different threads. This is safe when a
    thread-safe accumulator and a storage with thread-safe cell access are used.
  */
  auto fill_h = [&h]() { fill(h); };
  std::thread t1(fill_h);
  std::thread t2(fill_h);
  std::thread t3(fill_h);
  std::thread t4(fill_h);
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  // Without a thread-safe accumulator, this number may be smaller.
  assert(algorithm::sum(h) == 4000);
}

//]
