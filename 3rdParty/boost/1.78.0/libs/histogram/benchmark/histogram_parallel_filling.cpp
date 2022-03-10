// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <boost/histogram/accumulators/count.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <chrono>
#include <functional>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <vector>
#include "../test/throw_exception.hpp"

#include <cassert>
struct assert_check {
  assert_check() {
    assert(false); // don't run with asserts enabled
  }
} _;

using namespace boost::histogram;
using namespace std::chrono_literals;

using DS = dense_storage<unsigned>;
using DSTS = dense_storage<accumulators::count<unsigned, true>>;

static void NoThreads(benchmark::State& state) {
  std::default_random_engine gen(1);
  std::uniform_real_distribution<> dis(0, 1);
  const unsigned nbins = state.range(0);
  auto hist = make_histogram_with(DS(), axis::regular<>(nbins, 0, 1));
  for (auto _ : state) {
    // simulate some work
    for (volatile unsigned n = 0; n < state.range(1); ++n)
      ;

    hist(dis(gen));
  }
}

std::mutex init;
static auto hist = make_histogram_with(DSTS(), axis::regular<>());

static void AtomicStorage(benchmark::State& state) {
  init.lock();
  if (state.thread_index == 0) {
    const unsigned nbins = state.range(0);
    hist = make_histogram_with(DSTS(), axis::regular<>(nbins, 0, 1));
  }
  init.unlock();
  std::default_random_engine gen(state.thread_index);
  std::uniform_real_distribution<> dis(0, 1);
  for (auto _ : state) {
    // simulate some work
    for (volatile unsigned n = 0; n < state.range(1); ++n)
      ;
    hist(dis(gen));
  }
}

BENCHMARK(NoThreads)
    ->UseRealTime()

    ->Args({1 << 4, 0})
    ->Args({1 << 6, 0})
    ->Args({1 << 8, 0})
    ->Args({1 << 10, 0})
    ->Args({1 << 14, 0})
    ->Args({1 << 18, 0})

    ->Args({1 << 4, 5})
    ->Args({1 << 6, 5})
    ->Args({1 << 8, 5})
    ->Args({1 << 10, 5})
    ->Args({1 << 14, 5})
    ->Args({1 << 18, 5})

    ->Args({1 << 4, 10})
    ->Args({1 << 6, 10})
    ->Args({1 << 8, 10})
    ->Args({1 << 10, 10})
    ->Args({1 << 14, 10})
    ->Args({1 << 18, 10})

    ->Args({1 << 4, 50})
    ->Args({1 << 6, 50})
    ->Args({1 << 8, 50})
    ->Args({1 << 10, 50})
    ->Args({1 << 14, 50})
    ->Args({1 << 18, 50})

    ->Args({1 << 4, 100})
    ->Args({1 << 6, 100})
    ->Args({1 << 8, 100})
    ->Args({1 << 10, 100})
    ->Args({1 << 14, 100})
    ->Args({1 << 18, 100})

    ;

BENCHMARK(AtomicStorage)
    ->UseRealTime()
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)

    ->Args({1 << 4, 0})
    ->Args({1 << 6, 0})
    ->Args({1 << 8, 0})
    ->Args({1 << 10, 0})
    ->Args({1 << 14, 0})
    ->Args({1 << 18, 0})

    ->Args({1 << 4, 5})
    ->Args({1 << 6, 5})
    ->Args({1 << 8, 5})
    ->Args({1 << 10, 5})
    ->Args({1 << 14, 5})
    ->Args({1 << 18, 5})

    ->Args({1 << 4, 10})
    ->Args({1 << 6, 10})
    ->Args({1 << 8, 10})
    ->Args({1 << 10, 10})
    ->Args({1 << 14, 10})
    ->Args({1 << 18, 10})

    ->Args({1 << 4, 50})
    ->Args({1 << 6, 50})
    ->Args({1 << 8, 50})
    ->Args({1 << 10, 50})
    ->Args({1 << 14, 50})
    ->Args({1 << 18, 50})

    ->Args({1 << 4, 100})
    ->Args({1 << 6, 100})
    ->Args({1 << 8, 100})
    ->Args({1 << 10, 100})
    ->Args({1 << 14, 100})
    ->Args({1 << 18, 100})

    ;
