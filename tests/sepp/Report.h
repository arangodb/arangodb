////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include <velocypack/Builder.h>

namespace arangodb::sepp {

struct ThreadReport {
  // a velocypack value containing arbitrary result data for this thread
  velocypack::Builder data;
  // total number of operations performed by this thread
  std::uint64_t operations;
};

struct Report {
  std::int64_t timestamp;
  // TODO - rocksdb statistics
  // velocypack::SliceContainer config;

  std::vector<ThreadReport> threads;
  double runtime;  // runtime in milliseconds
  std::uint64_t databaseSize;

  [[nodiscard]] std::uint64_t operations() const {
    std::uint64_t result = 0;
    for (const auto& thread : threads) {
      result += thread.operations;
    }
    return result;
  }

  [[nodiscard]] double throughput() const {
    return static_cast<double>(operations()) / runtime;
  }
};

}  // namespace arangodb::sepp
