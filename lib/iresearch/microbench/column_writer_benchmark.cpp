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

#include "formats/columnstore2.hpp"

static constexpr size_t kThreads = 1;
static constexpr size_t kColumns = 1000;
static constexpr size_t kColumnsIter = kColumns * 10;

static void WriteFile(std::mt19937_64& /*rng*/) {
  auto writer = irs::columnstore2::make_writer(
    irs::columnstore2::Version::kMax, false, irs::IResourceManager::kNoop);
  // const auto size = size_t{1} << kFileSizePower(rng);
  for (size_t i = 0; i != kColumnsIter; ++i) {
    if (i % kColumns == 0) {
      writer->rollback();
    }
    writer->push_column({}, {});
  }
}

int main() {
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (size_t i = 0; i != kThreads; ++i) {
    threads.emplace_back([i] {
      std::mt19937_64 rng(43 * i);
      WriteFile(rng);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
