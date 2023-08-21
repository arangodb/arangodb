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

#include "Transaction/History.h"

#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

#include <mutex>
#include <string>
#include <vector>

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
namespace arangodb::transaction {

HistoryEntry::HistoryEntry(std::string databaseName,
                           std::vector<std::string> collections,
                           OperationOrigin operationOrigin)
    : _databaseName(std::move(databaseName)),
      _collections(std::move(collections)),
      _operationOrigin(operationOrigin),
      _id(0),
      _memoryUsage(0),
      _peakMemoryUsage(0) {}

void HistoryEntry::toVelocyPack(velocypack::Builder& result) const {
  std::unique_lock<std::mutex> lock(_mutex);

  result.openObject();
  result.add("id", VPackValue(_id));
  result.add("database", VPackValue(_databaseName));

  result.add("collections", VPackValue(VPackValueType::Array));
  for (auto const& it : _collections) {
    result.add(VPackValue(it));
  }
  result.close();

  result.add("origin", VPackValue(_operationOrigin.description));

  switch (_operationOrigin.type) {
    case OperationOrigin::Type::kAQL:
      result.add("type", VPackValue("AQL"));
      break;
    case OperationOrigin::Type::kREST:
      result.add("type", VPackValue("REST"));
      break;
    case OperationOrigin::Type::kInternal:
      result.add("type", VPackValue("internal"));
      break;
  }

  result.add("memoryUsage",
             VPackValue(_memoryUsage.load(std::memory_order_relaxed)));
  result.add("peakMemoryUsage",
             VPackValue(_peakMemoryUsage.load(std::memory_order_relaxed)));

  result.close();
}

void HistoryEntry::adjustMemoryUsage(std::int64_t value) noexcept {
  if (value > 0) {
    auto now = _memoryUsage.fetch_add(value, std::memory_order_relaxed) + value;
    auto peak = _peakMemoryUsage.load(std::memory_order_relaxed);
    while (now > peak) {
      if (_peakMemoryUsage.compare_exchange_strong(peak, now,
                                                   std::memory_order_relaxed)) {
        break;
      }
    }
  } else {
    _memoryUsage.fetch_sub(-value, std::memory_order_relaxed);
  }
}

History::History(std::size_t maxSize) : _maxSize(maxSize), _id(1) {}

History::~History() = default;

void History::insert(TransactionState& state) {
  std::vector<std::string> collections;
  state.allCollections([&](auto const& c) {
    collections.emplace_back(c.collectionName());
    return true;
  });

  auto entry = std::make_shared<HistoryEntry>(
      state.vocbase().name(), std::move(collections), state.operationOrigin());

  std::unique_lock<std::shared_mutex> lock(_mutex);

  entry->_id = ++_id;

  state.setHistoryEntry(entry);
  try {
    _history.push_back(std::move(entry));
  } catch (...) {
    state.clearHistoryEntry();
    throw;
  }
}

void History::toVelocyPack(velocypack::Builder& result) const {
  std::shared_lock<std::shared_mutex> lock(_mutex);

  result.openArray();
  for (auto const& it : _history) {
    it->toVelocyPack(result);
  }
  result.close();
}

void History::garbageCollect() noexcept {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  while (_history.size() > _maxSize) {
    _history.pop_front();
  }
}

void History::clear() noexcept {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  _history.clear();
}

}  // namespace arangodb::transaction
#endif
