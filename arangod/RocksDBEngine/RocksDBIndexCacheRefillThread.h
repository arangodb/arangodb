////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

#include <memory>
#include <string>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
class DatabaseFeature;
class LogicalCollection;
class RocksDBIndexCacheRefillFeature;

class RocksDBIndexCacheRefillThread final : public Thread {
 public:
  explicit RocksDBIndexCacheRefillThread(
      application_features::ApplicationServer& server, size_t id,
      size_t maxCapacity);

  ~RocksDBIndexCacheRefillThread();

  void beginShutdown() override;

  bool trackRefill(std::shared_ptr<LogicalCollection> const& collection,
                   IndexId iid, containers::FlatHashSet<std::string>& keys);

  void waitForCatchup();

 protected:
  void run() override;

 private:
  using IndexValues =
      containers::FlatHashMap<IndexId, containers::FlatHashSet<std::string>>;
  using CollectionValues = containers::FlatHashMap<DataSourceId, IndexValues>;
  using DatabaseValues =
      containers::FlatHashMap<TRI_voc_tick_t, CollectionValues>;

  void refill(TRI_vocbase_t& vocbase, DataSourceId cid,
              IndexValues const& data);
  void refill(TRI_vocbase_t& vocbase, CollectionValues const& data);
  void refill(DatabaseValues const& data);

  DatabaseFeature& _databaseFeature;

  RocksDBIndexCacheRefillFeature& _refillFeature;

  // maximum queue capacity for thread
  size_t const _maxCapacity;

  // protects _operations and _numQueued
  basics::ConditionVariable _condition;

  // queued operations
  DatabaseValues _operations;

  // current number of operations queued
  size_t _numQueued;

  // current number of in-process operations
  size_t _proceeding;
};
}  // namespace arangodb
