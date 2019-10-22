// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <TH1I.h>
#include <TH2I.h>
#include <TH3I.h>
#include <THn.h>
#include <benchmark/benchmark.h>
#include "generator.hpp"

#include <boost/assert.hpp>
struct assert_check {
  assert_check() {
    BOOST_ASSERT(false); // don't run with asserts enabled
  }
} _;

template <class Distribution>
static void fill_1d(benchmark::State& state) {
  TH1I h("", "", 100, 0, 1);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(h.Fill(gen()));
}

template <class Distribution>
static void fill_2d(benchmark::State& state) {
  TH2I h("", "", 100, 0, 1, 100, 0, 1);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(h.Fill(gen(), gen()));
}

template <class Distribution>
static void fill_3d(benchmark::State& state) {
  TH3I h("", "", 100, 0, 1, 100, 0, 1, 100, 0, 1);
  generator<Distribution> gen;
  for (auto _ : state) benchmark::DoNotOptimize(h.Fill(gen(), gen(), gen()));
}

template <class Distribution>
static void fill_6d(benchmark::State& state) {
  int bin[] = {10, 10, 10, 10, 10, 10};
  double min[] = {0, 0, 0, 0, 0, 0};
  double max[] = {1, 1, 1, 1, 1, 1};
  THnI h("", "", 6, bin, min, max);
  generator<Distribution> gen;
  for (auto _ : state) {
    const double buf[6] = {gen(), gen(), gen(), gen(), gen(), gen()};
    benchmark::DoNotOptimize(h.Fill(buf));
  }
}

BENCHMARK_TEMPLATE(fill_1d, uniform);
BENCHMARK_TEMPLATE(fill_2d, uniform);
BENCHMARK_TEMPLATE(fill_3d, uniform);
BENCHMARK_TEMPLATE(fill_6d, uniform);

BENCHMARK_TEMPLATE(fill_1d, normal);
BENCHMARK_TEMPLATE(fill_2d, normal);
BENCHMARK_TEMPLATE(fill_3d, normal);
BENCHMARK_TEMPLATE(fill_6d, normal);
