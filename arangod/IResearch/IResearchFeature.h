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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RestServer/Metrics.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace arangodb {
struct IndexTypeFactory;

namespace aql {

struct Function;

}  // namespace aql

namespace iresearch {

class IResearchAsync;
class IResearchLink;
class ResourceMutex;
class IResearchRocksDBRecoveryHelper;

////////////////////////////////////////////////////////////////////////////////
/// @enum ThreadGroup
/// @brief there are 2 thread groups for execution of asynchronous maintenance
///        jobs.
////////////////////////////////////////////////////////////////////////////////
enum class ThreadGroup : size_t { _0 = 0, _1 };

////////////////////////////////////////////////////////////////////////////////
/// @returns true if the specified 'func' is an ArangoSearch filter function,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
bool isFilter(aql::Function const& func) noexcept;

////////////////////////////////////////////////////////////////////////////////
/// @returns true if the specified 'func' is an ArangoSearch scorer function,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
bool isScorer(aql::Function const& func) noexcept;

////////////////////////////////////////////////////////////////////////////////
/// @class IResearchFeature
////////////////////////////////////////////////////////////////////////////////
class IResearchFeature final : public application_features::ApplicationFeature {
 public:
  static std::string const& name();

  explicit IResearchFeature(application_features::ApplicationServer& server);

  void beginShutdown() override;
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void stop() override;
  void unprepare() override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schedule an asynchronous task for execution
  /// @param id thread group to handle the execution
  /// @param fn the function to execute
  /// @param delay how log to sleep before the execution
  //////////////////////////////////////////////////////////////////////////////
  bool queue(ThreadGroup id, std::chrono::steady_clock::duration delay,
             std::function<void()>&& fn);

  std::tuple<size_t, size_t, size_t> stats(ThreadGroup id) const;
  std::pair<size_t, size_t> limits(ThreadGroup id) const;

  template<typename Engine,
           typename std::enable_if_t<std::is_base_of_v<StorageEngine, Engine>,
                                     int> = 0>
  IndexTypeFactory& factory();

  bool linkSkippedDuringRecovery(arangodb::IndexId id) const noexcept;

  void trackOutOfSyncLink() noexcept;
  void untrackOutOfSyncLink() noexcept;

  bool failQueriesOnOutOfSync() const noexcept;

  bool writeHighTick() const noexcept { return _writeHighTick; }

#ifdef USE_ENTERPRISE
  bool trackColumnsCacheUsage(int64_t diff) noexcept;
#endif
 private:
  void registerRecoveryHelper();

  struct State {
    std::mutex mtx;
    std::condition_variable cv;
    size_t counter{0};
  };

  std::shared_ptr<State> _startState;
  std::shared_ptr<IResearchAsync> _async;
  std::atomic<bool> _running;

  // whether or not to fail queries on links/indexes that are marked as
  // out of sync
  bool _failQueriesOnOutOfSync;

  // names/ids of links/indexes to *NOT* recover. all entries should
  // be in format "collection-name/index-name" or "collection/index-id".
  // the pseudo-entry "all" skips recovering data for all links/indexes
  // found during recovery.
  std::vector<std::string> _skipRecoveryItems;

  uint32_t _consolidationThreads;
  uint32_t _consolidationThreadsIdle;
  uint32_t _commitThreads;
  uint32_t _commitThreadsIdle;
  uint32_t _threads;
  uint32_t _threadsLimit;
  std::map<std::type_index, std::shared_ptr<IndexTypeFactory>> _factories;

  // number of links/indexes currently out of sync
  Gauge<uint64_t>& _outOfSyncLinks;

#ifdef USE_ENTERPRISE
  Gauge<int64_t>& _columnsCacheMemoryUsed;
  uint64_t _columnsCacheLimit{0};
#endif
  bool _writeHighTick{false};
  // helper object, only useful during WAL recovery
  std::shared_ptr<IResearchRocksDBRecoveryHelper> _recoveryHelper;
};

}  // namespace iresearch
}  // namespace arangodb
