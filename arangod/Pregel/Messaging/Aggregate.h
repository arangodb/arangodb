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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include "Basics/ResultT.h"
#include "Inspection/VPack.h"

namespace arangodb::pregel {

template<typename T>
concept Addable = requires(T a, T b) {
  {a.add(b)};
};

template<Addable T>
struct Aggregate {
  Aggregate() = default;
  Aggregate(T initialValue, uint64_t countUntilFinished)
      : _countUntilFinished{countUntilFinished},
        _aggregate{std::move(initialValue)} {}
  static auto withComponentsCount(uint64_t countUntilFinished) -> Aggregate {
    return Aggregate{T{}, countUntilFinished};
  }
  auto aggregate(T message) -> std::optional<T> {
    _aggregate.add(message);
    _countUntilFinished--;
    if (_countUntilFinished == 0) {
      return _aggregate;
    }
    return std::nullopt;
  }

 private:
  uint64_t _countUntilFinished;
  T _aggregate;
};

template<typename T>
struct AggregateCount {
  AggregateCount(uint64_t countUntilFinished)
      : _countUntilFinished{countUntilFinished} {}
  auto aggregate(T message) -> bool {
    _countUntilFinished--;
    if (_countUntilFinished == 0) {
      return true;
    }
    return false;
  }

 private:
  uint64_t _countUntilFinished;
};

}  // namespace arangodb::pregel
