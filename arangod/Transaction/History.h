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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <shared_mutex>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace transaction {
class HistoryEntry;
class Methods;

class History {
  History(History const&) = delete;
  History& operator=(History const&) = delete;

 public:
  explicit History(std::size_t maxSize);
  ~History();

  void insert(Methods const& trx);
  void toVelocyPack(velocypack::Builder& result) const;

  void garbageCollect() noexcept;
  void clear() noexcept;

 private:
  std::size_t const _maxSize;

  std::shared_mutex mutable _mutex;
  std::deque<std::shared_ptr<HistoryEntry>> _history;
  std::uint64_t _id;
};

}  // namespace transaction
}  // namespace arangodb
