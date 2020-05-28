// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <boost/histogram/axis.hpp>
#include <numeric>
#include "../test/throw_exception.hpp"
#include "generator.hpp"

#include <boost/assert.hpp>
struct assert_check {
  assert_check() {
    BOOST_ASSERT(false); // don't run with asserts enabled
  }
} _;

using namespace boost::histogram;

template <class Distribution>
static void regular(benchmark::State& state) {
  auto a = axis::regular<>(100, 0.0, 1.0);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(a.index(gen()));
}

template <class Distribution>
static void circular(benchmark::State& state) {
  auto a = axis::circular<>(100, 0.0, 1.0);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(a.index(gen()));
}

template <class T, class Distribution>
static void integer(benchmark::State& state) {
  auto a = axis::integer<T>(0, 1);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(a.index(gen()));
}

template <class Distribution>
static void variable(benchmark::State& state) {
  std::vector<double> v;
  for (double x = 0; x <= state.range(0); ++x) { v.push_back(x / state.range(0)); }
  auto a = axis::variable<>(v);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(a.index(gen()));
}

static void category(benchmark::State& state) {
  std::vector<int> v(state.range(0));
  std::iota(v.begin(), v.end(), 0);
  auto a = axis::category<int>(v);
  generator<uniform_int> gen(static_cast<int>(state.range(0)));
  for (auto _ : state) benchmark::DoNotOptimize(a.index(gen()));
}

BENCHMARK_TEMPLATE(regular, uniform);
BENCHMARK_TEMPLATE(regular, normal);
BENCHMARK_TEMPLATE(circular, uniform);
BENCHMARK_TEMPLATE(circular, normal);
BENCHMARK_TEMPLATE(integer, int, uniform);
BENCHMARK_TEMPLATE(integer, int, normal);
BENCHMARK_TEMPLATE(integer, double, uniform);
BENCHMARK_TEMPLATE(integer, double, normal);
BENCHMARK_TEMPLATE(variable, uniform)->RangeMultiplier(10)->Range(10, 10000);
BENCHMARK_TEMPLATE(variable, normal)->RangeMultiplier(10)->Range(10, 10000);
BENCHMARK(category)->RangeMultiplier(10)->Range(10, 10000);
