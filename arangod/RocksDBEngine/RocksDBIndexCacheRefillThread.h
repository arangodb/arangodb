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

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
class DatabaseFeature;
class LogicalCollection;

class RocksDBIndexCacheRefillThread final : public ServerThread<ArangodServer> {
 public:
  explicit RocksDBIndexCacheRefillThread(ArangodServer& server,
                                         size_t maxCapacity);

  ~RocksDBIndexCacheRefillThread();

  void beginShutdown() override;

  void trackRefill(std::shared_ptr<LogicalCollection> const& collection,
                   IndexId iid, std::vector<std::string> keys);

  void waitForCatchup();

 protected:
  void run() override;

 private:
  DatabaseFeature& _databaseFeature;

  using IndexValues = std::unordered_map<IndexId, std::vector<std::string>>;
  using CollectionValues = std::unordered_map<DataSourceId, IndexValues>;
  using DatabaseValues = std::unordered_map<TRI_voc_tick_t, CollectionValues>;

  void refill(TRI_vocbase_t& vocbase, DataSourceId cid,
              IndexValues const& data);
  void refill(TRI_vocbase_t& vocbase, CollectionValues const& data);
  void refill(DatabaseValues const& data);

  size_t const _maxCapacity;

  // protects _operations and _numQueued
  basics::ConditionVariable _condition;

  // queued operations
  DatabaseValues _operations;
  // current number of operations queued
  size_t _numQueued;
  // current number of in-process operations
  size_t _proceeding;

  // total number of items ever queued
  metrics::Counter& _totalNumQueued;
  // total number of items ever dropped (because of queue full)
  metrics::Counter& _totalNumDropped;
};
}  // namespace arangodb
