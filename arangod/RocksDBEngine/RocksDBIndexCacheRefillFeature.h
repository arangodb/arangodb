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

#include "RestServer/arangod.h"
#include "VocBase/Identifiers/IndexId.h"

#include <memory>
#include <mutex>
#include <string_view>
#include <string>
#include <vector>

namespace arangodb {
class LogicalCollection;
class RocksDBIndexCacheRefillThread;

class RocksDBIndexCacheRefillFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "RocksDBIndexCacheRefill";
  }

  explicit RocksDBIndexCacheRefillFeature(Server& server);

  ~RocksDBIndexCacheRefillFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void beginShutdown() override;
  void start() override;
  void stop() override;

  void trackRefill(std::shared_ptr<LogicalCollection> const& collection,
                   IndexId iid, std::vector<std::string> keys);

  size_t maxCapacity() const noexcept;
  bool autoRefill() const noexcept;
  bool fillOnStartup() const noexcept;

 private:
  void stopThread();
  void buildIndexRefillTasks();
  void queueIndexRefillTasks();
  void warmupIndex(std::string const& database, std::string const& collection,
                   IndexId iid);

  std::unique_ptr<RocksDBIndexCacheRefillThread> _refillThread;

  // maximum capacity of queue used for automatic refilling of in-memory index
  // caches
  size_t _maxCapacity;

  // maximum concurrent index fill tasks that we are allowed to run to fill
  // indexes during startup
  size_t _maxConcurrentIndexFillTasks;

  // whether or not in-memory cache values for indexes are automatically
  // refilled upon insert/update/replace
  bool _autoRefill;

  // whether or not in-memory cache values for indexes are automatically
  // populated on server start
  bool _fillOnStartup;

  std::mutex _indexFillTasksMutex;

  struct IndexFillTask {
    std::string database;
    std::string collection;
    IndexId iid;
  };
  std::vector<IndexFillTask> _indexFillTasks;

  size_t _currentlyRunningIndexFillTasks;
};
}  // namespace arangodb
