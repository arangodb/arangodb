#include <benchmark/benchmark.h>

#include <frozen/set.h>

#include <set>
#include <array>
#include <algorithm>

static constexpr frozen::set<int, 32> Keywords{
  0, 2, 4, 6, 8, 10, 12, 14,
  16, 18, 20, 22, 24, 26, 28, 30,
  32, 34, 36, 38, 40, 42, 44, 46,
  48, 50, 52, 54, 56, 58, 60, 62
};

static auto const* volatile Some = &Keywords;

static void BM_IntInFzSet(benchmark::State& state) {
  for (auto _ : state) {
    for(auto kw : *Some) {
      volatile bool status = Keywords.count(kw);
      (void) status;
    }
  }
}
BENCHMARK(BM_IntInFzSet);

static const std::set<int> Keywords_(Keywords.begin(), Keywords.end());

static void BM_IntInStdSet(benchmark::State& state) {
  for (auto _ : state) {
    for(auto kw : *Some) {
      volatile bool status = Keywords_.count(kw);
      (void)status;
    }
  }
}

BENCHMARK(BM_IntInStdSet);

static const std::array<int, 32> Keywords__{{
  0, 2, 4, 6, 8, 10, 12, 14,
  16, 18, 20, 22, 24, 26, 28, 30,
  32, 34, 36, 38, 40, 42, 44, 46,
  48, 50, 52, 54, 56, 58, 60, 62
}};
static void BM_IntInStdArray(benchmark::State& state) {
  for (auto _ : state) {
    for(auto kw : *Some) {
      volatile bool status = std::find(Keywords__.begin(), Keywords__.end(), kw) != Keywords__.end();
      (void)status;
    }
  }
}

BENCHMARK(BM_IntInStdArray);

static const int SomeInts[32] = {
  1, 3, 5, 7, 9, 11, 13, 15,
  17, 19, 21, 23, 25, 27, 29, 31,
  33, 35, 37, 39, 41, 43, 45, 47,
  49, 51, 53, 55, 57, 59, 61, 63
};
static auto const * volatile SomeIntsPtr = &SomeInts;

static void BM_IntNotInFzSet(benchmark::State& state) {
  for (auto _ : state) {
    for(auto kw : *SomeIntsPtr) {
      volatile bool status = Keywords.count(kw);
      (void)status;
    }
  }
}
BENCHMARK(BM_IntNotInFzSet);

static void BM_IntNotInStdSet(benchmark::State& state) {
  for (auto _ : state) {
    for(auto kw : *SomeIntsPtr) {
      volatile bool status = Keywords_.count(kw);
      (void)status;
    }
  }
}
BENCHMARK(BM_IntNotInStdSet);

static void BM_IntNotInStdArray(benchmark::State& state) {
  for (auto _ : state) {
    for(auto kw : *SomeIntsPtr) {
      volatile bool status = std::find(Keywords__.begin(), Keywords__.end(), kw) != Keywords__.end();
      (void)status;
    }
  }
}
BENCHMARK(BM_IntNotInStdArray);
