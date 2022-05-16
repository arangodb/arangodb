// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <gsl/gsl_histogram.h>
#include <gsl/gsl_histogram2d.h>
#include "../test/throw_exception.hpp"
#include "generator.hpp"

#include <cassert>
struct assert_check {
  assert_check() {
    assert(false); // don't run with asserts enabled
  }
} _;

template <class Distribution>
static void fill_1d(benchmark::State& state) {
  gsl_histogram* h = gsl_histogram_alloc(100);
  gsl_histogram_set_ranges_uniform(h, 0, 1);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(gsl_histogram_increment(h, gen()));
  gsl_histogram_free(h);
  state.SetItemsProcessed(state.iterations());
}

template <class Distribution>
static void fill_2d(benchmark::State& state) {
  gsl_histogram2d* h = gsl_histogram2d_alloc(100, 100);
  gsl_histogram2d_set_ranges_uniform(h, 0, 1, 0, 1);
  generator<Distribution> gen;
  for (auto _ : state)
    benchmark::DoNotOptimize(gsl_histogram2d_increment(h, gen(), gen()));
  gsl_histogram2d_free(h);
  state.SetItemsProcessed(state.iterations() * 2);
}

BENCHMARK_TEMPLATE(fill_1d, uniform);
BENCHMARK_TEMPLATE(fill_2d, uniform);

BENCHMARK_TEMPLATE(fill_1d, normal);
BENCHMARK_TEMPLATE(fill_2d, normal);
