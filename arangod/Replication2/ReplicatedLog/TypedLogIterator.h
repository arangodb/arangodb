////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <optional>

namespace arangodb::replication2 {

struct LogRange;

template<typename T>
struct TypedLogIterator {
  virtual ~TypedLogIterator() = default;
  // The returned view is guaranteed to stay valid until a successive next()
  // call (only).
  virtual auto next() -> std::optional<T> = 0;
};

template<typename T>
struct TypedLogRangeIterator : TypedLogIterator<T> {
  // returns the index interval [from, to)
  // Note that this does not imply that all indexes in the range [from, to)
  // are returned. Hence (to - from) is only an upper bound on the number of
  // entries returned.
  [[nodiscard]] virtual auto range() const noexcept -> LogRange = 0;
};

}  // namespace arangodb::replication2
