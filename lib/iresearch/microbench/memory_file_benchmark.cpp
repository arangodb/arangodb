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

#include <random>
#include <thread>
#include <vector>

#include "store/memory_directory.hpp"

static constexpr size_t kThreads = 8;
static constexpr size_t kFiles = 1000;
std::uniform_int_distribution<size_t> kFileSizePower{8, 24};

static void WriteFile(std::mt19937_64& rng) {
  irs::memory_file file{irs::IResourceManager::kNoop};
  irs::memory_index_output output{file};
  const auto size = size_t{1} << kFileSizePower(rng);
  for (size_t i = 0; i != size; ++i) {
    output.write_byte(42);
  }
}

int main() {
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (size_t i = 0; i != kThreads; ++i) {
    threads.emplace_back([i] {
      std::mt19937_64 rng(43 * i);
      for (size_t i = 0; i != kFiles; ++i) {
        WriteFile(rng);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
