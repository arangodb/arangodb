// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/indexed.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/mp11/integral.hpp>
#include <vector>
#include "../test/throw_exception.hpp"

#include <boost/assert.hpp>
struct assert_check {
  assert_check() {
    BOOST_ASSERT(false); // don't run with asserts enabled
  }
} _;

using namespace boost::histogram;
using namespace boost::histogram::literals;

struct tuple {};
struct vector {};
struct vector_of_variant {};

template <unsigned I>
using Dim_t = boost::mp11::mp_int<I>;

using d1 = Dim_t<1>;
using d2 = Dim_t<2>;
using d3 = Dim_t<3>;

auto make_histogram(tuple, d1, unsigned n) {
  return make_histogram_with(std::vector<unsigned>(), axis::integer<>(0, n));
}

auto make_histogram(tuple, d2, unsigned n) {
  return make_histogram_with(std::vector<unsigned>(), axis::integer<>(0, n),
                             axis::integer<>(0, n));
}

auto make_histogram(tuple, d3, unsigned n) {
  return make_histogram_with(std::vector<unsigned>(), axis::integer<>(0, n),
                             axis::integer<>(0, n), axis::integer<>(0, n));
}

template <int Dim>
auto make_histogram(vector, boost::mp11::mp_int<Dim>, unsigned n) {
  std::vector<axis::integer<>> axes;
  for (unsigned d = 0; d < Dim; ++d) axes.emplace_back(axis::integer<>(0, n));
  return make_histogram_with(std::vector<unsigned>(), std::move(axes));
}

template <int Dim>
auto make_histogram(vector_of_variant, boost::mp11::mp_int<Dim>, unsigned n) {
  std::vector<axis::variant<axis::integer<>>> axes;
  for (unsigned d = 0; d < Dim; ++d) axes.emplace_back(axis::integer<>(0, n));
  return make_histogram_with(std::vector<unsigned>(), std::move(axes));
}

template <class Tag>
static void Naive(benchmark::State& state, Tag, d1, coverage cov) {
  auto h = make_histogram(Tag(), d1(), state.range(0));
  const int d = cov == coverage::all;
  for (auto _ : state) {
    for (int i = -d; i < h.axis().size() + d; ++i) {
      benchmark::DoNotOptimize(i);
      benchmark::DoNotOptimize(h.at(i));
    }
  }
}

template <class Tag>
static void Naive(benchmark::State& state, Tag, d2, coverage cov) {
  auto h = make_histogram(Tag(), d2(), state.range(0));
  const int d = cov == coverage::all;
  for (auto _ : state) {
    for (int i = -d; i < h.axis(0_c).size() + d; ++i) {
      for (int j = -d; j < h.axis(1_c).size() + d; ++j) {
        benchmark::DoNotOptimize(i);
        benchmark::DoNotOptimize(j);
        benchmark::DoNotOptimize(h.at(i, j));
      }
    }
  }
}

template <class Tag>
static void Naive(benchmark::State& state, Tag, d3, coverage cov) {
  auto h = make_histogram(Tag(), d3(), state.range(0));
  const int d = cov == coverage::all;
  for (auto _ : state) {
    for (int i = -d; i < h.axis(0_c).size() + d; ++i) {
      for (int j = -d; j < h.axis(1_c).size() + d; ++j) {
        for (int k = -d; k < h.axis(2_c).size() + d; ++k) {
          benchmark::DoNotOptimize(i);
          benchmark::DoNotOptimize(j);
          benchmark::DoNotOptimize(k);
          benchmark::DoNotOptimize(h.at(i, j, k));
        }
      }
    }
  }
}

template <class Tag>
static void Indexed(benchmark::State& state, Tag, d1, coverage cov) {
  auto h = make_histogram(Tag(), d1(), state.range(0));
  for (auto _ : state) {
    for (auto&& x : indexed(h, cov)) {
      benchmark::DoNotOptimize(*x);
      benchmark::DoNotOptimize(x.index());
    }
  }
}

template <class Tag>
static void Indexed(benchmark::State& state, Tag, d2, coverage cov) {
  auto h = make_histogram(Tag(), d2(), state.range(0));
  for (auto _ : state) {
    for (auto&& x : indexed(h, cov)) {
      benchmark::DoNotOptimize(*x);
      benchmark::DoNotOptimize(x.index(0));
      benchmark::DoNotOptimize(x.index(1));
    }
  }
}

template <class Tag>
static void Indexed(benchmark::State& state, Tag, d3, coverage cov) {
  auto h = make_histogram(Tag(), d3(), state.range(0));
  for (auto _ : state) {
    for (auto&& x : indexed(h, cov)) {
      benchmark::DoNotOptimize(*x);
      benchmark::DoNotOptimize(x.index(0));
      benchmark::DoNotOptimize(x.index(1));
      benchmark::DoNotOptimize(x.index(2));
    }
  }
}

#define BENCH(Type, Tag, Dim, Cov)                                             \
  BENCHMARK_CAPTURE(Type, (Tag, Dim, Cov), Tag{}, Dim_t<Dim>{}, coverage::Cov) \
      ->RangeMultiplier(4)                                                     \
      ->Range(4, 256)

BENCH(Naive, tuple, 1, inner);
BENCH(Indexed, tuple, 1, inner);

BENCH(Naive, vector, 1, inner);
BENCH(Indexed, vector, 1, inner);

BENCH(Naive, vector_of_variant, 1, inner);
BENCH(Indexed, vector_of_variant, 1, inner);

BENCH(Naive, tuple, 2, inner);
BENCH(Indexed, tuple, 2, inner);

BENCH(Naive, vector, 2, inner);
BENCH(Indexed, vector, 2, inner);

BENCH(Naive, vector_of_variant, 2, inner);
BENCH(Indexed, vector_of_variant, 2, inner);

BENCH(Naive, tuple, 3, inner);
BENCH(Indexed, tuple, 3, inner);

BENCH(Naive, vector, 3, inner);
BENCH(Indexed, vector, 3, inner);

BENCH(Naive, vector_of_variant, 3, inner);
BENCH(Indexed, vector_of_variant, 3, inner);
