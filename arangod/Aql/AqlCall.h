////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_CALL_H
#define ARANGOD_AQL_AQL_CALL_H 1

#include "Aql/ExecutionBlock.h"

#include <cstddef>
#include <variant>

namespace arangodb {
namespace aql {
struct AqlCall {
  class Infinity {};
  using Limit = std::variant<size_t, Infinity>;

  std::size_t offset{0};
  // TODO: The defaultBatchSize function could move into this file instead
  Limit softLimit{Infinity{}};
  Limit hardLimit{Infinity{}};
  bool fullCount{false};

  std::size_t getOffset() const { return offset; }

  std::size_t getLimit() const {
    // By default we use batchsize
    std::size_t limit = ExecutionBlock::DefaultBatchSize();
    // We are not allowed to go above softLimit
    if (std::holds_alternative<std::size_t>(softLimit)) {
      limit = (std::min)(std::get<std::size_t>(softLimit), limit);
    }
    // We are not allowed to go above hardLimit
    if (std::holds_alternative<std::size_t>(hardLimit)) {
      limit = (std::min)(std::get<std::size_t>(hardLimit), limit);
    }
    return limit;
  }

  void didProduce(std::size_t n) {
    if (std::holds_alternative<std::size_t>(softLimit)) {
      TRI_ASSERT(n <= std::get<std::size_t>(softLimit));
      softLimit = std::get<std::size_t>(softLimit) - n;
    }
    if (std::holds_alternative<std::size_t>(hardLimit)) {
      TRI_ASSERT(n <= std::get<std::size_t>(hardLimit));
      hardLimit = std::get<std::size_t>(hardLimit) - n;
    }
  }

  bool hasHardLimit() const {
    return !std::holds_alternative<AqlCall::Infinity>(hardLimit);
  }

  bool needsFullCount() const { return fullCount; }
};

}  // namespace aql
}  // namespace arangodb

#endif