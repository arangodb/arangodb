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
/// @author Valerii Mironov
////////////////////////////////////////////////////////////////////////////////

#include <benchmark/benchmark.h>

#include <string>

#include "utils/crc.hpp"

#include <absl/crc/crc32c.h>

namespace {

std::string TestString(size_t len) {
  std::string result;
  result.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    result.push_back(static_cast<char>(i % 256));
  }
  return result;
}

void BM_Calculate(benchmark::State& state) {
  int len = state.range(0);
  std::string data = TestString(len);
  for (auto s : state) {
    benchmark::DoNotOptimize(data);
    absl::crc32c_t crc = absl::ComputeCrc32c(data);
    benchmark::DoNotOptimize(crc);
  }
}

BENCHMARK(BM_Calculate)->Arg(0)->Arg(1)->Arg(100)->Arg(10000)->Arg(500000);

void BM_Calculate2(benchmark::State& state) {
  int len = state.range(0);
  std::string data = TestString(len);
  for (auto s : state) {
    benchmark::DoNotOptimize(data);
    irs::crc32c crc;
    crc.process_bytes(data.data(), data.size());
    auto tmp = crc.checksum();
    benchmark::DoNotOptimize(tmp);
  }
}

BENCHMARK(BM_Calculate2)->Arg(0)->Arg(1)->Arg(100)->Arg(10000)->Arg(500000);

void BM_Extend(benchmark::State& state) {
  int len = state.range(0);
  std::string extension = TestString(len);
  absl::crc32c_t base{0xC99465AA};  // CRC32C of "Hello World"
  for (auto s : state) {
    benchmark::DoNotOptimize(base);
    benchmark::DoNotOptimize(extension);
    absl::crc32c_t crc = absl::ExtendCrc32c(base, extension);
    benchmark::DoNotOptimize(crc);
  }
}

void BM_Extend2(benchmark::State& state) {
  int len = state.range(0);
  std::string extension = TestString(len);
  irs::crc32c base{0xC99465AA};  // CRC32C of "Hello World"
  for (auto s : state) {
    benchmark::DoNotOptimize(base);
    benchmark::DoNotOptimize(extension);
    base.process_bytes(extension.data(), extension.size());
    auto tmp = base.checksum();
    benchmark::DoNotOptimize(tmp);
  }
}

BENCHMARK(BM_Extend)->Arg(0)->Arg(1)->Arg(100)->Arg(10000)->Arg(500000);
BENCHMARK(BM_Extend2)->Arg(0)->Arg(1)->Arg(100)->Arg(10000)->Arg(500000);

}  // namespace
