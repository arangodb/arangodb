////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Transaction/OperationOrigin.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

namespace arangodb {
class TransactionState;

namespace velocypack {
class Builder;
}

namespace transaction {

class HistoryEntry {
  friend class History;

  HistoryEntry(HistoryEntry const&) = delete;
  HistoryEntry& operator=(HistoryEntry const&) = delete;

 public:
  HistoryEntry(std::string databaseName, std::vector<std::string> collections,
               OperationOrigin operationOrigin);

  void toVelocyPack(velocypack::Builder& result) const;

  void adjustMemoryUsage(std::int64_t value) noexcept;

 private:
  std::mutex mutable _mutex;

  std::string const _databaseName;
  std::vector<std::string> const _collections;
  OperationOrigin const _operationOrigin;

  std::uint64_t _id;
  std::atomic<std::uint64_t> _memoryUsage;
  std::atomic<std::uint64_t> _peakMemoryUsage;
};

class History {
  History(History const&) = delete;
  History& operator=(History const&) = delete;

 public:
  explicit History(std::size_t maxSize);
  ~History();

  void insert(TransactionState& state);
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
#endif
