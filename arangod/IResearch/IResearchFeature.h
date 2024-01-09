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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "resource_manager.hpp"
#include "function2.hpp"
#include "utils/async_utils.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

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

struct IResearchExecutionPool;

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
inline bool isScorer(aql::AstNode const& node) noexcept {
  if (aql::NODE_TYPE_FCALL != node.type &&
      aql::NODE_TYPE_FCALL_USER != node.type) {
    return false;
  }
  // cppcheck-suppress throwInNoexceptFunction
  return isScorer(*static_cast<aql::Function const*>(node.getData()));
}

////////////////////////////////////////////////////////////////////////////////
/// @returns true if the specified 'func' is an ArangoSearch OFFSET_INFO
///          function, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool isOffsetInfo(aql::Function const& func) noexcept;
inline bool isOffsetInfo(aql::AstNode const& node) noexcept {
  return aql::NODE_TYPE_FCALL == node.type &&
         isOffsetInfo(*static_cast<aql::Function const*>(node.getData()));
}

std::filesystem::path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                       TRI_vocbase_t& database);

void cleanupDatabase(TRI_vocbase_t& database);

////////////////////////////////////////////////////////////////////////////////
/// @class IResearchFeature
////////////////////////////////////////////////////////////////////////////////
class IResearchFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ArangoSearch"; }

  explicit IResearchFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void prepare() final;
  void start() final;
  void stop() final;
  void unprepare() final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;

  auto& getSearchPool() noexcept { return _searchExecutionPool; }
  //////////////////////////////////////////////////////////////////////////////
  /// @brief schedule an asynchronous task for execution
  /// @param id thread group to handle the execution
  /// @param fn the function to execute
  /// @param delay how log to sleep before the execution
  //////////////////////////////////////////////////////////////////////////////
  bool queue(ThreadGroup id, std::chrono::steady_clock::duration delay,
             fu2::unique_function<void()>&& fn);

  std::tuple<size_t, size_t, size_t> stats(ThreadGroup id) const;
  std::pair<size_t, size_t> limits(ThreadGroup id) const;

  template<typename Engine>
  IndexTypeFactory& factory();

  void trackOutOfSyncLink() noexcept;
  void untrackOutOfSyncLink() noexcept;

  bool failQueriesOnOutOfSync() const noexcept;

#ifdef USE_ENTERPRISE
  irs::IResourceManager& getCachedColumnsManager() const noexcept {
    return _columnsCacheMemoryUsed;
  }
  bool columnsCacheOnlyLeaders() const noexcept;
#ifdef ARANGODB_USE_GOOGLE_TESTS
  int64_t columnsCacheUsage() const noexcept;
  void setCacheUsageLimit(uint64_t limit) noexcept;

  void setColumnsCacheOnlyOnLeader(bool b) noexcept {
    _columnsCacheOnlyLeader = b;
  }
#endif
#endif

  uint32_t defaultParallelism() const noexcept { return _defaultParallelism; }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setDefaultParallelism(uint32_t v) noexcept { _defaultParallelism = v; }
#endif

 private:
  void registerRecoveryHelper();
  void registerIndexFactory();

  std::shared_ptr<IResearchAsync> _async;

  // whether or not to fail queries on links/indexes that are marked as
  // out of sync
  bool _failQueriesOnOutOfSync{false};

  // names/ids of links/indexes to *NOT* recover. all entries should
  // be in format "collection-name/index-name" or "collection/index-id".
  // the pseudo-entry "all" skips recovering data for all links/indexes
  // found during recovery.
  std::vector<std::string> _skipRecoveryItems;

  // number of links/indexes currently out of sync
  metrics::Gauge<uint64_t>& _outOfSyncLinks;

#ifdef USE_ENTERPRISE
  irs::IResourceManager& _columnsCacheMemoryUsed;
  bool _columnsCacheOnlyLeader{false};
#endif

  uint32_t _deprecatedOptions{0};
  uint32_t _consolidationThreads{0};
  uint32_t _commitThreads{0};
  uint32_t _threads{0};
  uint32_t _threadsLimit{0};
  uint32_t _searchExecutionThreadsLimit{0};
  uint32_t _defaultParallelism{1};

  std::shared_ptr<IndexTypeFactory> _clusterFactory;
  std::shared_ptr<IndexTypeFactory> _rocksDBFactory;

  // helper object, only useful during WAL recovery
  std::shared_ptr<IResearchRocksDBRecoveryHelper> _recoveryHelper;

  IResearchExecutionPool& _searchExecutionPool;
};

}  // namespace iresearch
}  // namespace arangodb
