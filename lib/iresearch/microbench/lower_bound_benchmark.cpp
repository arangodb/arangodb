////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#include <numeric>

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>

namespace {

void BM_lower_bound(benchmark::State& state) {
  std::vector<uint32_t> nums(state.range(0));
  std::iota(nums.begin(), nums.end(), 42);

  for (auto _ : state) {
    for (auto v : nums) {
      auto it = std::upper_bound(nums.begin(), nums.end(), v);
      benchmark::DoNotOptimize(it);
    }
  }
}

BENCHMARK(BM_lower_bound)->DenseRange(0, 256, 8);

void BM_linear_scan(benchmark::State& state) {
  std::vector<uint32_t> nums(state.range(0));
  std::iota(nums.begin(), nums.end(), 42);

  for (auto _ : state) {
    for (auto v : nums) {
      auto it = std::find(nums.begin(), nums.end(), v);
      benchmark::DoNotOptimize(it);
    }
  }
}

BENCHMARK(BM_linear_scan)->DenseRange(0, 256, 8);

}  // namespace
