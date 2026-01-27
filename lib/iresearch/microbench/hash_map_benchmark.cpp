////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include <benchmark/benchmark.h>

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>

namespace {

template<typename T>
void fill(T& map, size_t size, int seed) {
  ::srand(seed);
  while (size) {
    const auto kk = rand();
    map.emplace(kk, kk);
    --size;
  }
}

void BM_hash_std(benchmark::State& state) {
  int seed = 41;

  std::unordered_map<uint32_t, uint64_t> map;
  fill(map, state.range(0), seed);

  for (auto _ : state) {
    for (int i = 0; i < 100; ++i) {
      srand(seed);
      for (int64_t i = 0; i < state.range(0); ++i) {
        auto it = map.find(rand());
        if (it == map.end()) {
          std::abort();
        }
        benchmark::DoNotOptimize(it);
      }
    }
  }
}

void BM_hash_absl_flat(benchmark::State& state) {
  int seed = 41;

  absl::flat_hash_map<uint32_t, uint64_t> map;
  fill(map, state.range(0), seed);

  for (auto _ : state) {
    for (int i = 0; i < 100; ++i) {
      srand(seed);
      for (int64_t i = 0; i < state.range(0); ++i) {
        auto it = map.find(rand());
        if (it == map.end()) {
          std::abort();
        }
        benchmark::DoNotOptimize(it);
      }
    }
  }
}

void BM_hash_absl_node(benchmark::State& state) {
  int seed = 41;

  absl::node_hash_map<uint32_t, uint64_t> map;
  fill(map, state.range(0), seed);

  for (auto _ : state) {
    for (int i = 0; i < 100; ++i) {
      srand(seed);
      for (int64_t i = 0; i < state.range(0); ++i) {
        auto it = map.find(rand());
        if (it == map.end()) {
          std::abort();
        }
        benchmark::DoNotOptimize(it);
      }
    }
  }
}

BENCHMARK(BM_hash_absl_flat)->RangeMultiplier(2)->Range(0, 100);
BENCHMARK(BM_hash_absl_node)->RangeMultiplier(2)->Range(0, 100);
BENCHMARK(BM_hash_std)->RangeMultiplier(2)->Range(0, 100);

}  // namespace
