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
#include "Basics/overload.h"

#include <cstddef>
#include <variant>

namespace arangodb {
namespace aql {
struct AqlCall {
  class Infinity {};
  using Limit = std::variant<size_t, Infinity>;

  // TODO Remove me, this will not be necessary later
  static AqlCall SimulateSkipSome(std::size_t toSkip) {
    AqlCall call;
    call.offset = toSkip;
    call.softLimit = 0;
    call.hardLimit = AqlCall::Infinity{};
    call.fullCount = false;
    return call;
  }

  // TODO Remove me, this will not be necessary later
  static AqlCall SimulateGetSome(std::size_t atMost) {
    AqlCall call;
    call.offset = 0;
    call.softLimit = atMost;
    call.hardLimit = AqlCall::Infinity{};
    call.fullCount = false;
    return call;
  }

  // TODO Remove me, this will not be necessary later
  static bool IsSkipSomeCall(AqlCall const& call) {
    return !call.hasHardLimit() && call.getLimit() == 0 && call.getOffset() > 0;
  }

  // TODO Remove me, this will not be necessary later
  static bool IsGetSomeCall(AqlCall const& call) {
    return !call.hasHardLimit() && call.getLimit() > 0 && call.getOffset() == 0;
  }

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

  void didSkip(std::size_t n) {
    TRI_ASSERT(n <= offset);
    offset -= n;
  }

  void didProduce(std::size_t n) {
    auto minus = overload{
        [n](size_t& i) {
          TRI_ASSERT(n <= i);
          i -= n;
        },
        [](auto) {},
    };
    std::visit(minus, softLimit);
    std::visit(minus, hardLimit);
  }

  bool hasHardLimit() const {
    return !std::holds_alternative<AqlCall::Infinity>(hardLimit);
  }

  bool needsFullCount() const { return fullCount; }
};

constexpr bool operator<(AqlCall::Limit const& a, AqlCall::Limit const& b) {
  if (std::holds_alternative<AqlCall::Infinity>(a)) {
    return false;
  }
  if (std::holds_alternative<AqlCall::Infinity>(b)) {
    return true;
  }
  if (std::get<size_t>(a) < std::get<size_t>(b)) {
    return true;
  }
  return false;
}

constexpr AqlCall::Limit operator+(AqlCall::Limit const& a, size_t n) {
  return std::visit(overload{[n](size_t const& i) -> AqlCall::Limit {
                               return i + n;
                             },
                             [](auto inf) -> AqlCall::Limit { return inf; }},
                    a);
}

constexpr AqlCall::Limit operator+(size_t n, AqlCall::Limit const& a) {
  return a + n;
}

}  // namespace aql
}  // namespace arangodb

#endif