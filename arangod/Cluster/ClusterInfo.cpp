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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "ClusterInfo.h"

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/TransactionBuilder.h"
#include "Agency/Supervision.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/FeatureFlags.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/GlobalSerialization.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Basics/Result.tpp"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/TimeString.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/hashes.h"
#include "Basics/system-functions.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterCollectionCreationInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/CollectionInfoCurrent.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/RebootTracker.h"
#include "Cluster/ServerState.h"
#include "Containers/NodeHashMap.h"
#include "Indexes/Index.h"
#include "Inspection/VPack.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "Logger/Logger.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"
#include "Random/RandomGenerator.h"
#include "Replication2/Methods.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/AgencyCollectionSpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Rest/CommonDefines.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/CountCache.h"
#include "Utils/Events.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/Methods/Indexes.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <chrono>

#include "Containers/Enumerate.h"

namespace arangodb {
/// @brief internal helper struct for counting the number of shards etc.
struct ShardStatistics {
  uint64_t databases{0};
  uint64_t collections{0};
  uint64_t shards{0};
  uint64_t leaders{0};
  uint64_t realLeaders{0};
  uint64_t followers{0};
  uint64_t servers{0};

  void toVelocyPack(velocypack::Builder& builder) const {
    builder.openObject();
    builder.add("databases", VPackValue(databases));
    builder.add("collections", VPackValue(collections));
    builder.add("shards", VPackValue(shards));
    builder.add("leaders", VPackValue(leaders));
    builder.add("realLeaders", VPackValue(realLeaders));
    builder.add("followers", VPackValue(followers));
    builder.add("servers", VPackValue(servers));
    builder.close();
  }
};

namespace {

std::string const kMetricsServerId = "Plan/Metrics/ServerId";
std::string const kMetricsRebootId = "Plan/Metrics/RebootId";

void addToShardStatistics(ShardStatistics& stats,
                          containers::FlatHashSet<std::string>& servers,
                          velocypack::Slice databaseSlice,
                          std::string_view restrictServer) {
  bool foundCollection = false;
  std::unordered_map<replication2::agency::CollectionGroupId, std::string_view>
      designatedGroupLeader;

  for (auto it : VPackObjectIterator(databaseSlice)) {
    VPackSlice collection = it.value;

    bool hasDistributeShardsLike = false;
    if (VPackSlice dsl = collection.get(StaticStrings::DistributeShardsLike);
        dsl.isString()) {
      hasDistributeShardsLike = dsl.getStringLength() > 0;
    }

    bool foundShard = false;
    VPackSlice shards = collection.get("shards");
    for (auto pair : VPackObjectIterator(shards)) {
      int i = 0;
      for (auto const& serv : VPackArrayIterator(pair.value)) {
        if (!restrictServer.empty() && serv.stringView() != restrictServer) {
          // different server
          i++;
          continue;
        }

        foundShard = true;
        servers.emplace(serv.copyString());

        ++stats.shards;
        if (i++ == 0) {
          ++stats.leaders;
          if (!hasDistributeShardsLike) {
            auto groupId = collection.get(StaticStrings::GroupId);
            if (groupId.isNumber()) {
              auto gid = replication2::agency::CollectionGroupId{
                  groupId.getNumber<uint64_t>()};
              auto maybeLeader = designatedGroupLeader.find(gid);
              if (maybeLeader == designatedGroupLeader.end()) {
                // Just declare this collection to "lead" the group
                // The concept of a group leader is obsolete in Replication2
                designatedGroupLeader.emplace(gid, it.key.stringView());
                ++stats.realLeaders;
              } else if (maybeLeader->second == it.key.stringView()) {
                // We have already declared this collection to lead the group
                ++stats.realLeaders;
              }
            } else {
              ++stats.realLeaders;
            }
          }
        } else {
          ++stats.followers;
        }
      }
    }

    if (foundShard) {
      foundCollection = true;
      ++stats.collections;
    }
  }

  if (foundCollection) {
    ++stats.databases;
  }
}

void addToShardStatistics(
    containers::NodeHashMap<ServerID, ShardStatistics>& stats,
    velocypack::Slice databaseSlice) {
  containers::FlatHashSet<std::string_view> serversSeenForDatabase;
  std::unordered_map<replication2::agency::CollectionGroupId, std::string_view>
      designatedGroupLeader;
  for (auto it : VPackObjectIterator(databaseSlice)) {
    VPackSlice collection = it.value;

    bool hasDistributeShardsLike = false;
    if (VPackSlice dsl = collection.get(StaticStrings::DistributeShardsLike);
        dsl.isString()) {
      hasDistributeShardsLike = dsl.getStringLength() > 0;
    }

    containers::FlatHashSet<std::string_view> serversSeenForCollection;

    VPackSlice shards = collection.get("shards");
    for (auto pair : VPackObjectIterator(shards)) {
      int i = 0;
      for (auto const& serv : VPackArrayIterator(pair.value)) {
        auto& stat = stats[serv.copyString()];

        if (serversSeenForCollection.emplace(serv.stringView()).second) {
          ++stat.collections;

          if (serversSeenForDatabase.emplace(serv.stringView()).second) {
            ++stat.databases;
          }
        }

        ++stat.shards;
        if (i++ == 0) {
          ++stat.leaders;
          if (!hasDistributeShardsLike) {
            auto groupId = collection.get(StaticStrings::GroupId);
            if (groupId.isNumber()) {
              auto gid = replication2::agency::CollectionGroupId{
                  groupId.getNumber<uint64_t>()};
              auto maybeLeader = designatedGroupLeader.find(gid);
              if (maybeLeader == designatedGroupLeader.end()) {
                // Just declare this collection to "lead" the group
                // The concept of a group leader is obsolete in Replication2
                designatedGroupLeader.emplace(gid, it.key.stringView());
                ++stat.realLeaders;
              } else if (maybeLeader->second == it.key.stringView()) {
                // We have already declared this collection to lead the group
                ++stat.realLeaders;
              }
            } else {
              ++stat.realLeaders;
            }
          }
        } else {
          ++stat.followers;
        }
      }
    }
  }
}

inline std::string analyzersPath(std::string_view dbName) {
  return "Plan/Analyzers/" + std::string{dbName};
}

constexpr frozen::unordered_map<std::string_view, ServerHealth, 4>
    kHealthStatusMap = {
        {consensus::Supervision::HEALTH_STATUS_BAD, ServerHealth::kBad},
        {consensus::Supervision::HEALTH_STATUS_FAILED, ServerHealth::kFailed},
        {consensus::Supervision::HEALTH_STATUS_GOOD, ServerHealth::kGood},
        {consensus::Supervision::HEALTH_STATUS_UNCLEAR,
         ServerHealth::kUnclear}};

[[nodiscard]] ServersKnown parseServersKnown(
    VPackSlice const serversKnownSlice, VPackSlice const supervisionHealth,
    containers::FlatHashSet<ServerID> const& serverIds) {
  ServersKnown res;
  TRI_ASSERT(serversKnownSlice.isNone() || serversKnownSlice.isObject());
  LOG_TOPIC("91da8", TRACE, Logger::CLUSTER)
      << "Supervision health is:" << supervisionHealth.toString();
  if (serversKnownSlice.isObject()) {
    for (auto const it : VPackObjectIterator(serversKnownSlice)) {
      auto status = ServerHealth::kUnclear;
      std::string serverId = it.key.copyString();
      if (ADB_LIKELY(supervisionHealth.isObject())) {
        auto serverKey = supervisionHealth.get(serverId);
        // Server may be missing from Health if it has just arrived
        // to our cluster.
        if (serverKey.isObject()) {
          auto statusString = serverKey.get("Status");
          if (statusString.isString()) {
            if (auto decoded = kHealthStatusMap.find(statusString.stringView());
                decoded != kHealthStatusMap.end()) {
              status = decoded->second;
            }
          }
        }
      }
      VPackSlice const knownServerSlice = it.value;
      TRI_ASSERT(knownServerSlice.isObject());
      if (knownServerSlice.isObject()) {
        VPackSlice const rebootIdSlice = knownServerSlice.get("rebootId");
        TRI_ASSERT(rebootIdSlice.isInteger());
        if (rebootIdSlice.isInteger()) {
          auto const rebootId =
              RebootId{rebootIdSlice.getNumericValue<uint64_t>()};
          res.emplace(
              std::move(serverId),
              ServerHealthState{.rebootId = rebootId, .status = status});
        }
      }
    }
  }
  return res;
}

void doQueueLinkDrop(IndexId id, std::string const& collection,
                     std::string const& vocbase, ClusterInfo& ci) {
  auto* scheduler = SchedulerFeature::SCHEDULER;
  if (!scheduler || ci.server().isStopping()) {
    return;
  }
  LOG_TOPIC("0d7b2", WARN, Logger::CLUSTER)
      << "Scheduling drop for dangling link " << id;
  auto dropTask = [id = id, collection = collection, vocbase = vocbase,
                   &ci = ci] {
    if (!ci.server().isStopping()) {
      auto coll = ci.getCollectionNT(vocbase, collection);
      if (coll) {
        velocypack::Builder builder;
        builder.openObject();
        builder.add(StaticStrings::IndexId, velocypack::Value(id.id()));
        builder.close();
        LOG_TOPIC("d7665", TRACE, Logger::CLUSTER)
            << "Dropping dangling link " << id;
        Result res;
        TRI_IF_FAILURE("IResearchLink::failDropDangling") {
          res = Result{TRI_ERROR_DEBUG};
        }
        else {
          res = methods::Indexes::drop(*coll, builder.slice()).waitAndGet();
        }
        if (res.fail() && res.isNot(TRI_ERROR_ARANGO_INDEX_NOT_FOUND)) {
          // we should have internal superuser
          TRI_ASSERT(res.isNot(TRI_ERROR_FORBIDDEN));
          LOG_TOPIC("b27f3", WARN, Logger::CLUSTER)
              << "Failed to drop dangling link " << id
              << " Err: " << res.errorMessage();
          doQueueLinkDrop(id, collection, vocbase, ci);
        } else {
          LOG_TOPIC("2c47a", TRACE, Logger::CLUSTER)
              << "Removed dangling link" << id;
        }
      } else {
        LOG_TOPIC("f5596", TRACE, Logger::CLUSTER)
            << "Scheduled drop for dangling link " << id
            << " skipped as collection is dropped";
      }
    };
  };
  scheduler->queue(RequestLane::INTERNAL_LOW, dropTask);
}

inline arangodb::AgencyOperation SetOldEntry(
    std::string const& key, std::vector<std::string_view> const& path,
    VPackSlice plan) {
  VPackSlice newEntry = plan.get(path);
  if (newEntry.isNone()) {
    // This is a countermeasure to protect against non-existing paths. If we
    // get anything else original plan is already broken.
    newEntry = VPackSlice::emptyObjectSlice();
  }
  return {key, AgencyValueOperationType::SET, newEntry};
}

}  // namespace
}  // namespace arangodb

using namespace arangodb;
using namespace cluster;
using namespace methods;

namespace StringUtils = basics::StringUtils;

class ClusterInfo::SyncerThread final
    : public arangodb::ServerThread<ArangodServer> {
 public:
  explicit SyncerThread(Server&, std::string const& section,
                        std::function<consensus::index_t()> const&,
                        AgencyCache&);
  ~SyncerThread() override;
  void beginShutdown() override;
  void run() override;
  bool start();
  void sendNews() noexcept;
  void waitForNews() noexcept;

 private:
  auto call() noexcept -> std::optional<consensus::index_t>;
  futures::Future<futures::Unit> fetchUpdates();

 private:
  std::mutex _m;
  std::condition_variable _cv;
  bool _news;

  std::string _section;
  std::function<consensus::index_t()> _f;
  AgencyCache& _agencyCache;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cluster info object
////////////////////////////////////////////////////////////////////////////////

struct ClusterInfoScale {
  static metrics::LogScale<float> scale() {
    return {std::exp(1.f), 0.f, 2500.f, 10};
  }
};

DECLARE_HISTOGRAM(arangodb_load_current_runtime, ClusterInfoScale,
                  "Current loading runtimes [ms]");
DECLARE_HISTOGRAM(arangodb_load_plan_runtime, ClusterInfoScale,
                  "Plan loading runtimes [ms]");

DECLARE_GAUGE(arangodb_internal_cluster_info_memory_usage, std::uint64_t,
              "Total memory used by internal cluster info data structures");

ClusterInfo::ClusterInfo(ArangodServer& server, AgencyCache& agencyCache,
                         AgencyCallbackRegistry& agencyCallbackRegistry,
                         ErrorCode syncerShutdownCode,
                         metrics::MetricsFeature& metrics)
    : _server(server),
      _clusterFeature(server.getFeature<ClusterFeature>()),
      _agency(server),
      _agencyCache(agencyCache),
      _agencyCallbackRegistry(agencyCallbackRegistry),
      _rebootTracker(SchedulerFeature::SCHEDULER),
      _syncerShutdownCode(syncerShutdownCode),
      _memoryUsage(metrics.add(arangodb_internal_cluster_info_memory_usage{})),
      _lpTimer(metrics.add(arangodb_load_plan_runtime{})),
      _lcTimer(metrics.add(arangodb_load_current_runtime{})),
      _resourceMonitor(_memoryUsage),
      _servers(_resourceMonitor),
      _serverAliases(_resourceMonitor),
      _serverAdvertisedEndpoints(_resourceMonitor),
      _serverTimestamps(_resourceMonitor),
      _dbServers(_resourceMonitor),
      _coordinators(_resourceMonitor),
      _coordinatorIdMap(_resourceMonitor),
      _plan(_resourceMonitor),
      _current(_resourceMonitor),
      _plannedDatabases(_resourceMonitor),
      _planVersion(0),
      _planIndex(0),
      _planMemoryUsage(0),
      _currentDatabases(_resourceMonitor),
      _currentVersion(0),
      _currentIndex(0),
      _currentMemoryUsage(0),
      _plannedCollections(_resourceMonitor),
      _newPlannedCollections(_resourceMonitor),
      _shards(_resourceMonitor),
      _shardsToPlanServers(_resourceMonitor),
      _shardToName(_resourceMonitor),
      _shardToDb(_resourceMonitor),
      _shardToShardGroupLeader(_resourceMonitor),
      _shardGroups(_resourceMonitor),
      _plannedViews(_resourceMonitor),
      _newPlannedViews(_resourceMonitor),
      _dbAnalyzersRevision(_resourceMonitor),
      _planLoader(std::thread::id()),
      _currentCollections(_resourceMonitor),
      _shardsToCurrentServers(_resourceMonitor),
      _newStuffByDatabase(_resourceMonitor),
      _replicatedLogs(_resourceMonitor),
      _collectionGroups(_resourceMonitor),
      _uniqid(),
      _failedServers(_resourceMonitor),
      _waitPlan(_resourceMonitor),
      _waitPlanVersion(_resourceMonitor),
      _waitCurrent(_resourceMonitor),
      _waitCurrentVersion(_resourceMonitor) {
  _uniqid._currentValue = 1ULL;
  _uniqid._upperValue = 0ULL;
  _uniqid._nextBatchStart = 1ULL;
  _uniqid._nextUpperValue = 0ULL;
  _uniqid._backgroundJobIsRunning = false;
  // Actual loading into caches is postponed until necessary

#ifdef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_syncerShutdownCode == TRI_ERROR_NO_ERROR ||
             _syncerShutdownCode == TRI_ERROR_SHUTTING_DOWN);
#else
  TRI_ASSERT(_syncerShutdownCode == TRI_ERROR_SHUTTING_DOWN);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::~ClusterInfo() {
  _resourceMonitor.decreaseMemoryUsage(_planMemoryUsage);
  _planMemoryUsage = 0;
  _resourceMonitor.decreaseMemoryUsage(_currentMemoryUsage);
  _currentMemoryUsage = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup method which frees cluster-internal shared ptrs on shutdown
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::unprepare() {
  waitForSyncersToStop();

  while (true) {
    {
      std::lock_guard mutexLocker{_idLock};
      if (!_uniqid._backgroundJobIsRunning) {
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::lock_guard mutexLocker{_planProt.mutex};

  {
    WRITE_LOCKER(writeLocker, _planProt.lock);
    _plannedViews.clear();
    _plannedCollections.clear();
    _shards.clear();
  }

  {
    WRITE_LOCKER(writeLocker, _currentProt.lock);
    _currentCollections.clear();
    _shardsToCurrentServers.clear();
  }
}

void ClusterInfo::triggerBackgroundGetIds() {
  // Trigger a new load of batches
  _uniqid._nextBatchStart = 1ULL;
  _uniqid._nextUpperValue = 0ULL;

  try {
    if (_uniqid._backgroundJobIsRunning) {
      return;
    }
    _uniqid._backgroundJobIsRunning = true;
    std::thread([this] {
      auto guardRunning = scopeGuard([this]() noexcept {
        std::lock_guard mutexLocker{_idLock};
        _uniqid._backgroundJobIsRunning = false;
      });

      uint64_t result;
      try {
        result = _agency.uniqid(MinIdsPerBatch, 0.0);
      } catch (std::exception const&) {
        return;
      }

      {
        std::lock_guard mutexLocker{_idLock};

        if (1ULL == _uniqid._nextBatchStart) {
          // Invalidate next batch
          _uniqid._nextBatchStart = result;
          _uniqid._nextUpperValue = result + MinIdsPerBatch - 1;
        }
        // If we get here, somebody else tried succeeded in doing the same,
        // so we just try again.
      }
    }).detach();
  } catch (std::exception const& e) {
    LOG_TOPIC("adef4", WARN, Logger::CLUSTER)
        << "Failed to trigger background get ids. " << e.what();
  }
}

/// @brief produces an agency dump and logs it
void ClusterInfo::logAgencyDump() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto [acb, idx] = _agencyCache.read(std::vector<std::string>{"/"});
  auto res = acb->slice();

  if (!res.isNone()) {
    LOG_TOPIC("fe8ce", INFO, Logger::CLUSTER) << "Agency dump:\n"
                                              << res.toJson();
  } else {
    LOG_TOPIC("e7e30", WARN, Logger::CLUSTER) << "Could not get agency dump!";
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the uniqid value. if it exceeds the upper bound, fetch a
/// new upper bound value from the agency
////////////////////////////////////////////////////////////////////////////////

uint64_t ClusterInfo::uniqid(uint64_t count) {
  TRI_IF_FAILURE("deterministic-cluster-wide-uniqid") {
    // we want to use value range which HLC encoded starts with a digit.
    // 54 * 64 ^ 3 HLC encoded is "0---"
    static std::atomic<uint64_t> idCounter = 54 * 64 * 64 * 64;
    return idCounter.fetch_add(1);
  }

  std::lock_guard mutexLocker{_idLock};

  if (_uniqid._currentValue + count - 1 <= _uniqid._upperValue) {
    uint64_t result = _uniqid._currentValue;
    _uniqid._currentValue += count;

    TRI_ASSERT(result != 0);
    return result;
  }

  // Try if we can use the next batch
  if (_uniqid._nextBatchStart + count - 1 <= _uniqid._nextUpperValue) {
    uint64_t result = _uniqid._nextBatchStart;
    _uniqid._currentValue = _uniqid._nextBatchStart + count;
    _uniqid._upperValue = _uniqid._nextUpperValue;
    triggerBackgroundGetIds();

    TRI_ASSERT(result != 0);
    return result;
  }

  // We need to fetch from the agency

  uint64_t fetch = count;

  if (fetch < MinIdsPerBatch) {
    fetch = MinIdsPerBatch;
  }

  uint64_t result = _agency.uniqid(2 * fetch, 0.0);

  _uniqid._currentValue = result + count;
  _uniqid._upperValue = result + fetch - 1;
  // Invalidate next batch
  _uniqid._nextBatchStart = _uniqid._upperValue + 1;
  _uniqid._nextUpperValue = _uniqid._upperValue + fetch - 1;

  TRI_ASSERT(result != 0);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing)
////////////////////////////////////////////////////////////////////////////////
void ClusterInfo::flush() {
  loadServers();
  loadCurrentDBServers();
  loadCurrentCoordinators();
  loadCurrentMappings();
  loadPlan();
  loadCurrent();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

bool ClusterInfo::doesDatabaseExist(std::string_view databaseID) {
  // Wait for sensible data in agency cache
  if (!_planProt.isValid) {
    Result r = waitForPlan(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.
  {
    size_t expectedSize;
    {
      READ_LOCKER(readLocker, _dbServersProt.lock);
      expectedSize = _dbServers.size();
    }

    // look up database by name:

    READ_LOCKER(readLocker, _planProt.lock);
    // _plannedDatabases is a map-type<DatabaseID, VPackSlice>

    if (_plannedDatabases.contains(databaseID)) {
      // found the database in Plan
      READ_LOCKER(readLocker, _currentProt.lock);
      // _currentDatabases is
      //     a map-type<DatabaseID, a map-type<ServerID, VPackSlice>>
      if (auto it = _currentDatabases.find(databaseID);
          it != _currentDatabases.end()) {
        // found the database in Current
        return it->second->size() >= expectedSize;
      }
    }
  }

  loadCurrentDBServers();

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

std::vector<DatabaseID> ClusterInfo::databases() {
  if (!_planProt.isValid) {
    Result r = waitForPlan(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  // The _plannedDatabases map contains all Databases that
  // are planned to exist, and that do not have the "isBuilding"
  // flag set. Hence those databases have been successfully created
  // and should be listed.
  std::vector<DatabaseID> result;
  {
    READ_LOCKER(readLockerPlanned, _planProt.lock);
    // _plannedDatabases is a map-type<DatabaseID, VPackSlice>
    for (auto const& it : _plannedDatabases) {
      result.emplace_back(it.first);
    }
  }

  return result;
}

/// @brief create a new collecion object from the data, using the cache if
/// possible
ClusterInfo::CollectionWithHash ClusterInfo::buildCollection(
    bool isBuilding, AllCollections::const_iterator existingCollections,
    std::string_view collectionId, velocypack::Slice data,
    TRI_vocbase_t& vocbase, uint64_t planVersion, bool cleanupLinks) const {
  std::shared_ptr<LogicalCollection> collection;
  uint64_t hash = 0;
  uint64_t countCache = transaction::CountCache::kNotPopulated;

  if (!isBuilding && existingCollections != _plannedCollections.end()) {
    // check if we already know this collection from a previous run...
    if (auto existing = existingCollections->second->find(collectionId);
        existing != existingCollections->second->end()) {
      CollectionWithHash const& previous = existing->second;
      // note the cached count result of the previous collection
      countCache = previous.collection->countCache().get();

      // compare the hash values of what is in the cache with the hash of the
      // collection a hash value of 0 means that the collection must not be read
      // from the cache, potentially because it contains a link to a view (which
      // would require more complex dependency management)
      if (previous.hash != 0) {
        // calculate hash value. we are using Slice::hash() here intentionally
        // in contrast to the slower Slice::normalizedHash(), as the only source
        // for the VelocyPack is the agency/agency cache, which will always
        // create the data in the same way.
        hash = data.hash();
        // if for some reason the generated hash value is 0, too, we simply
        // don't cache this collection. This is not a problem, as it will not
        // affect correctness but will only lead to one collection less being
        // cached.
        if (previous.hash == hash) {
          // hashes are identical. reuse the collection from cache
          // this is very beneficial for performance, because we can avoid
          // rebuilding the entire LogicalCollection object!
          collection = previous.collection;
        }
      }
    }
  }

  // collection may be a nullptr here if no such collection exists in the cache,
  // or if the collection is in building stage
  if (collection == nullptr) {
    // no previous version of the collection exists, or its hash value has
    // changed
    collection = vocbase.createCollectionObject(data, /*isAStub*/ true);
    TRI_ASSERT(collection != nullptr);

    if (countCache != transaction::CountCache::kNotPopulated) {
      // carry forward the count cache value from the previous collection, if
      // set. this way we avoid that the count value will be refetched via
      // HTTP requests instantly after the collection object is used next.
      collection->countCache().store(countCache);
    }
    if (!isBuilding) {
      auto indexes = collection->getPhysical()->getAllIndexes();
      // if the collection has a link to a view, there are dependencies between
      // collection objects and view objects. in this case, we need to disable
      // the collection caching optimization
      bool const hasViewLink =
          std::any_of(indexes.begin(), indexes.end(), [](auto const& index) {
            return (index->type() == Index::TRI_IDX_TYPE_IRESEARCH_LINK);
          });
      if (hasViewLink) {
        // we do have a view. set hash to 0, which will disable the caching
        // optimization
        hash = 0;
        if (cleanupLinks) {
          TRI_ASSERT(ServerState::instance()->isCoordinator());
          for (auto const& idx : indexes) {
            TRI_ASSERT(idx);
            if (idx->type() == Index::TRI_IDX_TYPE_IRESEARCH_LINK) {
              auto& coordLink =
                  basics::downCast<iresearch::IResearchLinkCoordinator const>(
                      *idx);
              auto const& viewId = coordLink.getViewId();
              if (auto vocbaseViews = _newPlannedViews.find(vocbase.name());
                  vocbaseViews == _newPlannedViews.end() ||
                  !vocbaseViews->second.contains(viewId)) {
                if (!_pendingCleanups.contains(idx->id().id())) {
                  doQueueLinkDrop(idx->id(), collection->name(), vocbase.name(),
                                  const_cast<ClusterInfo&>(*this));
                }
                _currentCleanups.emplace(idx->id().id());
              }
            }
          }
        }
      } else if (hash == 0) {
        // not yet hashed. now calculate the hash value
        hash = data.hash();
      }
    }
  }

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(!isBuilding || hash == 0);

  return {hash, collection};
}

struct ClusterInfo::NewStuffByDatabase {
  using allocator_type = ClusterInfoResourceAllocator<NewStuffByDatabase>;

  NewStuffByDatabase(allocator_type const& alloc)
      : replicatedLogs(alloc), collectionGroups(alloc) {}

  ReplicatedLogsMap replicatedLogs;
  CollectionGroupMap collectionGroups;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about our plan
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

auto ClusterInfo::loadPlan() -> consensus::index_t {
  using namespace std::chrono;
  using clock = std::chrono::high_resolution_clock;

  bool const isCoordinator = ServerState::instance()->isCoordinator();

  [[maybe_unused]] auto start = clock::now();

  auto& databaseFeature = _server.getFeature<DatabaseFeature>();

  // We need to wait for any cluster operation, which needs access to the
  // agency cache for it to become ready. The essentials in the cluster, namely
  // ClusterInfo etc, need to start after first poll result from the agency.
  // This is of great importance to not accidentally delete data facing an
  // empty agency. There are also other measures that guard against such an
  // outcome. But there is also no point continuing with a first agency poll.
  Result r = _agencyCache.waitFor(1).waitAndGet();
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }

  std::lock_guard mutexLocker{_planProt.mutex};  // only one may work at a time

  uint64_t planIndex;
  uint64_t planVersion;
  {
    READ_LOCKER(guard, _planProt.lock);
    planIndex = _planIndex;
    planVersion = _planVersion;
  }

  auto const changeSet = _agencyCache.changedSince(
      "Plan", planIndex);  // also delivers plan/version
  bool const changed = !changeSet.dbs.empty() || changeSet.rest != nullptr;

  // early exit if nothing changed
  // Note: I don't think checking for Plan/Version in addition to `changed` is
  //       of any use, and that it could be removed.
  if (!changed && planVersion == changeSet.version) {
    auto const newPlanIndex = changeSet.ind;
    {
      WRITE_LOCKER(writeLocker, _planProt.lock);
      _planIndex = newPlanIndex;
    }
    if (planIndex < newPlanIndex) {
      std::lock_guard w(_waitPlanLock);
      triggerWaiting(_waitPlan, newPlanIndex);
      auto heartbeatThread = _clusterFeature.heartbeatThread();
      if (heartbeatThread) {
        // In the unittests, there is no heartbeat thread, and we do not need to
        // notify
        heartbeatThread->notify();
      }
    }
    return newPlanIndex;
  }

  decltype(_plan) newPlan{_resourceMonitor};
  {
    READ_LOCKER(readLocker, _planProt.lock);
    newPlan = _plan;
    for (auto const& db : changeSet.dbs) {  // Databases
      newPlan[db.first] = db.second;
    }
    if (changeSet.rest != nullptr) {  // Rest
      newPlan[std::string_view{}] = changeSet.rest;
    }
  }

  // For ArangoSearch views we need to get access to immediately created views
  // in order to allow links to be created correctly.
  // For the scenario above, we track such views in '_newPlannedViews' member
  // which is supposed to be empty before and after 'ClusterInfo::loadPlan()'
  // execution. In addition, we do the following "trick" to provide access to
  // '_newPlannedViews' from outside 'ClusterInfo': in case if
  // 'ClusterInfo::getView' has been called from within 'ClusterInfo::loadPlan',
  // we redirect caller to search view in
  // '_newPlannedViews' member instead of '_plannedViews'

  // set plan loader
  {
    READ_LOCKER(guard, _planProt.lock);
    // Create a copy, since we might not visit all databases
    _newPlannedViews = _plannedViews;
    _newPlannedCollections = _plannedCollections;
    _planLoader = std::this_thread::get_id();
    _currentCleanups.clear();
  }

  // ensure we'll eventually reset plan loader
  auto resetLoader = scopeGuard([&]() noexcept {
    _planLoader = std::thread::id();
    _newPlannedViews.clear();
    _newPlannedCollections.clear();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto diff = clock::now() - start;
    if (diff > std::chrono::milliseconds(500)) {
      LOG_TOPIC("66666", WARN, Logger::CLUSTER)
          << "Loading the new plan took: "
          << std::chrono::duration<double>(diff).count() << "s";
    }
#endif
  });

  containers::FlatHashSet<std::string> buildingDatabases;
  decltype(_plannedDatabases) newDatabases{_resourceMonitor};
  decltype(_shards) newShards{_resourceMonitor};
  decltype(_shardsToPlanServers) newShardsToPlanServers{_resourceMonitor};
  decltype(_shardToShardGroupLeader) newShardToShardGroupLeader{
      _resourceMonitor};
  decltype(_shardGroups) newShardGroups{_resourceMonitor};
  decltype(_shardToName) newShardToName{_resourceMonitor};
  decltype(_shardToDb) newShardToDb{_resourceMonitor};
  decltype(_dbAnalyzersRevision) newDbAnalyzersRevision{_resourceMonitor};
  decltype(_newStuffByDatabase) newStuffByDatabase{_resourceMonitor};

  bool swapDatabases = false;
  bool swapCollections = false;
  bool swapViews = false;
  bool swapAnalyzers = false;

  bool planValid = true;  // has the loadPlan completed without skipping valid
                          // objects we will set in the end

  {
    READ_LOCKER(guard, _planProt.lock);
    auto start = std::chrono::steady_clock::now();
    newDatabases = _plannedDatabases;
    newShards = _shards;
    newShardsToPlanServers = _shardsToPlanServers;
    newShardToShardGroupLeader = _shardToShardGroupLeader;
    newShardGroups = _shardGroups;
    newShardToName = _shardToName;
    newShardToDb = _shardToDb;
    newDbAnalyzersRevision = _dbAnalyzersRevision;
    newStuffByDatabase = _newStuffByDatabase;
    auto ende = std::chrono::steady_clock::now();
    LOG_TOPIC("feee1", TRACE, Logger::CLUSTER)
        << "Time for copy operation in loadPlan: "
        << std::chrono::duration_cast<std::chrono::nanoseconds>(ende - start)
               .count()
        << " ns";
  }

  std::string name;

  // mark for swap even if no databases present to ensure dangling datasources
  // are removed
  // TODO: Maybe optimize. Else remove altogether
  if (!changeSet.dbs.empty()) {
    swapDatabases = true;
    swapCollections = true;
    swapViews = true;
    swapAnalyzers = true;
  }

  for (auto const& database : changeSet.dbs) {
    if (database.first.empty()) {  // Rest of plan
      continue;
    }

    name = database.first;
    auto dbSlice = database.second->slice()[0];
    std::initializer_list<std::string_view> const dbPath{
        AgencyCommHelper::path(), "Plan", "Databases", name};

    // Dropped from Plan?
    if (!dbSlice.hasKey(dbPath)) {
      std::shared_ptr<VPackBuilder const> plan;
      {
        READ_LOCKER(guard, _planProt.lock);
        if (auto it = _plan.find(name); it != _plan.end()) {
          plan = it->second;
        }
      }
      if (plan != nullptr) {
        std::initializer_list<std::string_view> const colPath{
            AgencyCommHelper::path(), "Plan", "Collections", name};
        if (plan->slice()[0].hasKey(colPath)) {
          for (auto col : VPackObjectIterator(plan->slice()[0].get(colPath))) {
            if (col.value.hasKey("shards")) {
              for (auto shard : VPackObjectIterator(col.value.get("shards"))) {
                auto const& shardName = shard.key.copyString();
                ShardID shardID{shardName};
                newShards.erase(shardName);
                newShardsToPlanServers.erase(shardID);
                newShardToName.erase(shardID);
                newShardToShardGroupLeader.erase(shardID);
                newShardGroups.erase(shardID);
              }
            }
          }
        }
      }
      newDatabases.erase(name);
      newStuffByDatabase.erase(name);
      newPlan.erase(name);
      continue;
    }

    dbSlice = dbSlice.get(dbPath);
    bool const isBuilding = dbSlice.hasKey(StaticStrings::AttrIsBuilding);

    // We create the database object on the coordinator here, because
    // it is used to create LogicalCollection instances further
    // down in this function.
    if (isCoordinator && !isBuilding && !databaseFeature.existsDatabase(name)) {
      // database does not yet exist, create it now

      // create a local database object...
      CreateDatabaseInfo info(_server, ExecContext::current());
      // validation of the database creation parameters should have happened
      // already when we get here. whenever we get here, we have a database
      // in the plan with whatever settings.
      // we should not make the validation fail here, because that can lead
      // to all sorts of problems later on if _new_ servers join the cluster
      // that validate _existing_ databases. this must not fail.
      info.strictValidation(false);

      // do not validate database names for existing databases.
      // the rationale is that if a database was already created with
      // an extended name, we should not declare it invalid and abort
      // the startup once the extended names option is turned off.
      info.validateNames(false);

      if (!arangodb::replication2::EnableReplication2) {
        // We right now cannot run a replication2 database on a coordinator
        // that has replication2 disabled
        if (info.replicationVersion() == arangodb::replication::Version::TWO) {
          LOG_TOPIC("8fdd8", FATAL, Logger::REPLICATION2)
              << "Replication version 2 is disabled in this binary, but "
                 "loading a "
                 "version 2 database "
              << "(named '" << info.getName() << "'). "
              << "Creating such databases is disabled. Please dump the data, "
                 "and "
                 "recreate the database with replication version 1 (the "
                 "default), "
                 "and then restore the data.";
          FATAL_ERROR_EXIT();
        }
      }

      Result res = info.load(dbSlice, VPackSlice::emptyArraySlice());

      if (res.fail()) {
        LOG_TOPIC("94357", ERR, Logger::AGENCY)
            << "validating data for local database '" << name
            << "' failed: " << res.errorMessage();
      } else {
        std::string dbName = info.getName();
        TRI_vocbase_t* vocbase = nullptr;
        res = databaseFeature.createDatabase(std::move(info), vocbase);
        events::CreateDatabase(dbName, res, ExecContext::current());

        if (res.fail()) {
          LOG_TOPIC("91870", ERR, Logger::AGENCY)
              << "creating local database '" << name
              << "' failed: " << res.errorMessage();
        }
      }
    }

    // On a coordinator we can only see databases that have been fully created
    if (isCoordinator && isBuilding) {
      buildingDatabases.emplace(std::move(name));
    } else {
      newDatabases[std::move(name)] = dbSlice;
    }
  }

  // Since we have few types of view requiring initialization we perform
  // dedicated loop to ensure database involved are properly cleared
  for (auto const& database : changeSet.dbs) {
    // TODO Why database name can be emtpy
    if (!database.first.empty()) {
      _newPlannedViews.erase(database.first);
    }
  }
  // Immediate children of "Views" are database names, then ids
  // of views, then one JSON object with the description:
  // "Plan":{"Views": {
  //  "_system": {
  //    "654321": {
  //      "id": "654321",
  //      "name": "v",
  //      "collections": [
  //        <list of cluster-wide collection IDs of linked collections>
  //      ]
  //    },...
  //  },...
  //  }}
  auto ensureViews = [&](std::string_view type) {
    for (auto const& database : changeSet.dbs) {
      // TODO Why database name can be emtpy
      if (database.first.empty()) {  // Rest of plan
        continue;
      }
      auto const& databaseName = database.first;
      std::initializer_list<std::string_view> const viewsPath{
          AgencyCommHelper::path(), "Plan", "Views", databaseName};
      auto viewsSlice = database.second->slice()[0];
      viewsSlice = viewsSlice.get(viewsPath);
      if (viewsSlice.isNone()) {
        continue;
      }

      auto vocbase = databaseFeature.useDatabase(databaseName);

      if (!vocbase) {
        // No database with this name found.
        // We have an invalid state here.
        LOG_TOPIC("f105f", WARN, Logger::AGENCY)
            << "No database '" << databaseName << "' found,"
            << " corresponding view will be ignored for now and the "
            << "invalid information will be repaired. VelocyPack: "
            << viewsSlice.toJson();
        // cannot find vocbase for defined views (allow empty views for missing
        // vocbase)
        planValid &= !viewsSlice.length();
        continue;
      }

      for (auto const& viewPairSlice :
           velocypack::ObjectIterator(viewsSlice, true)) {
        auto const& viewSlice = viewPairSlice.value;

        if (!viewSlice.isObject()) {
          LOG_TOPIC("2487b", INFO, Logger::AGENCY)
              << "View entry is not a valid json object."
              << " The view will be ignored for now and the invalid "
              << "information will be repaired. VelocyPack: "
              << viewSlice.toJson();
          continue;
        }
        auto const typeSlice = viewSlice.get(StaticStrings::DataSourceType);
        if (!typeSlice.isString() || typeSlice.stringView() != type) {
          continue;
        }
        auto const viewId = viewPairSlice.key.copyString();

        try {
          LogicalView::ptr view;
          auto res = LogicalView::instantiate(view, *vocbase,
                                              viewPairSlice.value, false);

          if (!res.ok() || !view) {
            LOG_TOPIC("b0d48", ERR, Logger::AGENCY)
                << "Failed to create view '" << viewId
                << "'. The view will be ignored for now and the invalid "
                << "information will be repaired. VelocyPack: "
                << viewSlice.toJson();
            planValid = false;  // view creation failure
            continue;
          }

          auto& views = _newPlannedViews[databaseName];

          // register with guid/id/name
          // TODO Maybe assert view.id == view.planId == viewId?
          // In that case we can use map<uint64_t, ...> for mapping id -> view
          // and map<string_view, ...> for name/guid -> view
          views.reserve(views.size() + 3);
          views[viewId] = view;
          views[view->name()] = view;
          views[view->guid()] = view;
        } catch (std::exception const& ex) {
          // The Plan contains invalid view information.
          // This should not happen in healthy situations.
          // If it happens in unhealthy situations the
          // cluster should not fail.
          LOG_TOPIC("ec9e6", ERR, Logger::AGENCY)
              << "Failed to load information for view '" << viewId
              << "': " << ex.what() << ". invalid information in Plan. The "
              << "view will be ignored for now and the invalid "
              << "information will be repaired. VelocyPack: "
              << viewSlice.toJson();
        } catch (...) {
          // The Plan contains invalid view information.
          // This should not happen in healthy situations.
          // If it happens in unhealthy situations the
          // cluster should not fail.
          LOG_TOPIC("660bf", ERR, Logger::AGENCY)
              << "Failed to load information for view '" << viewId
              << ". invalid information in Plan. The view will "
              << "be ignored for now and the invalid information will "
              << "be repaired. VelocyPack: " << viewSlice.toJson();
        }
      }
    }
  };

  // Ensure "arangosearch" views are being created BEFORE collections
  // to allow collection's links find them
  ensureViews(iresearch::StaticStrings::ViewArangoSearchType);

  // "Plan":{"Analyzers": {
  //  "_system": {
  //    "Revision": 0,
  //    "BuildingRevision": 0,
  //    "Coordinator": "",
  //    "RebootID": 0
  //  },...
  // }}
  // Now the same for analyzers:
  for (auto const& database : changeSet.dbs) {
    if (database.first.empty()) {  // Rest of plan
      continue;
    }
    auto const& databaseName = database.first;
    auto analyzerSlice = database.second->slice()[0];

    std::initializer_list<std::string_view> const analyzersPath{
        AgencyCommHelper::path(), "Plan", "Analyzers", databaseName};
    if (!analyzerSlice.hasKey(analyzersPath)) {  // DB Gone
      newDbAnalyzersRevision.erase(databaseName);
      continue;
    }
    analyzerSlice = analyzerSlice.get(analyzersPath);

    auto vocbase = databaseFeature.useDatabase(databaseName);

    if (!vocbase) {
      // No database with this name found.
      // We have an invalid state here.
      LOG_TOPIC("e5a6b", WARN, Logger::AGENCY)
          << "No database '" << databaseName << "' found,"
          << " corresponding analyzer will be ignored for now and the "
          << "invalid information will be repaired. VelocyPack: "
          << analyzerSlice.toJson();
      // cannot find vocbase for defined analyzers (allow empty analyzers for
      // missing vocbase)
      planValid &= !analyzerSlice.length();

      continue;
    }

    std::string revisionError;
    auto revision =
        AnalyzersRevision::fromVelocyPack(analyzerSlice, revisionError);
    if (revision) {
      newDbAnalyzersRevision[databaseName] = std::move(revision);
    } else {
      LOG_TOPIC("e3f08", WARN, Logger::AGENCY)
          << "Invalid analyzer data for database '" << databaseName << "'"
          << " Error:" << revisionError << ", "
          << " corresponding analyzers revision will be ignored for now and "
             "the "
          << "invalid information will be repaired. VelocyPack: "
          << analyzerSlice.toJson();
    }
  }
  TRI_IF_FAILURE("AlwaysSwapAnalyzersRevision") { swapAnalyzers = true; }

  decltype(_replicatedLogs) newReplicatedLogs{_resourceMonitor};
  decltype(_collectionGroups) newCollectionGroups{_resourceMonitor};

  static_assert(
      std::uses_allocator_v<NewStuffByDatabase,
                            ClusterInfoResourceAllocator<NewStuffByDatabase>>);

  // And now for replicated logs
  for (auto const& [databaseName, query] : changeSet.dbs) {
    if (databaseName.empty()) {
      continue;
    }

    auto stuff = allocateShared<NewStuffByDatabase>();
    {
      auto replicatedLogsPaths = cluster::paths::aliases::plan()
                                     ->replicatedLogs()
                                     ->database(databaseName)
                                     ->vec();

      auto logsSlice = query->slice()[0].get(replicatedLogsPaths);
      if (!logsSlice.isNone()) {
        ReplicatedLogsMap newLogs{_resourceMonitor};
        for (auto const& [idString, logSlice] :
             VPackObjectIterator(logsSlice)) {
          auto spec =
              std::make_shared<replication2::agency::LogPlanSpecification>(
                  velocypack::deserialize<
                      replication2::agency::LogPlanSpecification>(logSlice));
          newLogs.emplace(spec->id, std::move(spec));
        }
        stuff->replicatedLogs = std::move(newLogs);
      }
    }

    {
      auto collectionGroupsPath = std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "CollectionGroups", databaseName};
      auto groupsSlice = query->slice()[0].get(collectionGroupsPath);
      if (!groupsSlice.isNone()) {
        CollectionGroupMap groups{_resourceMonitor};
        for (auto const& [idString, groupSlice] :
             VPackObjectIterator(groupsSlice)) {
          auto spec = allocateShared<
              replication2::agency::CollectionGroupPlanSpecification>();
          velocypack::deserialize(groupSlice, *spec);
          groups.emplace(spec->id, std::move(spec));
        }
        stuff->collectionGroups = std::move(groups);
      }
    }

    newStuffByDatabase[databaseName] = std::move(stuff);
  }

  for (auto& dbs : newStuffByDatabase) {
    for (auto& it : dbs.second->replicatedLogs) {
      newReplicatedLogs.emplace(it.first, it.second);
    }
    for (auto& it : dbs.second->collectionGroups) {
      newCollectionGroups.emplace(it.first, it.second);
    }
  }

  // Immediate children of "Collections" are database names, then ids
  // of collections, then one JSON object with the description:
  // "Plan":{"Collections": {
  //  "_system": {
  //    "3010001": {
  //      "deleted": false,
  //      "id": "3010001",
  //      "indexes": [
  //        {
  //          "fields": [
  //            "_key"
  //          ],
  //          "id": "0",
  //          "sparse": false,
  //          "type": "primary",
  //          "unique": true
  //        }
  //      ],
  //      "isSmart": false,
  //      "isSystem": true,
  //      "keyOptions": {
  //        "allowUserKeys": true,
  //        "lastValue": 0,
  //        "type": "traditional"
  //      },
  //      "name": "_graphs",
  //      "numberOfShards": 1,
  //      "path": "",
  //      "replicationFactor": 2,
  //      "shardKeys": [
  //        "_key"
  //      ],
  //      "shards": {
  //        "s3010002": [
  //          "PRMR-bf44d6fe-e31c-4b09-a9bf-e2df6c627999",
  //          "PRMR-11a29830-5aca-454b-a2c3-dac3a08baca1"
  //        ]
  //      },
  //      "status": 3,
  //      "statusString": "loaded",
  //      "type": 2,
  //      "waitForSync": false
  //    },...
  //  },...
  // }}
  bool cleanupLinkResponsible{ServerState::instance()->isCoordinator() &&
                              !changeSet.dbs.empty()};
  if (cleanupLinkResponsible) {
    auto const myId = ServerState::instance()->getId();
    auto ctors = getCurrentCoordinators();
    for (auto const& s : ctors) {
      if (_rebootTracker.isServerAlive(s) && s < myId) {
        cleanupLinkResponsible = false;
        break;
      }
    }
    LOG_TOPIC_IF("567be", TRACE, Logger::CLUSTER, cleanupLinkResponsible)
        << "This server is responsible for dangling links cleanup.";
  }
  for (auto const& database : changeSet.dbs) {
    if (database.first.empty()) {  // Rest of plan
      continue;
    }

    auto const& databaseName = database.first;

    auto collectionsSlice = database.second->slice()[0];

    std::vector<std::string_view> collectionsPath{
        AgencyCommHelper::path(), "Plan", "Collections", databaseName};
    if (!collectionsSlice.hasKey(collectionsPath)) {
      if (auto it = _newPlannedCollections.find(databaseName);
          it != _newPlannedCollections.end()) {
        for (auto const& collection : *(it->second)) {
          auto& collectionId = collection.first;
          // delete from maps with shardID as key
          newShards.erase(collectionId);
          if (auto maybeShardID = ShardID::shardIdFromString(collectionId);
              maybeShardID.ok()) {
            auto const& shardId = maybeShardID.get();
            // The list contains collections and shards by name and id.
            // So it is expected that some are not valid shard ids.
            // Make sure we only erase valid shard ids.
            newShardToName.erase(shardId);
            newShardToDb.erase(shardId);
          }
        }
        _newPlannedCollections.erase(it);
      }
      continue;
    }

    // Skip databases that are still building.
    if (buildingDatabases.contains(databaseName)) {
      continue;
    }

    collectionsSlice = collectionsSlice.get(collectionsPath);

    auto vocbase = databaseFeature.useDatabase(databaseName);

    if (!vocbase) {
      // No database with this name found.
      // We have an invalid state here.
      LOG_TOPIC("83d4c", DEBUG, Logger::AGENCY)
          << "No database '" << databaseName << "' found,"
          << " corresponding collection will be ignored for now and the "
          << "invalid information will be repaired. VelocyPack: "
          << collectionsSlice.toJson();
      // cannot find vocbase for defined collections
      // (allow empty collections for missing vocbase)
      planValid &= !collectionsSlice.length();
      continue;
    }

    auto databaseCollections = allocateShared<DatabaseCollections>();

    // an iterator to all collections in the current database (from the previous
    // round) we can safely keep this iterator around because we hold the
    // read-lock on _planProt here. reusing the lookup positions helps avoiding
    // redundant lookups into _plannedCollections for the same database

    AllCollections::const_iterator existingCollections;
    {
      READ_LOCKER(guard, _planProt.lock);
      existingCollections = _plannedCollections.find(databaseName);
    }

    if (auto stillExistingCollections =
            _newPlannedCollections.find(databaseName);
        stillExistingCollections != _newPlannedCollections.end()) {
      if (auto np = newPlan.find(databaseName); np != newPlan.end()) {
        auto nps = np->second->slice()[0];
        for (auto const& ec : *(stillExistingCollections->second)) {
          auto const& cid = ec.first;
          if (!std::isdigit(cid.front())) {
            continue;
          }
          collectionsPath.push_back(cid);
          if (!nps.hasKey(collectionsPath)) {  // collection gone
            collectionsPath.emplace_back("shards");
            READ_LOCKER(guard, _planProt.lock);
            TRI_ASSERT(_plan.contains(databaseName));
            for (auto sh : VPackObjectIterator(_plan.find(databaseName)
                                                   ->second->slice()[0]
                                                   .get(collectionsPath))) {
              auto const& shardId = sh.key.copyString();
              ShardID sId{shardId};
              newShards.erase(shardId);
              newShardsToPlanServers.erase(sId);
              newShardToName.erase(sId);
              newShardToDb.erase(sId);
              // We try to erase the shard ID anyway, no problem if it is
              // not in there, should it be a shard group leader!
              newShardToShardGroupLeader.erase(sId);
              newShardGroups.erase(sId);
            }
            collectionsPath.pop_back();
          }
          collectionsPath.pop_back();
        }
      }
    }

    for (auto collectionPairSlice :
         velocypack::ObjectIterator(collectionsSlice)) {
      auto collectionSlice = collectionPairSlice.value;

      if (!collectionSlice.isObject()) {
        LOG_TOPIC("0f689", WARN, Logger::AGENCY)
            << "Collection entry is not a valid json object."
            << " The collection will be ignored for now and the invalid "
            << "information will be repaired. VelocyPack: "
            << collectionSlice.toJson();

        continue;
      }
      auto const collectionId = collectionPairSlice.key.copyString();

      VPackBuilder newSliceBuilder;
      if (auto gid = collectionSlice.get("groupId"); !gid.isNone()) {
        auto group = newCollectionGroups.at(
            replication2::agency::CollectionGroupId{gid.getUInt()});
        {
          VPackObjectBuilder ob(&newSliceBuilder);
          newSliceBuilder.add(VPackObjectIterator(collectionSlice));
          newSliceBuilder.add(
              StaticStrings::NumberOfShards,
              group->attributes.immutableAttributes.numberOfShards);
          newSliceBuilder.add(StaticStrings::WriteConcern,
                              group->attributes.mutableAttributes.writeConcern);
          newSliceBuilder.add(StaticStrings::WaitForSyncString,
                              group->attributes.mutableAttributes.waitForSync);
          newSliceBuilder.add(
              StaticStrings::ReplicationFactor,
              group->attributes.mutableAttributes.replicationFactor);
          if (collectionId != group->groupLeader) {
            newSliceBuilder.add(StaticStrings::DistributeShardsLike,
                                group->groupLeader);
          }
        }
        collectionSlice = newSliceBuilder.slice();
      }

      bool const isBuilding =
          isCoordinator &&
          basics::VelocyPackHelper::getBooleanValue(
              collectionSlice, StaticStrings::AttrIsBuilding, false);

      // check if we already know this collection (i.e. have it in our local
      // cache). we do this to avoid rebuilding LogicalCollection objects from
      // scratch in every iteration the cache check is very coarse-grained: it
      // simply hashes the Plan VelocyPack data for the collection, and will
      // only reuse a collection from the cache if the hash is identical.
      CollectionWithHash cwh = buildCollection(
          isBuilding, existingCollections, collectionId, collectionSlice,
          *vocbase, changeSet.version, cleanupLinkResponsible);
      auto& newCollection = cwh.collection;
      TRI_ASSERT(newCollection != nullptr);

      try {
        auto& collectionName = newCollection->name();

        // NOTE: This is building has the following feature. A collection needs
        // to be working on all DBServers to allow replication to go on, also we
        // require to have the shards planned. BUT: The users should not be able
        // to detect these collections. Hence we simply do NOT add the
        // collection to the coordinator local vocbase, which happens inside the
        // IF

        if (!isBuilding) {
          // register with name as well as with id:
          databaseCollections->try_emplace(collectionName, cwh);
          databaseCollections->try_emplace(collectionId, cwh);
        }

        auto shardIDs = newCollection->shardIds();
        auto shards = allocateShared<std::vector<ShardID>>();
        shards->reserve(shardIDs->size());
        newShardToName.reserve(newShardToName.size() + shardIDs->size());
        newShardToDb.reserve(newShardToDb.size() + shardIDs->size());

        for (auto const& p : *shardIDs) {
          shards->push_back(p.first);
          auto v = allocateShared<ManagedVector<pmr::ServerID>>();
          v->assign(p.second.begin(), p.second.end());
          newShardsToPlanServers.insert_or_assign(p.first, std::move(v));
          newShardToName.insert_or_assign(p.first, newCollection->name());
          newShardToDb.insert_or_assign(p.first, databaseName);
        }

        // Sort by the number in the shard ID ("s0000001" for example):
        std::sort(shards->begin(), shards->end());
        newShards.insert_or_assign(collectionId, std::move(shards));
      } catch (std::exception const& ex) {
        // The plan contains invalid collection information.
        // This should not happen in healthy situations.
        // If it happens in unhealthy situations the
        // cluster should not fail.
        LOG_TOPIC("359f3", ERR, Logger::AGENCY)
            << "Failed to load information for collection '" << collectionId
            << "': " << ex.what() << ". invalid information in plan. The "
            << "collection will be ignored for now and the invalid "
            << "information will be repaired. VelocyPack: "
            << collectionSlice.toJson();

        TRI_ASSERT(false);
        continue;
      } catch (...) {
        // The plan contains invalid collection information.
        // This should not happen in healthy situations.
        // If it happens in unhealthy situations the
        // cluster should not fail.
        LOG_TOPIC("5f3d5", ERR, Logger::AGENCY)
            << "Failed to load information for collection '" << collectionId
            << ". invalid information in plan. The collection will "
            << "be ignored for now and the invalid information will "
            << "be repaired. VelocyPack: " << collectionSlice.toJson();

        TRI_ASSERT(false);
        continue;
      }
    }
    // Now that the loop is completed, we have to run through it one more
    // time to get the shard groups done:
    for (auto const& colPair : *databaseCollections) {
      if (colPair.first ==
          std::string_view{colPair.second.collection->name()}) {
        // Every collection shows up once with its ID and once with its name.
        // We only want it once, so we only take it when we see the ID, not
        // the name as key:
        continue;
      }
      auto const& groupLeader = std::invoke([&]() -> std::string {
        if (colPair.second.collection->replicationVersion() ==
            replication::Version::TWO) {
          // For Replication2 we cannot call distributeShardsLike() because it
          // will look into the objects we are just building here.
          if (auto it = newCollectionGroups.find(
                  colPair.second.collection->groupID());
              it != newCollectionGroups.end()) {
            auto const& leader = it->second->groupLeader;
            if (leader == colPair.second.collection->name()) {
              return StaticStrings::Empty;
            }
            return leader;
          }
          TRI_ASSERT(false) << "newCollectionGroups is required to contain the "
                               "collection we investigate.";
          return StaticStrings::Empty;
        } else {
          return colPair.second.collection->distributeShardsLike();
        }
      });
      if (!groupLeader.empty()) {
        if (auto groupLeaderCol = newShards.find(groupLeader);
            groupLeaderCol != newShards.end()) {
          if (auto col = newShards.find(
                  std::to_string(colPair.second.collection->id().id()));
              col != newShards.end()) {
            auto logicalColToBeCreated = colPair.second.collection;
            if (col->second->size() == 0 ||
                (logicalColToBeCreated->isSmart() &&
                 logicalColToBeCreated->type() == TRI_COL_TYPE_EDGE)) {
              // Can happen for smart edge collections. But in this case we
              // can ignore the collection.
              continue;
            }
            TRI_ASSERT(groupLeaderCol->second->size() == col->second->size());
            for (size_t i = 0; i < col->second->size(); ++i) {
              newShardToShardGroupLeader.try_emplace(
                  col->second->at(i), groupLeaderCol->second->at(i));
              if (auto it = newShardGroups.find(groupLeaderCol->second->at(i));
                  it == newShardGroups.end()) {
                // Need to create a new list:
                auto list = allocateShared<ManagedVector<ShardID>>();
                list->reserve(2);
                // group leader as well as member:
                list->emplace_back(groupLeaderCol->second->at(i));
                list->emplace_back(col->second->at(i));
                newShardGroups.try_emplace(groupLeaderCol->second->at(i),
                                           std::move(list));
              } else {
                // Need to add us to the list:
                it->second->emplace_back(col->second->at(i));
              }
            }
          } else {
            LOG_TOPIC("12f32", WARN, Logger::CLUSTER)
                << "loadPlan: Strange, could not find collection: "
                << colPair.second.collection->name();
          }
        } else {
          LOG_TOPIC("22312", WARN, Logger::CLUSTER)
              << "loadPlan: Strange, could not find proto collection: "
              << groupLeader;
        }
      }
    }
    _newPlannedCollections.insert_or_assign(databaseName,
                                            std::move(databaseCollections));
  }

  // Ensure "search-alias" views are being created AFTER collections
  // to allow views find collection's inverted indexes
  if (ServerState::instance()->isCoordinator()) {
    ensureViews(iresearch::StaticStrings::ViewSearchAliasType);
  }

  if (isCoordinator) {
    auto systemDB = _server.getFeature<SystemDatabaseFeature>().use();
    if (systemDB &&
        systemDB->shardingPrototype() == ShardingPrototype::Undefined) {
      // system database does not have a shardingPrototype set...
      // sharding prototype of _system database defaults to _users nowadays
      systemDB->setShardingPrototype(ShardingPrototype::Users);
      if (auto it = _newPlannedCollections.find(StaticStrings::SystemDatabase);
          it != _newPlannedCollections.end()) {
        // but for "old" databases it may still be "_graphs". we need to find
        // out! find _system database in Plan. Replication TWO databases cannot
        // be old.
        if (systemDB->replicationVersion() == replication::Version::ONE) {
          // find _graphs collection in Plan
          if (auto it2 = it->second->find(StaticStrings::GraphCollection);
              it2 != it->second->end()) {
            // found!
            if (it2->second.collection->distributeShardsLike().empty()) {
              // _graphs collection has no distributeShardsLike, so it is
              // the prototype!
              systemDB->setShardingPrototype(ShardingPrototype::Graphs);
            }
          }
        }

        // The systemDB does initially set default values.
        // As they can change with startup parameters we need
        // to overwrite hem here to align with existing properties
        // and all participants in the cluster agree on
        // same definition.
        if (newPlan.contains(StaticStrings::SystemDatabase)) {
          auto planSlice = newPlan[StaticStrings::SystemDatabase]->slice();
          if (planSlice.isArray() && planSlice.length() == 1) {
            if (planSlice.at(0).isObject()) {
              auto entrySlice = planSlice.at(0);
              {
                // Add sharding Attribute
                auto path = std::vector<std::string>{
                    "arango", "Plan", "Databases",
                    StaticStrings::SystemDatabase, StaticStrings::Sharding};
                if (auto value = entrySlice.get(path); value.isString()) {
                  systemDB->setSharding(value.copyString());
                }
              }
              {
                // Add replication version
                auto path =
                    std::vector<std::string>{"arango", "Plan", "Databases",
                                             StaticStrings::SystemDatabase,
                                             StaticStrings::ReplicationVersion};
                if (auto value = entrySlice.get(path); value.isString()) {
                  auto res = replication::parseVersion(value.stringView());
                  // Just care for valid Replication versions now
                  if (res.ok() && res.get() != systemDB->replicationVersion()) {
                    // We cannot change the replication version of the system
                    // database The system database is created during startup,
                    // using the default option There is a timeframe where new
                    // collections are already created for this database, and
                    // use the "wrong" replicationVersion. This crashes on the
                    // way before this codepoint is reached and could repair the
                    // system Database.
                    LOG_TOPIC("50b83", FATAL, arangodb::Logger::STARTUP)
                        << "Changed option "
                           "'--database.default-replication-version' between "
                           "startups. (Was "
                        << value.stringView() << ", is now set to "
                        << replication::versionToString(
                               systemDB->replicationVersion())
                        << "). This would break the _system database. If you "
                           "want your _system database to use a different "
                           "replication version you need to start with an "
                           "empty cluster and restore data.";
                    FATAL_ERROR_EXIT();
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  WRITE_LOCKER(writeLocker, _planProt.lock);

  _planVersion = changeSet.version;
  _planIndex = changeSet.ind;
  _plan.swap(newPlan);

  // update memory usage
  std::uint64_t memoryUsage = 0;
  for (auto const& it : _plan) {
    TRI_ASSERT(it.second != nullptr);
    memoryUsage += it.second->slice().byteSize();
  }
  _resourceMonitor.decreaseMemoryUsage(_planMemoryUsage);
  _resourceMonitor.increaseMemoryUsage(memoryUsage);
  _planMemoryUsage = memoryUsage;

  LOG_TOPIC("54321", DEBUG, Logger::CLUSTER)
      << "Updating ClusterInfo plan: version=" << _planVersion
      << " index=" << _planIndex;

  if (swapDatabases) {
    _plannedDatabases.swap(newDatabases);
  }

  if (swapCollections) {
    _plannedCollections.swap(_newPlannedCollections);
    _shards.swap(newShards);
    _shardsToPlanServers.swap(newShardsToPlanServers);
    _shardToShardGroupLeader.swap(newShardToShardGroupLeader);
    _shardGroups.swap(newShardGroups);
    _shardToName.swap(newShardToName);
    _shardToDb.swap(newShardToDb);
    _pendingCleanups.swap(_currentCleanups);
  }

  if (swapViews) {
    _plannedViews.swap(_newPlannedViews);
  }
  if (swapAnalyzers) {
    _dbAnalyzersRevision.swap(newDbAnalyzersRevision);
  }

  _newStuffByDatabase.swap(newStuffByDatabase);
  _replicatedLogs.swap(newReplicatedLogs);
  _collectionGroups.swap(newCollectionGroups);

  if (planValid) {
    _planProt.isValid = true;
  }

  _clusterFeature.addDirty(changeSet.dbs);

  {
    std::lock_guard w(_waitPlanLock);
    triggerWaiting(_waitPlan, _planIndex);
    auto heartbeatThread = _clusterFeature.heartbeatThread();
    if (heartbeatThread) {
      // In the unittests, there is no heartbeat thread, and we do not need to
      // notify
      heartbeatThread->notify();
    }
  }

  {
    std::lock_guard w(_waitPlanVersionLock);
    triggerWaiting(_waitPlanVersion, _planVersion);
  }

  auto diff = duration<float, std::milli>(clock::now() - start).count();
  _lpTimer.count(diff);

  TRI_ASSERT(_planIndex == changeSet.ind);
  return changeSet.ind;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

auto ClusterInfo::loadCurrent() -> consensus::index_t {
  using namespace std::chrono;
  using clock = std::chrono::high_resolution_clock;

  auto start = clock::now();

  // reread from the agency!
  std::lock_guard mutexLocker{
      _currentProt.mutex};  // only one may work at a time

  uint64_t currentIndex;
  uint64_t currentVersion;
  {
    READ_LOCKER(guard, _currentProt.lock);
    currentIndex = _currentIndex;
    currentVersion = _currentVersion;
  }

  auto const changeSet = _agencyCache.changedSince("Current", currentIndex);
  bool const changed = !changeSet.dbs.empty() || changeSet.rest != nullptr;

  // early exit if nothing changed
  // Note: I don't think checking for Current/Version in addition to `changed`
  //       is of any use, and that it could be removed.
  if (!changed && currentVersion == changeSet.version) {
    auto const newCurrentIndex = changeSet.ind;
    {
      WRITE_LOCKER(writeLocker, _currentProt.lock);
      _currentIndex = changeSet.ind;
    }
    if (currentIndex < newCurrentIndex) {
      std::lock_guard w(_waitCurrentLock);
      triggerWaiting(_waitCurrent, newCurrentIndex);
      auto heartbeatThread = _clusterFeature.heartbeatThread();
      if (heartbeatThread) {
        // In the unittests, there is no heartbeat thread, and we do not need to
        // notify
        heartbeatThread->notify();
      }
    }
    return newCurrentIndex;
  }

  // We need to update ServersKnown to notice rebootId changes for all servers.
  // To keep things simple and separate, we call loadServers here instead of
  // trying to integrate the servers upgrade code into loadCurrent, even if that
  // means small bits of the plan are read twice.
  loadServers();

  decltype(_current) newCurrent{_resourceMonitor};
  {
    READ_LOCKER(readLocker, _currentProt.lock);
    newCurrent = _current;
    for (auto const& db : changeSet.dbs) {  // Databases
      newCurrent[db.first] = db.second;
    }
    if (changeSet.rest != nullptr) {  // Rest
      newCurrent[std::string_view{}] = changeSet.rest;
    }
  }

  decltype(_currentDatabases) newDatabases{_resourceMonitor};
  decltype(_currentCollections) newCollections{_resourceMonitor};
  decltype(_shardsToCurrentServers) newShardsToCurrentServers{_resourceMonitor};

  {
    READ_LOCKER(guard, _currentProt.lock);
    newDatabases = _currentDatabases;
    newCollections = _currentCollections;
    newShardsToCurrentServers = _shardsToCurrentServers;
  }

  bool swapDatabases = false;
  bool swapCollections = false;

  // Current/Databases
  for (auto const& database : changeSet.dbs) {
    auto const databaseName = database.first;
    if (databaseName.empty()) {
      continue;
    }
    swapDatabases = true;

    std::initializer_list<std::string_view> const dbPath{
        AgencyCommHelper::path(), "Current", "Databases", databaseName};
    auto databaseSlice = database.second->slice()[0];

    // Database missing in Current
    if (!databaseSlice.hasKey(dbPath)) {
      // newShardIds
      std::shared_ptr<VPackBuilder const> db;
      {
        READ_LOCKER(guard, _currentProt.lock);
        if (auto it = _current.find(databaseName); it != _current.end()) {
          db = it->second;
        }
      }
      std::initializer_list<std::string_view> const colPath{
          AgencyCommHelper::path(), "Current", "Collections", databaseName};

      if (db != nullptr) {
        if (db->slice()[0].hasKey(colPath)) {
          auto const colsSlice = db->slice()[0].get(colPath);
          if (colsSlice.isObject()) {
            for (auto cc : VPackObjectIterator(colsSlice)) {
              if (cc.value.isObject()) {
                for (auto cs : VPackObjectIterator(cc.value)) {
                  newShardsToCurrentServers.erase(ShardID{cs.key.stringView()});
                }
              }
            }
          }
        }
      }
      continue;
    }
    databaseSlice = databaseSlice.get(dbPath);

    auto serverList =
        allocateShared<FlatMap<pmr::ServerID, velocypack::Slice>>();
    if (databaseSlice.isObject()) {
      for (auto const& serverSlicePair : VPackObjectIterator(databaseSlice)) {
        serverList->try_emplace(serverSlicePair.key.copyString(),
                                serverSlicePair.value);
      }
    }

    newDatabases.insert_or_assign(databaseName, std::move(serverList));
  }

  // Current/Collections
  for (auto const& database : changeSet.dbs) {
    auto const databaseName = database.first;
    if (databaseName.empty()) {
      continue;
    }
    swapCollections = true;

    std::initializer_list<std::string_view> const dbPath{
        AgencyCommHelper::path(), "Current", "Collections", databaseName};
    auto databaseSlice = database.second->slice()[0];
    if (!databaseSlice.hasKey(dbPath)) {
      newCollections.erase(databaseName);
      continue;
    }
    databaseSlice = databaseSlice.get(dbPath);

    auto databaseCollections = allocateShared<DatabaseCollectionsCurrent>();

    if (auto existingCollections = newCollections.find(databaseName);
        existingCollections != newCollections.end()) {
      if (auto nc = newCurrent.find(databaseName); nc != newCurrent.end()) {
        auto ncs = nc->second->slice()[0];
        std::vector<std::string> path{AgencyCommHelper::path(), "Current",
                                      "Collections", databaseName};
        for (auto const& ec : *existingCollections->second) {
          auto const& cid = ec.first;
          path.emplace_back(cid);
          if (ncs.hasKey(path)) {
            std::shared_ptr<VPackBuilder const> cur;
            {
              READ_LOCKER(guard, _currentProt.lock);
              TRI_ASSERT(_current.contains(databaseName));
              cur = _current.find(databaseName)->second;
            }
            auto const cc = cur->slice()[0].get(path);
            for (auto const& sh : VPackObjectIterator(cc)) {
              path.push_back(sh.key.copyString());
              if (!ncs.hasKey(path)) {
                ShardID shardID{path.back()};
                newShardsToCurrentServers.erase(shardID);
              }
              path.pop_back();
            }
            path.pop_back();
          }
        }
      }
    }

    for (auto const& collectionSlice : VPackObjectIterator(databaseSlice)) {
      std::string collectionName = collectionSlice.key.copyString();

      auto collectionDataCurrent =
          std::make_shared<CollectionInfoCurrent>(changeSet.version);

      for (auto const& shardSlice :
           velocypack::ObjectIterator(collectionSlice.value)) {
        auto maybeShardID =
            ShardID::shardIdFromString(shardSlice.key.stringView());
        if (ADB_UNLIKELY(maybeShardID.fail())) {
          TRI_ASSERT(false)
              << "Indexed malformed shard name " << shardSlice.key.stringView();
          // TODO cannot handle this entry, is it better to continue or to abort
          // here?
          continue;
        }

        auto shardID = maybeShardID.get();
        collectionDataCurrent->add(shardID, shardSlice.value);

        // Note that we have only inserted the CollectionInfoCurrent under
        // the collection ID and not under the name! It is not possible
        // to query the current collection info by name. This is because
        // the correct place to hold the current name is in the plan.
        // Thus: Look there and get the collection ID from there. Then
        // ask about the current collection info.

        // Now take note of this shard and its responsible server:
        auto const& xx = collectionDataCurrent->servers(shardID);
        auto servers = allocateShared<ManagedVector<pmr::ServerID>>();
        servers->assign(xx.begin(), xx.end());
        newShardsToCurrentServers.insert_or_assign(std::move(shardID),
                                                   std::move(servers));
        TRI_IF_FAILURE("ClusterInfo::loadCurrentSeesLeader") {
          if (!xx.empty()) {  // just in case
            std::string myShortName = ServerState::instance()->getShortName();
            observeGlobalEvent("ClusterInfo::loadCurrentSeesLeader",
                               absl::StrCat(myShortName, ":",
                                            std::string{shardID}, ":", xx[0]));
          }
        }
      }

      databaseCollections->try_emplace(std::move(collectionName),
                                       std::move(collectionDataCurrent));
    }

    newCollections.insert_or_assign(databaseName,
                                    std::move(databaseCollections));
  }

  // Now set the new value:
  WRITE_LOCKER(writeLocker, _currentProt.lock);

  _current.swap(newCurrent);
  std::uint64_t memoryUsage = 0;
  for (auto const& it : _current) {
    TRI_ASSERT(it.second != nullptr);
    memoryUsage += it.second->slice().byteSize();
  }
  _resourceMonitor.decreaseMemoryUsage(_currentMemoryUsage);
  _resourceMonitor.increaseMemoryUsage(memoryUsage);
  _currentMemoryUsage = memoryUsage;

  _currentVersion = changeSet.version;
  _currentIndex = changeSet.ind;
  LOG_TOPIC("feddd", TRACE, Logger::CLUSTER)
      << "Updating current in ClusterInfo: version=" << changeSet.version
      << " index=" << _currentIndex;

  if (swapDatabases) {
    _currentDatabases.swap(newDatabases);
  }

  if (swapCollections) {
    LOG_TOPIC("b4059", TRACE, Logger::CLUSTER)
        << "Have loaded new collections current cache!";
    _currentCollections.swap(newCollections);
    _shardsToCurrentServers.swap(newShardsToCurrentServers);
  }

  _currentProt.isValid = true;
  _clusterFeature.addDirty(changeSet.dbs);

  {
    std::lock_guard w(_waitCurrentLock);
    triggerWaiting(_waitCurrent, _currentIndex);
    auto heartbeatThread = _clusterFeature.heartbeatThread();
    if (heartbeatThread) {
      // In the unittests, there is no heartbeat thread, and we do not need to
      // notify
      heartbeatThread->notify();
    }
  }

  {
    std::lock_guard w(_waitCurrentVersionLock);
    triggerWaiting(_waitCurrentVersion, _currentVersion);
  }

  auto diff = duration<float, std::milli>(clock::now() - start).count();
  _lcTimer.count(diff);

  TRI_IF_FAILURE("ClusterInfo::loadCurrentDone") {
    observeGlobalEvent("ClusterInfo::loadCurrentDone",
                       ServerState::instance()->getShortName());
  }

  TRI_ASSERT(_currentIndex == changeSet.ind);
  return changeSet.ind;
}

/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once
/// if the collection is not found afterwards, this method will throw an
/// exception
std::shared_ptr<LogicalCollection> ClusterInfo::getCollection(
    std::string_view databaseID, std::string_view collectionID) {
  auto c = getCollectionNT(databaseID, collectionID);

  if (c == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        getCollectionNotFoundMsg(databaseID, collectionID)  // message
    );
  }
  return c;
}

std::shared_ptr<LogicalCollection> ClusterInfo::getCollectionNT(
    std::string_view databaseID, std::string_view collectionID) {
  auto lookupCollection =
      [&](auto const& collections) -> std::shared_ptr<LogicalCollection> {
    // look up database by id
    if (auto it = collections.find(databaseID); it != collections.end()) {
      // look up collection by id (or by name)
      if (auto it2 = it->second->find(collectionID); it2 != it->second->end()) {
        return it2->second.collection;
      }
    }
    return nullptr;
  };

  if (std::this_thread::get_id() == _planLoader) {
    // we're loading plan, lookup inside immediately created planned data
    // sources already protected by _planProt.mutex, don't need to lock there
    return lookupCollection(_newPlannedCollections);
  }

  if (!_planProt.isValid) {
    Result r = waitForPlan(1).waitAndGet();
    if (r.fail()) {
      return nullptr;
    }
  }

  READ_LOCKER(readLocker, _planProt.lock);
  return lookupCollection(_plannedCollections);
}

std::shared_ptr<LogicalDataSource> ClusterInfo::getCollectionOrViewNT(
    std::string_view databaseID, std::string_view name) {
  auto lookupDataSource =
      [&](auto const& collections,
          auto const& views) -> std::shared_ptr<LogicalDataSource> {
    // look up collection first
    {
      // look up database by id
      if (auto it = collections.find(databaseID); it != collections.end()) {
        // look up collection by id (or by name)
        if (auto it2 = it->second->find(name); it2 != it->second->end()) {
          return it2->second.collection;
        }
      }
    }

    // look up views next
    {
      // look up database by id
      if (auto it = views.find(databaseID); it != views.end()) {
        // look up collection by id (or by name)
        if (auto it2 = it->second.find(name); it2 != it->second.end()) {
          return it2->second;
        }
      }
    }
    return nullptr;
  };

  if (std::this_thread::get_id() == _planLoader) {
    // we're loading plan, lookup inside immediately created planned data
    // sources already protected by _planProt.mutex, don't need to lock there
    return lookupDataSource(_newPlannedCollections, _newPlannedViews);
  }

  if (!_planProt.isValid) {
    Result r = waitForPlan(1).waitAndGet();
    if (r.fail()) {
      return nullptr;
    }
  }

  READ_LOCKER(readLocker, _planProt.lock);
  return lookupDataSource(_plannedCollections, _plannedViews);
}

std::string ClusterInfo::getCollectionNotFoundMsg(
    std::string_view databaseID, std::string_view collectionID) {
  return absl::StrCat("Collection not found: ", collectionID, " in database ",
                      databaseID);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalCollection>> ClusterInfo::getCollections(
    std::string_view databaseID) {
  std::vector<std::shared_ptr<LogicalCollection>> result;

  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  if (auto it = _plannedCollections.find(databaseID);
      it != _plannedCollections.end()) {
    // iterate over all collections
    for (auto& e : *it->second) {
      char c = e.first[0];
      if (c < '0' || c > '9') {
        // skip collections indexed by id
        result.push_back(e.second.collection);
      }
    }
  }

  return result;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Generate Collection Stubs during Database buiding
/// These stubs are no real collection, use this API with care
/// and only if the database is in building state.
//////////////////////////////////////////////////////////////////////////////

[[nodiscard]] std::unordered_map<std::string,
                                 std::shared_ptr<LogicalCollection>>
ClusterInfo::generateCollectionStubs(TRI_vocbase_t& database) {
  std::unordered_map<std::string, std::shared_ptr<LogicalCollection>> result;

  // TODO: Make this an AgencyPath object
  std::string collectionsPath = "Plan/Collections/" + database.name();
  VPackBuilder collectionsBuilder;

  // We really do not care for the Index.
  std::ignore = _agencyCache.get(collectionsBuilder, collectionsPath);
  auto collectionsSlice = collectionsBuilder.slice();
  for (auto const& [cid, colData] : VPackObjectIterator(collectionsSlice)) {
    auto collection =
        database.createCollectionObject(colData, /*isAStub*/ true);
    TRI_ASSERT(collection != nullptr);
    result.emplace(collection->name(), collection);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection in current. This returns information about
/// all shards in the collection.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CollectionInfoCurrent> ClusterInfo::getCollectionCurrent(
    std::string_view databaseID, std::string_view collectionID) {
  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  READ_LOCKER(readLocker, _currentProt.lock);
  // look up database by id
  if (auto it = _currentCollections.find(databaseID);
      it != _currentCollections.end()) {
    // look up collection by id
    if (auto it2 = it->second->find(collectionID); it2 != it->second->end()) {
      return it2->second;
    }
  }

  return std::make_shared<CollectionInfoCurrent>(0);
}

RebootTracker& ClusterInfo::rebootTracker() noexcept { return _rebootTracker; }

RebootTracker const& ClusterInfo::rebootTracker() const noexcept {
  return _rebootTracker;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Test if all names (Collection & Views) are available  in the given
/// database and return the planVersion this can be guaranteed on.
//////////////////////////////////////////////////////////////////////////////

ResultT<uint64_t> ClusterInfo::checkDataSourceNamesAvailable(
    std::string_view databaseName, std::vector<std::string> const& names) {
  READ_LOCKER(readLocker, _planProt.lock);
  // Load all collections of the Database
  auto colList = _plannedCollections.find(databaseName);
  if (colList == _plannedCollections.end()) {
    // If there are no collections in the Database, it is not there.
    // So we are in the create New database case.
    // We will protect against deleted database with Preconditions
    return {_planVersion};
  }

  auto viewList = _plannedViews.find(databaseName);
  for (auto const& name : names) {
    if (colList->second->contains(name) ||
        (viewList != _plannedViews.end() && viewList->second.contains(name))) {
      // Either a Collection or a view is known with this name. Disallow it.
      return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME,
                    absl::StrCat("duplicate collection name '", name, "'"));
    }
  }
  return {_planVersion};
}

//////////////////////////////////////////////////////////////////////////////
/// @brief ask about a view
/// If it is not found in the cache, the cache is reloaded once. The second
/// argument can be a view ID or a view name (both cluster-wide).
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LogicalView> ClusterInfo::getView(std::string_view databaseID,
                                                  std::string_view viewID) {
  if (viewID.empty()) {
    return nullptr;
  }

  auto lookupView =
      [&](AllViews const& dbs) noexcept -> std::shared_ptr<LogicalView> {
    // look up database by id
    if (auto db = dbs.find(databaseID); db != dbs.end()) {
      // look up view by id (or by name)
      auto& views = db->second;
      if (auto view = views.find(viewID); view != views.end()) {
        return view->second;
      }
    }

    return nullptr;
  };

  if (std::this_thread::get_id() == _planLoader) {
    // we're loading plan, lookup inside immediately created planned views
    // already protected by _planProt.mutex, don't need to lock there
    return lookupView(_newPlannedViews);
  }

  if (!_planProt.isValid) {
    return nullptr;
  }

  {
    READ_LOCKER(readLocker, _planProt.lock);
    auto view = lookupView(_plannedViews);

    if (view) {
      return view;
    }
  }

  Result res =
      fetchAndWaitForPlanVersion(std::chrono::seconds(10)).waitAndGet();
  if (res.ok()) {
    READ_LOCKER(readLocker, _planProt.lock);
    auto view = lookupView(_plannedViews);

    if (view) {
      return view;
    }
  }

  LOG_TOPIC("a227e", DEBUG, Logger::CLUSTER)
      << "View not found: '" << viewID << "' in database '" << databaseID
      << "'";

  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief ask about all views of a database
//////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalView>> ClusterInfo::getViews(
    std::string_view databaseID) {
  std::vector<std::shared_ptr<LogicalView>> result;

  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  if (auto it = _plannedViews.find(databaseID); it != _plannedViews.end()) {
    // iterate over all collections
    for (auto& e : it->second) {
      char c = e.first[0];
      if (c >= '0' && c <= '9') {
        // skip views indexed by name
        result.emplace_back(e.second);
      }
    }
  }

  return result;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief ask about analyzers revision
//////////////////////////////////////////////////////////////////////////////

AnalyzersRevision::Ptr ClusterInfo::getAnalyzersRevision(
    std::string_view databaseID, bool forceLoadPlan /* = false */) {
  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  if (auto it = _dbAnalyzersRevision.find(databaseID);
      it != _dbAnalyzersRevision.cend()) {
    return it->second;
  }
  return nullptr;
}

QueryAnalyzerRevisions ClusterInfo::getQueryAnalyzersRevision(
    std::string_view databaseID) {
  if (!_planProt.isValid) {
    loadPlan();
  }
  AnalyzersRevision::Revision currentDbRevision{AnalyzersRevision::MIN};
  AnalyzersRevision::Revision systemDbRevision{AnalyzersRevision::MIN};
  // no looping here. As if cluster is freshly updated some databases will
  // never have revisions record (and they do not need one actually)
  // so waiting them to apper is futile. Anyway if database has revision
  // we will see it at best effort basis as soon as plan updates itself
  // and some lag in revision appearing is expected (even with looping )
  {
    READ_LOCKER(readLocker, _planProt.lock);
    // look up database by id
    if (auto it = _dbAnalyzersRevision.find(databaseID);
        it != _dbAnalyzersRevision.cend()) {
      currentDbRevision = it->second->getRevision();
    }
    // analyzers from system also available
    // so grab revision for system database as well
    if (databaseID != StaticStrings::SystemDatabase) {
      // if we have non-system database in plan system should be here for sure!
      // but for freshly updated cluster this is not true so, check is necessary
      if (auto sysIt = _dbAnalyzersRevision.find(StaticStrings::SystemDatabase);
          sysIt != _dbAnalyzersRevision.cend()) {
        systemDbRevision = sysIt->second->getRevision();
      }
    } else {
      // micro-optimization. If we are querying system database
      // than current always equal system. And all requests for revision
      // will be resolved only with systemDbRevision member. So we copy
      // current to system and set current to MIN. As MIN value is default
      // and not transferred at all we will reduce json size for query
      systemDbRevision = currentDbRevision;
      currentDbRevision = AnalyzersRevision::MIN;
    }
  }

  return QueryAnalyzerRevisions(currentDbRevision, systemDbRevision);
}

/// @brief get shard statistics for the specified database
Result ClusterInfo::getShardStatisticsForDatabase(
    std::string const& dbName, std::string_view restrictServer,
    VPackBuilder& builder) const {
  auto [acb, idx] = _agencyCache.read(std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections/" + dbName)});

  velocypack::Slice databaseSlice =
      acb->slice()[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections", dbName});

  if (!databaseSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  containers::FlatHashSet<std::string> servers;
  ShardStatistics stats;
  addToShardStatistics(stats, servers, databaseSlice, restrictServer);
  stats.servers = servers.size();

  stats.toVelocyPack(builder);
  return {};
}

/// @brief get shard statistics for all databases, totals,
/// optionally restricted to the specified server
Result ClusterInfo::getShardStatisticsGlobal(std::string const& restrictServer,
                                             VPackBuilder& builder) const {
  if (!restrictServer.empty() &&
      (!serverExists(restrictServer) ||
       !ClusterHelpers::isDBServerName(restrictServer))) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid DBserver id");
  }

  auto [acb, idx] = _agencyCache.read(
      std::vector<std::string>{AgencyCommHelper::path("Plan/Collections")});

  velocypack::Slice databasesSlice =
      acb->slice()[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections"});

  if (!databasesSlice.isObject()) {
    return Result(TRI_ERROR_INTERNAL, "invalid Plan structure");
  }

  containers::FlatHashSet<std::string> servers;
  ShardStatistics stats;

  for (auto db : VPackObjectIterator(databasesSlice)) {
    addToShardStatistics(stats, servers, db.value, restrictServer);
  }
  stats.servers = servers.size();

  stats.toVelocyPack(builder);
  return {};
}

/// @brief get shard statistics for all databases, separate for each database,
/// optionally restricted to the specified server
Result ClusterInfo::getShardStatisticsGlobalDetailed(
    std::string const& restrictServer, VPackBuilder& builder) const {
  if (!restrictServer.empty() &&
      (!serverExists(restrictServer) ||
       !ClusterHelpers::isDBServerName(restrictServer))) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid DBserver id");
  }

  auto [acb, idx] = _agencyCache.read(
      std::vector<std::string>{AgencyCommHelper::path("Plan/Collections")});

  velocypack::Slice databasesSlice =
      acb->slice()[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections"});

  if (!databasesSlice.isObject()) {
    return Result(TRI_ERROR_INTERNAL, "invalid Plan structure");
  }

  containers::FlatHashSet<std::string> servers;

  builder.openObject();
  for (auto db : VPackObjectIterator(databasesSlice)) {
    servers.clear();
    ShardStatistics stats;
    addToShardStatistics(stats, servers, db.value, restrictServer);
    stats.servers = servers.size();

    builder.add(VPackValue(db.key.copyString()));
    stats.toVelocyPack(builder);
  }
  builder.close();

  return {};
}

/// @brief get shard statistics for all databases, split by servers.
Result ClusterInfo::getShardStatisticsGlobalByServer(
    VPackBuilder& builder) const {
  auto [acb, idx] = _agencyCache.read(
      std::vector<std::string>{AgencyCommHelper::path("Plan/Collections")});

  velocypack::Slice databasesSlice =
      acb->slice()[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections"});

  if (!databasesSlice.isObject()) {
    return Result(TRI_ERROR_INTERNAL, "invalid Plan structure");
  }

  // TODO(MBkkt) FlatHashMap<K, unique_ptr<V>>
  containers::NodeHashMap<ServerID, ShardStatistics> stats;
  {
    // create empty static objects for each DB server
    READ_LOCKER(readLocker, _dbServersProt.lock);
    for (auto& it : _dbServers) {
      stats.emplace(it.first, ShardStatistics{});
    }
  }

  for (auto db : VPackObjectIterator(databasesSlice)) {
    addToShardStatistics(stats, db.value);
  }

  builder.openObject();
  for (auto& it : stats) {
    builder.add(VPackValue(it.first));
    auto& stat = it.second;
    stat.servers = 1;
    stat.toVelocyPack(builder);
  }
  builder.close();

  return {};
}

// Build the VPackSlice that contains the `isBuilding` entry
void ClusterInfo::buildIsBuildingSlice(CreateDatabaseInfo const& database,
                                       VPackBuilder& builder) {
  VPackObjectBuilder guard(&builder);
  database.toVelocyPack(builder);

  builder.add(StaticStrings::AttrCoordinator,
              VPackValue(ServerState::instance()->getId()));
  builder.add(StaticStrings::AttrCoordinatorRebootId,
              VPackValue(ServerState::instance()->getRebootId().value()));
  builder.add(StaticStrings::AttrIsBuilding, VPackValue(true));
}

// Build the VPackSlice that does not contain  the `isBuilding` entry
void ClusterInfo::buildFinalSlice(CreateDatabaseInfo const& database,
                                  VPackBuilder& builder) {
  VPackObjectBuilder guard(&builder);
  database.toVelocyPack(builder);
}

// This waits for the database described in `database` to turn up in `Current`
// and no DBServer is allowed to report an error.
Result ClusterInfo::waitForDatabaseInCurrent(
    CreateDatabaseInfo const& database, AgencyWriteTransaction const& trx) {
  auto DBServers =
      std::make_shared<std::vector<ServerID>>(getCurrentDBServers());
  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  // make capture explicit as callback might be called after the return
  // of this function. So beware of lifetime for captured objects!
  std::function<bool(VPackSlice result)> dbServerChanged =
      [errMsg, dbServerResult, DBServers](VPackSlice result) {
        size_t numDbServers = DBServers->size();
        if (result.isObject() && result.length() >= numDbServers) {
          // We use >= here since the number of DBservers could have increased
          // during the creation of the database and we might not yet have
          // the latest list. Thus there could be more reports than we know
          // servers.
          VPackObjectIterator dbs(result);

          std::string tmpMsg;
          bool tmpHaveError = false;

          for (VPackObjectIterator::ObjectPair dbserver : dbs) {
            VPackSlice slice = dbserver.value;
            if (basics::VelocyPackHelper::getBooleanValue(
                    slice, StaticStrings::Error, false)) {
              tmpHaveError = true;
              absl::StrAppend(&tmpMsg, " DBServer:", dbserver.key.stringView(),
                              ":",
                              basics::VelocyPackHelper::getStringValue(
                                  slice, StaticStrings::ErrorMessage, ""));
              if (auto errorNum = slice.get(StaticStrings::ErrorNum);
                  errorNum.isNumber()) {
                absl::StrAppend(&tmpMsg,
                                " (errorNum=", errorNum.getNumber<uint32_t>(),
                                ")");
              }
            }
          }
          if (tmpHaveError) {
            *errMsg = "Error in creation of database:" + tmpMsg;
            dbServerResult->store(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE,
                                  std::memory_order_release);
            return true;
          }
          dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
        }
        return true;
      };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
      _server, "Current/Databases/" + database.getName(), dbServerChanged, true,
      false);
  Result r = _agencyCallbackRegistry.registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }
  auto cbGuard = scopeGuard([&]() noexcept {
    try {
      _agencyCallbackRegistry.unregisterCallback(agencyCallback);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e952f", ERR, Logger::CLUSTER)
          << "Failed to unregister agency callback: " << ex.what();
    }
  });

  // TODO: Should this never timeout?
  AgencyComm ac(_server);
  auto res = ac.sendTransactionWithFailover(trx, 0.0);
  if (!res.successful()) {
    if (res._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return Result(
          TRI_ERROR_ARANGO_DUPLICATE_NAME,
          absl::StrCat("duplicate database name '", database.getName(), "'"));
    }
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN);
  }

  if (VPackSlice resultsSlice = res.slice().get("results");
      resultsSlice.length() > 0) {
    Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
    if (r.fail()) {
      return r;
    }
  }

  // Waits for the database to turn up in Current/Databases
  {
    double const interval = getPollInterval();

    int count = 0;  // this counts, when we have to reload the DBServers
    while (true) {
      if (++count >=
          static_cast<int>(getReloadServerListTimeout() / interval)) {
        // We update the list of DBServers every minute in case one of them
        // was taken away since we last looked. This also helps (slightly)
        // if a new DBServer was added. However, in this case we report
        // success a bit too early, which is not too bad.
        loadCurrentDBServers();
        *DBServers = getCurrentDBServers();
        count = 0;
      }

      auto tmpRes = dbServerResult->load(std::memory_order_acquire);

      // An error was detected on one of the DBServers
      if (tmpRes.has_value()) {
        cbGuard.fire();  // unregister cb before accessing errMsg

        return Result(*tmpRes, *errMsg);
      }

      {
        std::lock_guard locker{agencyCallback->_cv.mutex};
        agencyCallback->executeByCallbackOrTimeout(
            getReloadServerListTimeout() / interval);
      }

      if (_server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

// Start creating a database in a coordinator by entering it into Plan/Databases
// with, status flag `isBuilding`; this makes the database invisible to the
// outside world.
Result ClusterInfo::createIsBuildingDatabaseCoordinator(
    CreateDatabaseInfo const& database) {
  AgencyCommResult res;

  // Instruct the Agency to enter the creation of the new database
  // by entering it into Plan/Databases/ but with the fields
  // isBuilding: true, and coordinator and rebootId set to our respective
  // id and rebootId
  VPackBuilder builder;
  buildIsBuildingSlice(database, builder);

  AgencyWriteTransaction trx(
      {AgencyOperation("Plan/Databases/" + database.getName(),
                       AgencyValueOperationType::SET, builder.slice()),
       AgencyOperation("Plan/Version",
                       AgencySimpleOperationType::INCREMENT_OP)},
      {AgencyPrecondition("Plan/Databases/" + database.getName(),
                          AgencyPrecondition::Type::EMPTY, true),
       AgencyPrecondition(analyzersPath(database.getName()),
                          AgencyPrecondition::Type::EMPTY, true)});

  // And wait for our database to show up in `Current/Databases`
  auto waitResult = waitForDatabaseInCurrent(database, trx);

  if (waitResult.is(TRI_ERROR_ARANGO_DUPLICATE_NAME) ||
      waitResult.is(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN)) {
    // Early exit without cancellation if we did not do anything
    return waitResult;
  }

  if (waitResult.ok()) {
    return waitResult;
  }

  // cleanup: remove database from plan
  auto ret = cancelCreateDatabaseCoordinator(database);

  if (ret.fail()) {
    // Cleanup failed too
    return ret;
  }
  // Cleanup ok, but creation failed
  return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE,
                "database creation failed");
}

// Finalize creation of database in cluster by removing isBuilding, coordinator,
// and coordinatorRebootId; as precondition that the entry we put in
// createIsBuildingDatabaseCoordinator is still in Plan/ unchanged.
Result ClusterInfo::createFinalizeDatabaseCoordinator(
    CreateDatabaseInfo const& database) {
  AgencyComm ac(_server);

  VPackBuilder pcBuilder;
  buildIsBuildingSlice(database, pcBuilder);

  VPackBuilder entryBuilder;
  buildFinalSlice(database, entryBuilder);

  VPackBuilder analyzersBuilder;
  AgencyComm::buildInitialAnalyzersSlice(analyzersBuilder);

  AgencyWriteTransaction trx(
      {AgencyOperation("Plan/Databases/" + database.getName(),
                       AgencyValueOperationType::SET, entryBuilder.slice()),
       AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP),
       AgencyOperation(analyzersPath(database.getName()),
                       AgencyValueOperationType::SET,
                       analyzersBuilder.slice())},
      {AgencyPrecondition("Plan/Databases/" + database.getName(),
                          AgencyPrecondition::Type::VALUE, pcBuilder.slice()),
       AgencyPrecondition(analyzersPath(database.getName()),
                          AgencyPrecondition::Type::EMPTY, true)});

  auto res = ac.sendTransactionWithFailover(trx, 0.0);

  if (!res.successful()) {
    if (res._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE,
                    "Could not finish creation of database: Plan/Databases/ "
                    "entry was modified in Agency");
    }
    // Something else went wrong.
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE);
  }

  Result r;
  if (VPackSlice resultsSlice = res.slice().get("results");
      resultsSlice.length() > 0) {
    r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
  }

  // The transaction was successful and the database should
  // now be visible and usable.
  return r;
}

// This function can only return on success or when the cluster is shutting
// down.
Result ClusterInfo::cancelCreateDatabaseCoordinator(
    CreateDatabaseInfo const& database) {
  AgencyComm ac(_server);

  VPackBuilder builder;
  buildIsBuildingSlice(database, builder);

  // delete all collections and the database itself from the agency plan
  AgencyOperation delPlanCollections("Plan/Collections/" + database.getName(),
                                     AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanDatabase("Plan/Databases/" + database.getName(),
                                  AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrPlan("Plan/Version",
                           AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition preCondition("Plan/Databases/" + database.getName(),
                                  AgencyPrecondition::Type::VALUE,
                                  builder.slice());

  AgencyWriteTransaction trx({delPlanCollections, delPlanDatabase, incrPlan},
                             preCondition);

  size_t tries = 0;
  double nextTimeout = 0.5;

  AgencyCommResult res;
  while (true) {
    tries++;
    res = ac.sendTransactionWithFailover(trx, nextTimeout);

    if (res.successful()) {
      break;
    }

    if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      auto [acb, index] = _agencyCache.read(std::vector<std::string>{
          AgencyCommHelper::path("Plan/Databases/" + database.getName())});

      velocypack::Slice databaseSlice =
          acb->slice()[0].get(std::initializer_list<std::string_view>{
              AgencyCommHelper::path(), "Plan", "Databases",
              database.getName()});

      if (!databaseSlice.isObject()) {
        // database key in agency does _not_ exist. this can happen if on
        // another coordinator the database gets dropped while on this
        // coordinator we are still trying to create it
        break;
      }

      VPackSlice agencyId = databaseSlice.get("id");
      VPackSlice preconditionId = builder.slice().get("id");
      if (agencyId.isString() && preconditionId.isString() &&
          !agencyId.isEqualString(preconditionId.stringView())) {
        // database key is there, but has a different id, this can happen if the
        // database has already been dropped in the meantime and recreated, in
        // any case, let's get us out of here...
        break;
      }
    }

    if (tries == 1) {
      events::CreateDatabase(database.getName(), res.asResult(),
                             ExecContext::current());
    }

    if (_server.isStopping()) {
      return Result(TRI_ERROR_SHUTTING_DOWN);
    }

    if (tries >= 5) {
      nextTimeout = 5.0;
    }

    LOG_TOPIC("b47aa", WARN, Logger::CLUSTER)
        << "failed to cancel creation of database " << database.getName()
        << " with error " << res.errorMessage() << ". Retrying.";

    // enhancing our calm a bit here, so this does not put the agency under
    // pressure too much.
    TRI_ASSERT(nextTimeout > 0.0 && nextTimeout <= 5.0);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(unsigned(1000.0 * nextTimeout)));
  }

  return Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::dropDatabaseCoordinator(  // drop database
    std::string const& name,                  // database name
    double timeout                            // request timeout
) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  if (name == StaticStrings::SystemDatabase) {
    return Result(TRI_ERROR_FORBIDDEN);
  }
  AgencyComm ac(_server);

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();
  auto collections = getCollections(name);

  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  // make capture explicit as callback might be called after the return
  // of this function. So beware of lifetime for captured objects!
  std::function<bool(VPackSlice result)> dbServerChanged =
      [dbServerResult](VPackSlice result) {
        if (result.isNone() || result.isEmptyObject()) {
          dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
        }
        return true;
      };

  std::string where("Current/Databases/" + name);

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
      _server, where, dbServerChanged, true, false);
  Result r = _agencyCallbackRegistry.registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }

  auto cbGuard = scopeGuard([this, &agencyCallback]() noexcept {
    try {
      _agencyCallbackRegistry.unregisterCallback(agencyCallback);
    } catch (std::exception const& ex) {
      LOG_TOPIC("1ec9b", ERR, Logger::CLUSTER)
          << "Failed to unregister agency callback: " << ex.what();
    }
  });

  // Transact to agency
  AgencyOperation delPlanDatabases("Plan/Databases/" + name,
                                   AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delTargetCollections("Target/Collections/" + name,
                                       AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delTargetCollectionNames(
      "Target/CollectionNames/" + name, AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanCollections("Plan/Collections/" + name,
                                     AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delTargetCollectionsGroup(
      "Target/CollectionGroups/" + name, AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanCollectionsGroups(
      "Plan/CollectionGroups/" + name, AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delTargetReplicatedLogs("Target/ReplicatedLogs/" + name,
                                          AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanReplicatedLogs("Plan/ReplicatedLogs/" + name,
                                        AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanViews("Plan/Views/" + name,
                               AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanAnalyzers(analyzersPath(name),
                                   AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition databaseExists("Plan/Databases/" + name,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans(
      {delPlanDatabases, delTargetCollections, delTargetCollectionNames,
       delPlanCollections, delTargetReplicatedLogs, delPlanReplicatedLogs,
       delTargetCollectionsGroup, delPlanCollectionsGroups, delPlanViews,
       delPlanAnalyzers, incrementVersion},
      databaseExists);
  AgencyCommResult res = ac.sendTransactionWithFailover(trans);
  if (!res.successful()) {
    if (res._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN);
  }
  if (VPackSlice resultsSlice = res.slice().get("results");
      resultsSlice.length() > 0) {
    Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
    if (r.fail()) {
      return r;
    }
  }

  auto replicatedStatesCleanup = futures::Future<Result>{std::in_place};
  if (!collections.empty() &&
      collections.front()->replicationVersion() == replication::Version::TWO) {
    std::vector<replication2::LogId> replicatedStates;
    std::set<CollectionID> collectionIds;

    VPackBuilder groupsBuilder;
    std::ignore =
        _agencyCache.get(groupsBuilder, "Plan/CollectionGroups/" + name);
    auto groupsSlice = groupsBuilder.slice();
    for (auto const& group : VPackObjectIterator(groupsSlice)) {
      auto collectionGroup = velocypack::deserialize<
          replication2::agency::CollectionGroupPlanSpecification>(group.value);
      for (auto const& shardSheaf : collectionGroup.shardSheaves) {
        replicatedStates.emplace_back(shardSheaf.replicatedLog);
      }
      for (auto const& [colId, _] : collectionGroup.collections) {
        collectionIds.emplace(colId);
      }
    }

    for (auto const& collection : collections) {
      if (collectionIds.contains(std::to_string(collection->id().id()))) {
        // Skip collections that are part of a CollectionGroup
        continue;
      }

      // The following code is there to support collection which are not part of
      // any collection group, soon to be removed
      auto shardIds = collection->shardIds();
      replicatedStates.reserve(replicatedStates.size() + shardIds->size());
      std::transform(
          shardIds->begin(), shardIds->end(),
          std::back_inserter(replicatedStates), [](auto const& shardPair) {
            return LogicalCollection::shardIdToStateId(shardPair.first);
          });
    }
    // replicatedStatesCleanup = deleteReplicatedStates(name, replicatedStates);
  }

  // Now wait stuff in Current to disappear and thus be complete:
  {
    while (true) {
      if (dbServerResult->load(std::memory_order_acquire).has_value() &&
          replicatedStatesCleanup.isReady()) {
        cbGuard.fire();  // unregister cb before calling ac.removeValues(...)
        AgencyOperation delCurrentCollection(
            where, AgencySimpleOperationType::DELETE_OP);
        AgencyOperation incrementCurrentVersion(
            "Current/Version", AgencySimpleOperationType::INCREMENT_OP);
        AgencyWriteTransaction cx(
            {delCurrentCollection, incrementCurrentVersion});
        res = ac.sendTransactionWithFailover(cx);
        if (res.successful() && replicatedStatesCleanup.waitAndGet().ok()) {
          return Result(TRI_ERROR_NO_ERROR);
        }
        return Result(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT);
      }

      if (TRI_microtime() > endTime) {
        logAgencyDump();
        return Result(TRI_ERROR_CLUSTER_TIMEOUT);
      }

      {
        std::lock_guard locker{agencyCallback->_cv.mutex};
        agencyCallback->executeByCallbackOrTimeout(interval);
      }

      if (_server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::dropCollectionCoordinator(  // drop collection
    std::string const& dbName,                  // database name
    std::string const& collectionID,
    double timeout  // request timeout
) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  AgencyComm ac(_server);
  AgencyCommResult res;

  // First check that no other collection has a distributeShardsLike
  // entry pointing to us:
  auto coll = getCollection(dbName, collectionID);
  auto colls = getCollections(dbName);  // reloads plan
  std::vector<std::string> clones;
  for (std::shared_ptr<LogicalCollection> const& p : colls) {
    if (p->distributeShardsLike() == coll->name() ||
        p->distributeShardsLike() == collectionID) {
      clones.push_back(p->name());
    }
  }

  if (!clones.empty()) {
    std::string errorMsg("Collection '");
    errorMsg += coll->name();
    errorMsg += "' must not be dropped while '";
    errorMsg += basics::StringUtils::join(clones, "', '");
    if (clones.size() == 1) {
      errorMsg += "' has ";
    } else {
      errorMsg += "' have ";
    }
    errorMsg += "distributeShardsLike set to '";
    errorMsg += coll->name();
    errorMsg += "'.";

    events::DropCollection(
        dbName, collectionID,
        TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE);
    return Result(  // result
        TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE,  // code
        errorMsg  // message
    );
  }

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();
  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  // Capture only explicitly! Please check lifetime of captured objects
  // as callback might be called after this function returns.
  std::function<bool(VPackSlice result)> dbServerChanged =
      [dbServerResult](VPackSlice result) {
        if (result.isNone() || result.isEmptyObject()) {
          dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
        }
        return true;
      };

  // monitor the entry for the collection
  // Note that generally, for replication2, we should monitor the Plan entry.
  // However, dropping a collection can have potential effects on ongoing
  // transactions. These effects are produced only once the maintenance thread
  // has run. Therefore, we monitor the Current entry, which is updated by the
  // maintenance thread.
  std::string const where =
      "Current/Collections/" + dbName + "/" + collectionID;

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
      _server, where, dbServerChanged, true, false);
  Result r = _agencyCallbackRegistry.registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }

  auto cbGuard = scopeGuard([this, &agencyCallback]() noexcept {
    try {
      _agencyCallbackRegistry.unregisterCallback(agencyCallback);
    } catch (std::exception const& ex) {
      LOG_TOPIC("be7da", ERR, Logger::CLUSTER)
          << "Failed to unregister agency callback: " << ex.what();
    }
  });

  size_t numberOfShards = 0;

  auto [acb, idx] =
      _agencyCache.read(std::vector<std::string>{AgencyCommHelper::path(
          "Plan/Collections/" + dbName + "/" + collectionID)});

  velocypack::Slice databaseSlice =
      acb->slice()[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections", dbName});

  if (!databaseSlice.isObject()) {
    // database dropped in the meantime
    events::DropCollection(dbName, collectionID,
                           TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  velocypack::Slice collectionSlice = databaseSlice.get(collectionID);
  if (!collectionSlice.isObject()) {
    // collection dropped in the meantime
    events::DropCollection(dbName, collectionID,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  velocypack::Slice shardsSlice = collectionSlice.get("shards");
  if (shardsSlice.isObject()) {
    numberOfShards = shardsSlice.length();
  } else {
    LOG_TOPIC("d340d", ERR, Logger::CLUSTER)
        << "Missing shards information on dropping " << dbName << "/"
        << collectionID;

    events::DropCollection(dbName, collectionID,
                           TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  auto trans = std::invoke([&]() {
    if (coll->replicationVersion() == replication::Version::TWO) {
      // Transact to agency
      AgencyOperation delTargetCollection(
          "Target/Collections/" + dbName + "/" + collectionID,
          AgencySimpleOperationType::DELETE_OP);
      AgencyOperation delTargetCollectionGroup(
          "Target/CollectionGroups/" + dbName + "/" +
              std::to_string(coll->groupID().id()) + "/collections/" +
              collectionID,
          AgencySimpleOperationType::DELETE_OP);
      AgencyOperation delTargetCollectionName(
          "Target/CollectionNames/" + dbName + "/" + coll->name(),
          AgencySimpleOperationType::DELETE_OP);
      AgencyOperation incrementVersion(
          "Target/CollectionGroups/" + dbName + "/" +
              std::to_string(coll->groupID().id()) + "/version",
          AgencySimpleOperationType::INCREMENT_OP);
      AgencyPrecondition precondition = AgencyPrecondition(
          "Plan/Databases/" + dbName, AgencyPrecondition::Type::EMPTY, false);
      AgencyWriteTransaction trans(
          {delTargetCollection, incrementVersion, delTargetCollectionGroup,
           delTargetCollectionName},
          precondition);
      return trans;
    } else {
      // Transact to agency
      AgencyOperation delPlanCollection(
          "Plan/Collections/" + dbName + "/" + collectionID,
          AgencySimpleOperationType::DELETE_OP);
      AgencyOperation incrementVersion("Plan/Version",
                                       AgencySimpleOperationType::INCREMENT_OP);
      AgencyPrecondition precondition = AgencyPrecondition(
          "Plan/Databases/" + dbName, AgencyPrecondition::Type::EMPTY, false);
      AgencyWriteTransaction trans({delPlanCollection, incrementVersion},
                                   precondition);
      return trans;
    }
  });
  res = ac.sendTransactionWithFailover(trans);

  if (!res.successful()) {
    if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      LOG_TOPIC("279c5", ERR, Logger::CLUSTER)
          << "Precondition failed for this agency transaction: "
          << trans.toJson() << ", return code: " << res.httpCode();
    }

    logAgencyDump();

    // TODO: this should rather be TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, as the
    // precondition is that the database still exists
    events::DropCollection(dbName, collectionID,
                           TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION);
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION);
  }
  if (VPackSlice resultsSlice = res.slice().get("results");
      resultsSlice.length() > 0) {
    Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
    if (r.fail()) {
      return r;
    }
  }

  if (numberOfShards == 0) {
    events::DropCollection(dbName, collectionID, TRI_ERROR_NO_ERROR);
    return Result(TRI_ERROR_NO_ERROR);
  }

  // TODO Need to wait for CollectionGroupVersion to be propagated
  // :/Current/CollectionGroups/<db>/<gid>/supervision/targetVersion == x
  while (true) {
    auto tmpRes = dbServerResult->load();
    if (tmpRes.has_value()) {
      cbGuard.fire();  // unregister cb before calling ac.removeValues(...)
      // ...remove the entire directory for the collection
      AgencyOperation delCurrentCollection(
          "Current/Collections/" + dbName + "/" + collectionID,
          AgencySimpleOperationType::DELETE_OP);
      AgencyWriteTransaction cx({delCurrentCollection});
      res = ac.sendTransactionWithFailover(cx);
      events::DropCollection(dbName, collectionID, *tmpRes);
      return Result(*tmpRes);
    }

    if (TRI_microtime() > endTime) {
      LOG_TOPIC("76ea6", ERR, Logger::CLUSTER)
          << "Timeout in _drop collection (" << realTimeout << ")"
          << ": database: " << dbName << ", collId:" << collectionID
          << "\ntransaction sent to agency: " << trans.toJson();

      logAgencyDump();

      events::DropCollection(dbName, collectionID, TRI_ERROR_CLUSTER_TIMEOUT);
      return Result(TRI_ERROR_CLUSTER_TIMEOUT);
    }

    {
      std::lock_guard locker{agencyCallback->_cv.mutex};
      agencyCallback->executeByCallbackOrTimeout(interval);
    }

    if (_server.isStopping()) {
      events::DropCollection(dbName, collectionID, TRI_ERROR_SHUTTING_DOWN);
      return Result(TRI_ERROR_SHUTTING_DOWN);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create view in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::createViewCoordinator(  // create view
    std::string const& databaseName,        // database name
    std::string const& viewID, velocypack::Slice json) {
  // TRI_ASSERT(ServerState::instance()->isCoordinator());
  // FIXME TODO is this check required?
  auto const typeSlice = json.get(StaticStrings::DataSourceType);

  if (!typeSlice.isString()) {
    std::string name;
    if (json.isObject()) {
      name = basics::VelocyPackHelper::getStringValue(
          json, StaticStrings::DataSourceName, "");
    }
    events::CreateView(databaseName, name, TRI_ERROR_BAD_PARAMETER);
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  auto const name = basics::VelocyPackHelper::getStringValue(
      json, StaticStrings::DataSourceName, StaticStrings::Empty);

  if (name.empty()) {
    events::CreateView(databaseName, name, TRI_ERROR_BAD_PARAMETER);
    return Result(TRI_ERROR_BAD_PARAMETER);  // must not be empty
  }

  {
    // check if a view with the same name is already planned
    READ_LOCKER(readLocker, _planProt.lock);
    {
      if (auto it = _plannedViews.find(databaseName);
          it != _plannedViews.end()) {
        if (it->second.contains(name)) {
          // view already exists!
          events::CreateView(databaseName, name,
                             TRI_ERROR_ARANGO_DUPLICATE_NAME);
          return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME,
                        absl::StrCat("duplicate view name '", name, "'"));
        }
      }
    }
    {
      // check against planned collections as well
      if (auto it = _plannedCollections.find(databaseName);
          it != _plannedCollections.end()) {
        if (it->second->contains(name)) {
          // collection already exists!
          events::CreateCollection(databaseName, name,
                                   TRI_ERROR_ARANGO_DUPLICATE_NAME);
          return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME,
                        absl::StrCat("duplicate view name '", name, "'"));
        }
      }
    }
  }

  if (!_agencyCache.has("Plan/Databases/" + databaseName)) {
    events::CreateView(databaseName, name, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (_agencyCache.has("Plan/Views/" + databaseName + "/" + viewID)) {
    events::CreateView(databaseName, name, TRI_ERROR_CLUSTER_VIEW_ID_EXISTS);
    return Result(TRI_ERROR_CLUSTER_VIEW_ID_EXISTS);
  }

  AgencyWriteTransaction const transaction{
      // operations
      {{"Plan/Views/" + databaseName + "/" + viewID,
        AgencyValueOperationType::SET, json},
       {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      // preconditions
      {{"Plan/Views/" + databaseName + "/" + viewID,
        AgencyPrecondition::Type::EMPTY, true}}};

  AgencyComm ac(_server);
  auto const res = ac.sendTransactionWithFailover(transaction);

  // Only if not precondition failed
  if (!res.successful()) {
    if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      // Dump agency plan:

      logAgencyDump();

      events::CreateView(databaseName, name,
                         TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN);
      return Result(
          TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN,
          absl::StrCat("Precondition that view ", name, " with ID ", viewID,
                       " does not yet exist failed. Cannot create view."));
    }

    events::CreateView(databaseName, name,
                       TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN);
    return Result(
        TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN,
        basics::StringUtils::concatT(
            "file: ", __FILE__, " line: ", __LINE__,
            " HTTP code: ", static_cast<int>(res.httpCode()),
            " error message: ", res.errorMessage(),
            " error details: ", res.errorDetails(), " body: ", res.body()));
  }

  Result r;
  if (VPackSlice resultsSlice = res.slice().get("results");
      resultsSlice.length() > 0) {
    r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
  }

  events::CreateView(databaseName, name, r.errorNumber());
  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop view in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::dropViewCoordinator(  // drop view
    std::string const& databaseName,      // database name
    std::string const& viewID             // view identifier
) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  // Transact to agency
  AgencyWriteTransaction const trans{
      // operations
      {{"Plan/Views/" + databaseName + "/" + viewID,
        AgencySimpleOperationType::DELETE_OP},
       {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      // preconditions
      {{"Plan/Databases/" + databaseName, AgencyPrecondition::Type::EMPTY,
        false},
       {"Plan/Views/" + databaseName + "/" + viewID,
        AgencyPrecondition::Type::EMPTY, false}}};

  AgencyComm ac(_server);
  auto const res = ac.sendTransactionWithFailover(trans);

  Result result;

  if (res.successful() && res.slice().get("results").length()) {
    result = waitForPlan(res.slice().get("results")[0].getNumber<uint64_t>())
                 .waitAndGet();
  }

  if (!res.successful() && !result.fail()) {
    // FIXME COULD_NOT_REMOVE_VIEW_IN_PLAN
    if (res.errorCode() == TRI_ERROR_HTTP_PRECONDITION_FAILED) {
      result.reset(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,
                   absl::StrCat("Precondition that view  with ID ", viewID,
                                " already exist failed. Cannot create view."));

      // Dump agency plan:
      logAgencyDump();
    } else {
      result = Result(
          TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,
          basics::StringUtils::concatT(
              "file: ", __FILE__, " line: ", __LINE__,
              " HTTP code: ", static_cast<int>(res.httpCode()),
              " error message: ", res.errorMessage(),
              " error details: ", res.errorDetails(), " body: ", res.body()));
    }
  }

  events::DropView(databaseName, viewID, result.errorNumber());

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set view properties in coordinator
////////////////////////////////////////////////////////////////////////////////

Result ClusterInfo::setViewPropertiesCoordinator(
    std::string const& databaseName, std::string const& viewID,
    VPackSlice json) {
  // TRI_ASSERT(ServerState::instance()->isCoordinator());
  auto [acb, index] = _agencyCache.read(std::vector<std::string>{
      AgencyCommHelper::path("Plan/Views/" + databaseName + "/" + viewID)});

  if (!acb->slice()[0].hasKey(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Views", databaseName, viewID})) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  auto const view = acb->slice()[0].get(std::initializer_list<std::string_view>{
      AgencyCommHelper::path(), "Plan", "Views", databaseName, viewID});

  if (!view.isObject()) {
    logAgencyDump();

    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  AgencyWriteTransaction const trans{
      // operations
      {{"Plan/Views/" + databaseName + "/" + viewID,
        AgencyValueOperationType::SET, json},
       {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      // preconditions
      {"Plan/Databases/" + databaseName, AgencyPrecondition::Type::EMPTY,
       false}};

  AgencyComm ac(_server);
  AgencyCommResult res = ac.sendTransactionWithFailover(trans);

  if (!res.successful()) {
    return {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, res.errorMessage()};
  }

  Result r;
  if (VPackSlice resultsSlice = res.slice().get("results");
      resultsSlice.length() > 0) {
    r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
  }
  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start creating or deleting an analyzer in coordinator,
/// the return value is an ArangoDB error code
/// and the errorMsg is set accordingly. One possible error
/// is a timeout.
////////////////////////////////////////////////////////////////////////////////
std::pair<Result, AnalyzersRevision::Revision>
ClusterInfo::startModifyingAnalyzerCoordinator(std::string_view databaseID) {
  VPackBuilder serverIDBuilder;
  serverIDBuilder.add(VPackValue(ServerState::instance()->getId()));

  VPackBuilder rebootIDBuilder;
  rebootIDBuilder.add(
      VPackValue(ServerState::instance()->getRebootId().value()));

  AgencyComm ac(_server);

  AnalyzersRevision::Revision revision;
  auto const endTime =
      TRI_microtime() + getTimeout(checkAnalyzersPreconditionTimeout);

  // do until precondition success or timeout
  do {
    {
      // Get current revision for precondition
      READ_LOCKER(readLocker, _planProt.lock);
      if (auto it = _dbAnalyzersRevision.find(databaseID);
          it != _dbAnalyzersRevision.cend()) {
        revision = it->second->getRevision();
      } else {
        if (TRI_microtime() > endTime) {
          return std::pair{
              Result{TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                     absl::StrCat(
                         "start modifying analyzer: unknown database name '",
                         databaseID, "'")},
              AnalyzersRevision::LATEST};
        }
        // less possible case - we have just updated database so try to write
        // EmptyRevision with preconditions
        {
          VPackBuilder emptyRevision;
          AnalyzersRevision::getEmptyRevision()->toVelocyPack(emptyRevision);
          auto const anPath = analyzersPath(databaseID) + "/";
          AgencyWriteTransaction const transaction{
              {{anPath, AgencyValueOperationType::SET, emptyRevision.slice()},
               {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
              {{anPath, AgencyPrecondition::Type::EMPTY, true}}};
          auto const res = ac.sendTransactionWithFailover(transaction);
          if (res.successful()) {
            auto results = res.slice().get("results");
            if (results.isArray() && results.length() > 0) {
              readLocker.unlock();  // we want to wait for plan to load -
                                    // release reader
              Result r =
                  waitForPlan(results[0].getNumber<uint64_t>()).waitAndGet();
              if (r.fail()) {
                return std::make_pair(r, AnalyzersRevision::LATEST);
              }
            }
          }
        }
        continue;
      }
    }

    VPackBuilder revisionBuilder;
    revisionBuilder.add(VPackValue(revision));

    auto const anPath = analyzersPath(databaseID) + "/";
    AgencyWriteTransaction const transaction{
        {{anPath + StaticStrings::AnalyzersBuildingRevision,
          AgencySimpleOperationType::INCREMENT_OP},
         {anPath + StaticStrings::AttrCoordinator,
          AgencyValueOperationType::SET, serverIDBuilder.slice()},
         {anPath + StaticStrings::AttrCoordinatorRebootId,
          AgencyValueOperationType::SET, rebootIDBuilder.slice()},
         {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
        {{anPath + StaticStrings::AnalyzersBuildingRevision,
          AgencyPrecondition::Type::VALUE, revisionBuilder.slice()}}};

    auto const res = ac.sendTransactionWithFailover(transaction);

    // Only if not precondition failed
    if (!res.successful()) {
      if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
        if (TRI_microtime() > endTime) {
          // Dump agency plan
          logAgencyDump();
          return std::pair{
              Result{TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                     absl::StrCat(
                         "start modifying analyzer precondition for database ",
                         databaseID, ": Revision ", revisionBuilder.toString(),
                         " is not equal to BuildingRevision. Cannot modify an "
                         "analyzer.")},
              AnalyzersRevision::LATEST};
        }

        if (_server.isStopping()) {
          return std::make_pair(Result(TRI_ERROR_SHUTTING_DOWN),
                                AnalyzersRevision::LATEST);
        }

        continue;
      }

      return std::make_pair(
          Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                 basics::StringUtils::concatT(
                     "file: ", __FILE__, " line: ", __LINE__,
                     " HTTP code: ", static_cast<int>(res.httpCode()),
                     " error message: ", res.errorMessage(), " error details: ",
                     res.errorDetails(), " body: ", res.body())),
          AnalyzersRevision::LATEST);
    } else {
      auto results = res.slice().get("results");
      if (results.isArray() && results.length() > 0) {
        Result r = waitForPlan(results[0].getNumber<uint64_t>()).waitAndGet();
        if (r.fail()) {
          return std::make_pair(r, AnalyzersRevision::LATEST);
        }
      }
    }
    break;
  } while (true);

  return std::make_pair(Result(TRI_ERROR_NO_ERROR),
                        revision + 1);  // as INCREMENT_OP succeeded
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finish creating or deleting an analyzer in coordinator,
/// the return value is an ArangoDB error code
/// and the errorMsg is set accordingly. One possible error
/// is a timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::finishModifyingAnalyzerCoordinator(
    std::string_view databaseID, bool restore) {
  TRI_IF_FAILURE("FinishModifyingAnalyzerCoordinator") {
    return Result(TRI_ERROR_DEBUG);
  }

  VPackBuilder serverIDBuilder;
  serverIDBuilder.add(VPackValue(ServerState::instance()->getId()));

  VPackBuilder rebootIDBuilder;
  rebootIDBuilder.add(
      VPackValue(ServerState::instance()->getRebootId().value()));

  AgencyComm ac(_server);

  AnalyzersRevision::Revision revision;
  auto const endTime =
      TRI_microtime() + getTimeout(checkAnalyzersPreconditionTimeout);

  // do until precondition success or timeout
  do {
    {
      // Get current revision for precondition
      READ_LOCKER(readLocker, _planProt.lock);
      if (auto it = _dbAnalyzersRevision.find(databaseID);
          it != _dbAnalyzersRevision.cend()) {
        revision = it->second->getRevision();
      } else {
        return Result(
            TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
            absl::StrCat("finish modifying analyzer: unknown database name '",
                         databaseID, "'"));
      }
    }

    VPackBuilder revisionBuilder;
    revisionBuilder.add(VPackValue(++revision));

    auto const anPath = analyzersPath(databaseID) + "/";
    AgencyWriteTransaction const transaction{
        {[restore, &anPath] {
           if (restore) {
             return AgencyOperation{
                 anPath + StaticStrings::AnalyzersBuildingRevision,
                 AgencySimpleOperationType::DECREMENT_OP};
           }
           return AgencyOperation{anPath + StaticStrings::AnalyzersRevision,
                                  AgencySimpleOperationType::INCREMENT_OP};
         }(),
         {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
        {{anPath + StaticStrings::AnalyzersBuildingRevision,
          AgencyPrecondition::Type::VALUE, revisionBuilder.slice()},
         {anPath + StaticStrings::AttrCoordinator,
          AgencyPrecondition::Type::VALUE, serverIDBuilder.slice()},
         {anPath + StaticStrings::AttrCoordinatorRebootId,
          AgencyPrecondition::Type::VALUE, rebootIDBuilder.slice()}}};

    auto const res = ac.sendTransactionWithFailover(transaction);

    // Only if not precondition failed
    if (!res.successful()) {
      // if preconditions failed -> somebody already finished our revision
      // record. That means agency maintenance already reverted our operation -
      // we must abandon this operation. So it differs from what we do in
      // startModifying.
      if (res.httpCode() != rest::ResponseCode::PRECONDITION_FAILED) {
        if (TRI_microtime() > endTime) {
          // Dump agency plan
          logAgencyDump();

          return Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                        basics::StringUtils::concatT(
                            "file: ", __FILE__, " line: ", __LINE__,
                            " HTTP code: ", static_cast<int>(res.httpCode()),
                            " error message: ", res.errorMessage(),
                            " error details: ", res.errorDetails(),
                            " body: ", res.body()));
        }

        if (_server.isStopping()) {
          return Result(TRI_ERROR_SHUTTING_DOWN);
        }

        continue;
      } else if (restore) {
        break;  // failed precondition means our revert is indirectly
                // successful!
      }
      return Result(
          TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
          "finish modifying analyzer precondition for database " +
              std::string{databaseID} + ": Revision " +
              revisionBuilder.toString() +
              " is not equal to BuildingRevision " + " or " +
              serverIDBuilder.toString() + " is not equal to coordinator or " +
              rebootIDBuilder.toString() +
              " is not equal to coordinatorRebootId. Cannot finish modify "
              "an analyzer.");
    } else {
      auto results = res.slice().get("results");
      if (results.isArray() && results.length() > 0) {
        Result r = waitForPlan(results[0].getNumber<uint64_t>()).waitAndGet();
        if (r.fail()) {
          return r;
        }
      }
    }
    break;
  } while (true);

  return Result(TRI_ERROR_NO_ERROR);
}

void ClusterInfo::initMetricsState() {
  auto rebootId = ServerState::instance()->getRebootId().value();
  auto serverId = ServerState::instance()->getId();

  VPackBuilder builderRebootId;
  builderRebootId.add(VPackValue{rebootId});
  VPackBuilder builderServerId;
  builderServerId.add(VPackValue{serverId});

  AgencyWriteTransaction const write{
      {{kMetricsRebootId, AgencyValueOperationType::SET,
        builderRebootId.slice()},
       {kMetricsServerId, AgencyValueOperationType::SET,
        builderServerId.slice()}},
      {{kMetricsRebootId, AgencyPrecondition::Type::EMPTY, true},
       {kMetricsServerId, AgencyPrecondition::Type::EMPTY, true}}};
  AgencyComm ac{_server};
  while (!server().isStopping()) {
    auto const r = ac.sendTransactionWithFailover(write);
    if (r.successful() ||
        r.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      return;
    }
    LOG_TOPIC("bfdc3", WARN, Logger::CLUSTER)
        << "Failed to self-propose leader with httpCode: " << r.httpCode();
  }
}

ClusterInfo::MetricsState ClusterInfo::getMetricsState(bool wantLeader) {
  auto [result, index] =
      _agencyCache.read({AgencyCommHelper::path(kMetricsServerId),
                         AgencyCommHelper::path(kMetricsRebootId)});
  auto data = result->slice().at(0).get(std::initializer_list<std::string_view>{
      AgencyCommHelper::path(), "Plan", "Metrics"});
  auto leaderRebootId = data.get("RebootId").getNumber<uint64_t>();
  auto leaderServerId = data.get("ServerId").stringView();
  auto ourRebootId = ServerState::instance()->getRebootId().value();
  auto ourServerId = ServerState::instance()->getId();
  if (wantLeader) {
    // remove old callback (with _metricsGuard call)
    // then store new callback or understand we are leader
    _metricsGuard = {};
  }
  if (ourRebootId == leaderRebootId && ourServerId == leaderServerId) {
    return {std::nullopt};
  }
  if (wantLeader) {
    _metricsGuard = _rebootTracker.callMeOnChange(
        {std::string{leaderServerId}, RebootId{leaderRebootId}},
        [this, leaderRebootId, serverId = std::string{leaderServerId}] {
          proposeMetricsLeader(leaderRebootId, serverId);
        },
        "Try to propose current server as a new leader for cluster metrics");
  }
  return {std::string{leaderServerId}};
}

void ClusterInfo::proposeMetricsLeader(uint64_t oldRebootId,
                                       std::string_view oldServerId) {
  AgencyComm ac{_server};
  auto rebootId = ServerState::instance()->getRebootId().value();
  auto serverId = ServerState::instance()->getId();

  VPackBuilder builderOldRebootId;
  builderOldRebootId.add(VPackValue{oldRebootId});
  VPackBuilder builderOldServerId;
  builderOldServerId.add(VPackValue{oldServerId});
  VPackBuilder builderRebootId;
  builderRebootId.add(VPackValue{rebootId});
  VPackBuilder builderServerId;
  builderServerId.add(VPackValue{serverId});

  AgencyWriteTransaction const write{
      {{kMetricsRebootId, AgencyValueOperationType::SET,
        builderRebootId.slice()},
       {kMetricsServerId, AgencyValueOperationType::SET,
        builderServerId.slice()}},
      {{kMetricsRebootId, AgencyPrecondition::Type::VALUE,
        builderOldRebootId.slice()},
       {kMetricsServerId, AgencyPrecondition::Type::VALUE,
        builderOldServerId.slice()}}};
  auto const r = ac.sendTransactionWithFailover(write);
  if (r.successful()) {
    return;
  }
  if (r.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
    LOG_TOPIC("bfdc5", TRACE, Logger::CLUSTER)
        << "Failed to self-propose leader";
  } else {
    // We don't need retry here, because we have retry in ClusterMetricsFeature
    LOG_TOPIC("bfdc6", WARN, Logger::CLUSTER)
        << "Failed to self-propose leader with httpCode: " << r.httpCode();
  }
}

AnalyzerModificationTransaction::Ptr
ClusterInfo::createAnalyzersCleanupTrans() {
  if (AnalyzerModificationTransaction::getPendingCount() ==
      0) {  // rough check, don`t care about sync much
    READ_LOCKER(readLocker, _planProt.lock);
    for (auto& it : _dbAnalyzersRevision) {
      if (it.second->getRebootID() == ServerState::instance()->getRebootId() &&
          it.second->getServerID() == ServerState::instance()->getId() &&
          it.second->getRevision() != it.second->getBuildingRevision()) {
        // this maybe dangling
        if (AnalyzerModificationTransaction::getPendingCount() ==
            0) {  // still nobody active
          return std::make_unique<AnalyzerModificationTransaction>(it.first,
                                                                   this, true);
        } else {
          break;
        }
      }
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixServersRegistered = "Current/ServersRegistered";
static std::string const prefixServersKnown = "Current/ServersKnown";
static std::string const mapUniqueToShortId = "Target/MapUniqueToShortID";
static std::string const prefixHealth = "Supervision/Health";

void ClusterInfo::loadServers() {
  ++_serversProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                 // reread from the agency!
  std::lock_guard mutexLocker{_serversProt.mutex};
  uint64_t storedVersion = _serversProt.wantedVersion;  // this is the version
  // we will set in the end
  if (_serversProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  auto [acb, index] = _agencyCache.read(
      std::vector<std::string>({AgencyCommHelper::path(prefixServersRegistered),
                                AgencyCommHelper::path(mapUniqueToShortId),
                                AgencyCommHelper::path(prefixServersKnown),
                                AgencyCommHelper::path(prefixHealth)}));
  auto result = acb->slice();
  if (!result.isArray()) {
    LOG_TOPIC("be98b", DEBUG, Logger::CLUSTER)
        << "Failed to load server lists from the agency cache given "
        << acb->toJson();
    return;
  }

  VPackSlice serversRegistered, serversAliases, serversKnownSlice,
      supervisionHealth;

  // FIXME(Dronplane): use std::arrays
  std::initializer_list<std::string_view> const serversRegisteredPath{
      AgencyCommHelper::path(), "Current", "ServersRegistered"};
  if (result[0].hasKey(serversRegisteredPath)) {
    serversRegistered = result[0].get(serversRegisteredPath);
  }
  std::initializer_list<std::string_view> const mapUniqueToShortIDPath{
      AgencyCommHelper::path(), "Target", "MapUniqueToShortID"};
  if (result[0].hasKey(mapUniqueToShortIDPath)) {
    serversAliases = result[0].get(mapUniqueToShortIDPath);
  }
  std::initializer_list<std::string_view> const serversKnownPath{
      AgencyCommHelper::path(), "Current", "ServersKnown"};
  if (result[0].hasKey(serversKnownPath)) {
    serversKnownSlice = result[0].get(serversKnownPath);
  }
  std::initializer_list<std::string_view> const supervisionHealthPath{
      AgencyCommHelper::path(), "Supervision", "Health"};
  if (result[0].hasKey(supervisionHealthPath)) {
    supervisionHealth = result[0].get(supervisionHealthPath);
  }

  if (serversRegistered.isObject()) {
    decltype(_servers) newServers{_resourceMonitor};
    decltype(_serverAliases) newAliases{_resourceMonitor};
    decltype(_serverAdvertisedEndpoints) newAdvertisedEndpoints{
        _resourceMonitor};
    decltype(_serverTimestamps) newTimestamps{_resourceMonitor};

    containers::FlatHashSet<ServerID> serverIds;

    for (auto const& res : VPackObjectIterator(serversRegistered)) {
      velocypack::Slice slice = res.value;

      if (slice.isObject() && slice.hasKey("endpoint")) {
        std::string server =
            basics::VelocyPackHelper::getStringValue(slice, "endpoint", "");
        std::string advertised = basics::VelocyPackHelper::getStringValue(
            slice, "advertisedEndpoint", "");

        std::string serverId = res.key.copyString();
        try {
          velocypack::Slice serverSlice;
          serverSlice = serversAliases.get(serverId);
          if (serverSlice.isObject()) {
            std::string alias = basics::VelocyPackHelper::getStringValue(
                serverSlice, "ShortName", "");
            newAliases.try_emplace(std::move(alias), serverId);
          }
        } catch (...) {
        }
        std::string serverTimestamp =
            basics::VelocyPackHelper::getStringValue(slice, "timestamp", "");
        newServers.try_emplace(serverId, server);
        newAdvertisedEndpoints.try_emplace(serverId, advertised);
        serverIds.emplace(serverId);
        newTimestamps.try_emplace(serverId, serverTimestamp);
      }
    }

    auto newServersKnown =
        parseServersKnown(serversKnownSlice, supervisionHealth, serverIds);

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _serversProt.lock);
      _servers.swap(newServers);
      _serverAliases.swap(newAliases);
      _serverAdvertisedEndpoints.swap(newAdvertisedEndpoints);
      _serversKnown = std::move(newServersKnown);
      _serverTimestamps.swap(newTimestamps);
      _serversProt.doneVersion = storedVersion;
      _serversProt.isValid = true;
    }
    // FIXME: Here _serversKnown was read without readlock. It looks safe
    // for now as the only write (not include setters for tests) is in this
    // method and it is protexted by mutex _serversProt.mutex

    // Our own RebootId might have changed if we have been FAILED at least once
    // since our last actual reboot, let's update it:
    auto* serverState = ServerState::instance();
    if (auto it = _serversKnown.find(serverState->getId());
        it != _serversKnown.end()) {
      // should always be ok
      if (serverState->getRebootId() != it->second.rebootId) {
        serverState->setRebootId(it->second.rebootId);
        LOG_TOPIC("feaab", INFO, Logger::CLUSTER)
            << "Updating my own rebootId to " << it->second.rebootId.value();
      }
    } else {
      LOG_TOPIC("feaaa", WARN, Logger::CLUSTER)
          << "Cannot find my own rebootId in the list of known servers, this "
             "is very strange and should not happen, if this persists, please "
             "report this error!";
    }
    // RebootTracker has its own mutex, and doesn't strictly need to be in
    // sync with the other members.
    rebootTracker().updateServerState(_serversKnown);
    return;
  }

  LOG_TOPIC("449e0", DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixServersRegistered << ", result was "
      << result.toJson();
}

////////////////////////////////////////////////////////////////////////
/// @brief Hand out copy of reboot ids
////////////////////////////////////////////////////////////////////////////////

ServersKnown ClusterInfo::rebootIds() const {
  READ_LOCKER(readLocker, _serversProt.lock);
  return _serversKnown;
}

////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint(std::string_view serverID) {
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  std::string serverID_{serverID};

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);

      // _serversAliases is a map-type <Alias, ServerID>
      if (auto ita = _serverAliases.find(serverID_);
          ita != _serverAliases.end()) {
        serverID_ = ita->second;
      }

      // _servers is a map-type <ServerId, pmr::ManagedString>
      if (auto it = _servers.find(serverID_); it != _servers.end()) {
        return std::string{it->second};
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the advertised endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerAdvertisedEndpoint(
    std::string_view serverID) {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  if (serverID == "debug-follower") {
    return "tcp://127.0.0.1:3000";
  }
#endif
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  std::string serverID_{serverID};

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);

      // _serversAliases is a map-type <Alias, ServerID>
      if (auto ita = _serverAliases.find(serverID_);
          ita != _serverAliases.end()) {
        serverID_ = ita->second;
      }

      // _serversAliases is a map-type <ServerID, std::string>
      if (auto it = _serverAdvertisedEndpoints.find(serverID_);
          it != _serverAdvertisedEndpoints.end()) {
        return std::string{it->second};
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the ID of a server from its endpoint.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerName(std::string_view endpoint) {
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);
      for (auto const& it : _servers) {
        if (it.second == endpoint) {
          return std::string{it.first};
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all coordinators from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentCoordinators = "Current/Coordinators";

void ClusterInfo::loadCurrentCoordinators() {
  ++_coordinatorsProt.wantedVersion;  // Indicate that after *NOW* somebody
                                      // has to reread from the agency!
  std::lock_guard mutexLocker{_coordinatorsProt.mutex};
  uint64_t storedVersion = _coordinatorsProt.wantedVersion;  // this is the
  // version we will set in the end
  if (_coordinatorsProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  auto [acb, index] = _agencyCache.read(std::vector<std::string>{
      AgencyCommHelper::path(prefixCurrentCoordinators)});
  auto result = acb->slice();

  if (result.isArray()) {
    velocypack::Slice currentCoordinators =
        result[0].get(std::initializer_list<std::string_view>{
            AgencyCommHelper::path(), "Current", "Coordinators"});

    if (currentCoordinators.isObject()) {
      decltype(_coordinators) newCoordinators{_resourceMonitor};

      for (auto const& coordinator : VPackObjectIterator(currentCoordinators)) {
        newCoordinators.try_emplace(coordinator.key.copyString(),
                                    coordinator.value.copyString());
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
        _coordinators.swap(newCoordinators);
        _coordinatorsProt.doneVersion = storedVersion;
        _coordinatorsProt.isValid = true;
      }
      return;
    }
  }

  LOG_TOPIC("5ee6d", DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixCurrentCoordinators << " result was "
      << result.toJson();
}

static std::string const prefixMappings = "Target/MapUniqueToShortID";

void ClusterInfo::loadCurrentMappings() {
  ++_mappingsProt.wantedVersion;  // Indicate that after *NOW* somebody
                                  // has to reread from the agency!
  std::lock_guard mutexLocker{_mappingsProt.mutex};
  uint64_t storedVersion = _mappingsProt.wantedVersion;  // this is the
                                                         // version we will
                                                         // set in the end
  if (_mappingsProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  auto [acb, index] = _agencyCache.read(
      std::vector<std::string>{AgencyCommHelper::path(prefixMappings)});
  auto result = acb->slice();

  if (result.isArray()) {
    velocypack::Slice mappings =
        result[0].get(std::initializer_list<std::string_view>{
            AgencyCommHelper::path(), "Target", "MapUniqueToShortID"});

    if (mappings.isObject()) {
      decltype(_coordinatorIdMap) newCoordinatorIdMap{_resourceMonitor};

      for (auto const& mapping : VPackObjectIterator(mappings)) {
        auto mapObject = mapping.value;
        if (mapObject.isObject()) {
          ServerID fullId = mapping.key.copyString();
          ServerShortName shortName = mapObject.get("ShortName").copyString();

          ServerShortID shortId =
              mapObject.get("TransactionID").getNumericValue<ServerShortID>();
          constexpr std::string_view kExpectedPrefix{"Coordinator"};
          if (shortName.size() > kExpectedPrefix.size() &&
              shortName.starts_with(kExpectedPrefix)) {
            newCoordinatorIdMap.try_emplace(shortId, std::move(fullId));
          }
        }
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _mappingsProt.lock);
        _coordinatorIdMap.swap(newCoordinatorIdMap);
        _mappingsProt.doneVersion = storedVersion;
        _mappingsProt.isValid = true;
      }
      return;
    }
  }

  LOG_TOPIC("36f2e", DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixMappings << " result was "
      << result.toJson();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentDBServers = "Current/DBServers";
static std::string const prefixTarget = "Target";

void ClusterInfo::loadCurrentDBServers() {
  ++_dbServersProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                   // reread from the agency!
  std::lock_guard mutexLocker{_dbServersProt.mutex};
  uint64_t storedVersion = _dbServersProt.wantedVersion;  // this is the version
  // we will set in the end
  if (_dbServersProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  auto [acb, index] = _agencyCache.read(
      std::vector<std::string>{AgencyCommHelper::path(prefixCurrentDBServers),
                               AgencyCommHelper::path(prefixTarget)});
  auto result = acb->slice();
  if (!result.isArray()) {
    return;
  }

  // Now contact the agency:
  VPackSlice currentDBServers, failedDBServers, cleanedDBServers,
      toBeCleanedDBServers;

  auto curDBServersPath = std::initializer_list<std::string_view>{
      AgencyCommHelper::path(), "Current", "DBServers"};
  if (result[0].hasKey(curDBServersPath)) {
    currentDBServers = result[0].get(curDBServersPath);
  }
  auto failedServerPath = std::initializer_list<std::string_view>{
      AgencyCommHelper::path(), "Target", "FailedServers"};
  if (result[0].hasKey(failedServerPath)) {
    failedDBServers = result[0].get(failedServerPath);
  }
  auto cleanedServersPath = std::initializer_list<std::string_view>{
      AgencyCommHelper::path(), "Target", "CleanedServers"};
  if (result[0].hasKey(cleanedServersPath)) {
    cleanedDBServers = result[0].get(cleanedServersPath);
  }

  auto toBeCleanedServersPath = std::initializer_list<std::string_view>{
      AgencyCommHelper::path(), "Target", "ToBeCleanedServers"};
  if (result[0].hasKey(toBeCleanedServersPath)) {
    toBeCleanedDBServers = result[0].get(toBeCleanedServersPath);
  }

  if (currentDBServers.isObject() && failedDBServers.isObject()) {
    decltype(_dbServers) newDBServers{_resourceMonitor};

    for (auto const& dbserver : VPackObjectIterator(currentDBServers)) {
      bool found = false;
      if (failedDBServers.isObject()) {
        for (auto const& failedServer : VPackObjectIterator(failedDBServers)) {
          if (basics::VelocyPackHelper::equal(dbserver.key, failedServer.key,
                                              false)) {
            found = true;
            break;
          }
        }
      }
      if (found) {
        continue;
      }

      if (cleanedDBServers.isArray()) {
        found = false;
        for (auto const& cleanedServer : VPackArrayIterator(cleanedDBServers)) {
          if (basics::VelocyPackHelper::equal(dbserver.key, cleanedServer,
                                              false)) {
            found = true;
            break;
          }
        }
        if (found) {
          continue;
        }
      }

      if (toBeCleanedDBServers.isArray()) {
        found = false;
        for (auto const& toBeCleanedServer :
             VPackArrayIterator(toBeCleanedDBServers)) {
          if (basics::VelocyPackHelper::equal(dbserver.key, toBeCleanedServer,
                                              false)) {
            found = true;
            break;
          }
        }
        if (found) {
          continue;
        }
      }

      newDBServers.try_emplace(dbserver.key.copyString(),
                               dbserver.value.copyString());
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _dbServersProt.lock);
      _dbServers.swap(newDBServers);
      _dbServersProt.doneVersion = storedVersion;
      _dbServersProt.isValid = true;
    }
    return;
  }

  LOG_TOPIC("5a7e1", DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixCurrentDBServers << " result was "
      << result.toJson();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentDBServers() {
  std::vector<ServerID> result;

  if (!_dbServersProt.isValid) {
    loadCurrentDBServers();
  }
  // return a consistent state of servers
  READ_LOCKER(readLocker, _dbServersProt.lock);

  result.reserve(_dbServers.size());

  for (auto& it : _dbServers) {
    result.emplace_back(it.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the servers who are responsible for a shard (one leader
/// and multiple followers)
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ClusterInfo::ManagedVector<ClusterInfo::pmr::ServerID> const>
ClusterInfo::getResponsibleServer(ShardID shardID) {
  int tries = 0;

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _currentProt.lock);
      // _shardsToCurrentServers is a map-type <ShardId,
      // std::shared_ptr<std::vector<ServerId>>>
      if (auto it = _shardsToCurrentServers.find(shardID);
          it != _shardsToCurrentServers.end()) {
        // TODO throw an exception if we don't find the shard or the server list
        // is null or empty?
        auto serverList = it->second;
        if (serverList != nullptr && !serverList->empty() &&
            (*serverList)[0].starts_with('_')) {
          // This is a temporary situation in which the leader has already
          // resigned, let's wait half a second and try again.
          --tries;
        } else {
          return it->second;
        }
      }
    }

    if (++tries >= 2 || _server.isStopping()) {
      break;
    }

    LOG_TOPIC("b1dc5", INFO, Logger::CLUSTER)
        << "getResponsibleServerReplication1: found resigned leader for shard "
        << shardID << ", waiting for half a second...";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  return std::make_shared<ManagedVector<pmr::ServerID>>(_resourceMonitor);
}

futures::Future<ResultT<ServerID>> ClusterInfo::getLeaderForShard(
    ShardID shardID) {
  ServerID resultBuffer;
  auto result = co_await getLeadersForShards({&shardID, 1}, {&resultBuffer, 1});
  if (result.fail()) {
    co_return result;
  }
  co_return resultBuffer;
}

futures::Future<Result> ClusterInfo::getLeadersForShards(
    std::span<ShardID const> shards, std::span<ServerID> result) {
  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  auto const resolveShards = [&]() -> std::variant<Result, ShardID> {
    for (auto [i, shardId] : enumerate(shards)) {
      // _shardsToCurrentServers is a map-type <ShardId,
      // std::shared_ptr<std::vector<ServerId>>>
      auto it = _shardsToCurrentServers.find(shardId);
      if (it == _shardsToCurrentServers.end()) {
        return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                      fmt::format("Could not find servers of shard {} in "
                                  "Current version {} (raft index {})",
                                  shardId, _currentVersion, _currentIndex)};
      }
      auto& servers = it->second;
      TRI_ASSERT(servers != nullptr);
      TRI_ASSERT(!servers->empty());

      if (servers->front().starts_with('_')) {
        return shardId;
      }
      result[i] = ServerID{servers->front()};
    }

    return Result{TRI_ERROR_NO_ERROR};
  };

  while (true) {
    if (_server.isStopping()) {
      co_return {TRI_ERROR_SHUTTING_DOWN};
    }

    auto resolveResult = [&]() {
      READ_LOCKER(readLocker, _currentProt.lock);
      return resolveShards();
    }();

    if (std::holds_alternative<Result>(resolveResult)) {
      co_return std::get<Result>(resolveResult);
    } else {
      auto shardId = std::get<ShardID>(resolveResult);

      auto readLocker = basics::ReadLocker(&_planProt.lock,
                                           basics::LockerType::BLOCKING, true);
      auto const database = getDatabaseNameForShard(readLocker, shardId);
      auto const collection = getCollectionNameForShard(readLocker, shardId);
      auto const planVersion = _planVersion;
      auto const planIndex = _planIndex;
      readLocker.unlock();
      if (!database.has_value()) {
        co_return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                   fmt::format("Could not find database for shard {} in Plan "
                               "version {} (raft index {})",
                               shardId, planVersion, planIndex)};
      }
      if (collection.empty()) {
        co_return {
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            fmt::format("Could not find collection for shard {} in Plan "
                        "version {} (raft index {}) (database is {})",
                        shardId, planVersion, planIndex, database.value())};
      }
      LOG_TOPIC("289f7", INFO, Logger::CLUSTER)
          << "getLeaderForShard: found resigned leader for shard " << shardId
          << " (part of " << *database << "/" << collection << ")"
          << ", waiting for the new leader to take over.";
      auto path = paths::aliases::current()
                      ->collections()
                      ->database(*database)
                      ->collection(collection)
                      ->shard(shardId)
                      ->servers();
      // wait for the leader takeover to appear in our agency cache
      auto const [waitResult, raftIndex] =
          co_await _agencyCallbackRegistry.waitFor(
              *path,
              [&](velocypack::Slice servers, consensus::index_t index)
                  -> std::optional<std::tuple<Result, consensus::index_t>> {
                if (servers.isNone()) {
                  return std::make_tuple(
                      Result{
                          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                          fmt::format(
                              "Database or collection ({}/{}) gone in Current "
                              "while waiting for leader of shard {} (raft "
                              "index {})",
                              *database, collection, shardId, index)},
                      index);
                }

                bool const leaderResigned = [&]() {
                  if (servers.isArray() && servers.length() > 0) {
                    auto const leader = servers[0];
                    return leader.isString() &&
                           leader.stringView().starts_with('_');
                  } else {
                    return false;
                  }
                }();
                if (leaderResigned) {
                  return std::nullopt;
                }

                return std::make_tuple(Result{}, index);
              });
      if (!waitResult.ok()) {
        co_return waitResult;
      }

      // wait for cluster info to catch up with the agency cache
      auto res = co_await waitForCurrent(raftIndex);
      if (!res.ok()) {
        co_return res;
      }
    }
  }

  ADB_UNREACHABLE;
}

ClusterInfo::ShardLeadership ClusterInfo::getShardLeadership(
    ServerID const& server, ShardID const& shard) const {
  if (!_currentProt.isValid) {
    return ShardLeadership::kUnclear;
  }
  READ_LOCKER(readLocker, _currentProt.lock);
  auto it = _shardsToCurrentServers.find(shard);
  if (it == _shardsToCurrentServers.end()) {
    return ShardLeadership::kUnclear;
  }
  auto const& serverList = it->second;
  if (!serverList || serverList->empty()) {
    return ShardLeadership::kUnclear;
  }
  auto const& front = serverList->front();
  TRI_ASSERT(!front.empty());
  if (front.starts_with('_')) {
    // This is a temporary situation in which the leader has already resigned,
    // so we don't know exactly right now.
    return ShardLeadership::kUnclear;
  }
  return front == std::string_view{server} ? ShardLeadership::kLeader
                                           : ShardLeadership::kFollower;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief atomically find all servers who are responsible for the given
/// shards (only the leaders).
/// will throw an exception if no leader can be found for any
/// of the shards. will return an empty result if the shards couldn't be
/// determined after a while - it is the responsibility of the caller to
/// check for an empty result!
//////////////////////////////////////////////////////////////////////////////

containers::FlatHashMap<ShardID, ServerID> ClusterInfo::getResponsibleServers(
    containers::FlatHashSet<ShardID> const& shardIds) {
  TRI_ASSERT(!shardIds.empty());

  containers::FlatHashMap<ShardID, ServerID> result;

  int tries = 0;

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).waitAndGet();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  while (true) {
    TRI_ASSERT(result.empty());
    {
      READ_LOCKER(readLocker, _currentProt.lock);
      for (auto const& shardId : shardIds) {
        auto it = _shardsToCurrentServers.find(shardId);

        if (it == _shardsToCurrentServers.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                         "no shard found with ID " + shardId);
        }

        auto serverList = it->second;
        if (serverList == nullptr || serverList->empty()) {
          break;  // replication 2 not yet ready
        }

        if ((*serverList)[0].starts_with('_')) {
          // This is a temporary situation in which the leader has already
          // resigned, let's wait half a second and try again.
          --tries;
          break;
        }

        // put leader into result
        result.try_emplace(shardId, it->second->front());
      }
    }

    if (result.size() == shardIds.size()) {
      // result is complete
      break;
    }

    // reset everything we found so far for the next round
    result.clear();

    if (++tries >= 2 || _server.isStopping()) {
      break;
    }

    LOG_TOPIC("31428", INFO, Logger::CLUSTER)
        << "getResponsibleServersReplication1: found resigned leader,"
        << "waiting for half a second...";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard list of a collection, sorted numerically
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ShardID> const> arangodb::ClusterInfo::getShardList(
    std::string_view collectionID) {
  TRI_IF_FAILURE("ClusterInfo::failedToGetShardList") {
    // Simulate no results
    return std::make_shared<std::vector<ShardID> const>();
  }

  {
    // Get the sharding keys and the number of shards:
    READ_LOCKER(readLocker, _planProt.lock);
    // _shards is a map-type <DataSourceId, shared_ptr<vector<string>>>
    auto it = _shards.find(collectionID);

    if (it != _shards.end()) {
      return it->second;
    }
  }
  return std::make_shared<std::vector<ShardID> const>();
}

std::shared_ptr<ClusterInfo::ManagedVector<ClusterInfo::pmr::ServerID> const>
ClusterInfo::getCurrentServersForShard(ShardID shardId) {
  READ_LOCKER(readLocker, _currentProt.lock);

  if (auto it = _shardsToCurrentServers.find(shardId);
      it != _shardsToCurrentServers.end()) {
    return it->second;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of coordinator server names
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentCoordinators() {
  std::vector<ServerID> result;

  if (!_coordinatorsProt.isValid) {
    loadCurrentCoordinators();
  }

  // return a consistent state of servers
  READ_LOCKER(readLocker, _coordinatorsProt.lock);

  result.reserve(_coordinators.size());

  for (auto& it : _coordinators) {
    result.emplace_back(it.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup full coordinator ID from short ID
////////////////////////////////////////////////////////////////////////////////

ServerID ClusterInfo::getCoordinatorByShortID(ServerShortID shortId) {
  int tries = 0;
  if (!_mappingsProt.isValid) {
    loadCurrentMappings();
    tries++;
  }

  while (true) {
    {
      // return a consistent state of servers
      READ_LOCKER(readLocker, _mappingsProt.lock);

      if (auto it = _coordinatorIdMap.find(shortId);
          it != _coordinatorIdMap.end()) {
        return ServerID{it->second};
      }
    }

    if (++tries >= 2) {
      break;
    }

    loadCurrentMappings();
  }

  return ServerID{};
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate current coordinators
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidateCurrentCoordinators() {
  WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
  _coordinatorsProt.isValid = false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get current "Plan" structure
//////////////////////////////////////////////////////////////////////////////

containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder const>>
ClusterInfo::getPlan(uint64_t& index,
                     containers::FlatHashSet<std::string> const& dirty) {
  // We should never proceed here, until we have seen an
  // initial agency cache through loadPlan
  Result r = waitForPlan(1).waitAndGet();
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }

  containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder const>> ret;
  READ_LOCKER(readLocker, _planProt.lock);
  index = _planIndex;
  for (auto const& i : dirty) {
    if (auto it = _plan.find(i); it != _plan.end()) {
      ret.try_emplace(it->first, it->second);
    }
  }
  return ret;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get current "Current" structure
//////////////////////////////////////////////////////////////////////////////

containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder const>>
ClusterInfo::getCurrent(uint64_t& index,
                        containers::FlatHashSet<std::string> const& dirty) {
  // We should never proceed here, until we have seen an
  // initial agency cache through loadCurrent
  Result r = waitForCurrent(1).waitAndGet();
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }

  containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder const>> ret;
  READ_LOCKER(readLocker, _currentProt.lock);
  index = _currentIndex;
  for (auto const& i : dirty) {
    if (auto it = _current.find(i); it != _current.end()) {
      ret.try_emplace(it->first, it->second);
    }
  }
  return ret;
}

containers::FlatHashSet<ServerID> ClusterInfo::getFailedServers() const {
  std::lock_guard lock{_failedServersMutex};
  containers::FlatHashSet<ServerID> result;
  result.reserve(_failedServers.size());
  for (auto const& server : _failedServers) {
    result.emplace(server);
  }
  return result;
}

void ClusterInfo::setFailedServers(
    containers::FlatHashSet<ServerID> failedServers) {
  std::lock_guard lock{_failedServersMutex};
  _failedServers.clear();
  _failedServers.reserve(failedServers.size());
  for (auto const& server : failedServers) {
    _failedServers.emplace(server);
  }
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void ClusterInfo::setServers(
    containers::FlatHashMap<ServerID, std::string> servers) {
  WRITE_LOCKER(writeLocker, _serversProt.lock);
  _servers.clear();
  for (auto const& [k, v] : servers) {
    _servers.emplace(k, v);
  }
}

void ClusterInfo::setServerAliases(
    containers::FlatHashMap<ServerID, std::string> aliases) {
  WRITE_LOCKER(writeLocker, _serversProt.lock);
  _serverAliases.clear();
  for (auto const& [k, v] : aliases) {
    _serverAliases.emplace(k, v);
  }
}

void ClusterInfo::setServerAdvertisedEndpoints(
    containers::FlatHashMap<ServerID, std::string> advertisedEndpoints) {
  WRITE_LOCKER(writeLocker, _serversProt.lock);
  _serverAdvertisedEndpoints.clear();
  for (auto const& [k, v] : advertisedEndpoints) {
    _serverAdvertisedEndpoints.emplace(k, v);
  }
}

void ClusterInfo::setShardToShardGroupLeader(
    containers::FlatHashMap<ShardID, ShardID> shardToShardGroupLeader) {
  WRITE_LOCKER(writeLocker, _planProt.lock);
  _shardToShardGroupLeader.clear();
  for (auto const& [k, v] : shardToShardGroupLeader) {
    _shardToShardGroupLeader.emplace(k, v);
  }
}

void ClusterInfo::setShardGroups(
    containers::FlatHashMap<ShardID, std::shared_ptr<std::vector<ShardID>>>
        shardGroups) {
  WRITE_LOCKER(writeLocker, _planProt.lock);
  _shardGroups.clear();
  for (auto const& [k, v] : shardGroups) {
    auto vv = allocateShared<ManagedVector<ShardID>>();
    vv->assign(v->begin(), v->end());
    _shardGroups.emplace(k, std::move(vv));
  }
}

void ClusterInfo::setShardIds(
    containers::FlatHashMap<ShardID,
                            std::shared_ptr<std::vector<ServerID> const>>
        shardIds) {
  WRITE_LOCKER(writeLocker, _currentProt.lock);
  _shardsToCurrentServers.clear();
  for (auto const& [k, v] : shardIds) {
    auto vv = allocateShared<ManagedVector<pmr::ServerID>>();
    vv->assign(v->begin(), v->end());
    _shardsToCurrentServers.emplace(k, std::move(vv));
  }
}
#endif

bool ClusterInfo::serverExists(std::string_view serverId) const noexcept {
  READ_LOCKER(readLocker, _serversProt.lock);
  return _servers.contains(serverId);
}

bool ClusterInfo::serverAliasExists(std::string_view alias) const noexcept {
  READ_LOCKER(readLocker, _serversProt.lock);
  return _serverAliases.contains(alias);
}

containers::FlatHashMap<ServerID, std::string> ClusterInfo::getServers() {
  if (!_serversProt.isValid) {
    loadServers();
  }
  READ_LOCKER(readLocker, _serversProt.lock);
  containers::FlatHashMap<ServerID, std::string> result;
  for (auto const& [k, v] : _servers) {
    result.emplace(k, v);
  }
  return result;
}

containers::FlatHashMap<ServerID, std::string> ClusterInfo::getServerAliases() {
  containers::FlatHashMap<std::string, std::string> ret;
  READ_LOCKER(readLocker, _serversProt.lock);
  // note: don't try to change this to
  //  return _serverAlias
  // because we are returning the aliases in {value, key} order here
  for (const auto& i : _serverAliases) {
    ret.try_emplace(i.second, i.first);
  }
  return ret;
}

Result ClusterInfo::getShardServers(ShardID shardId,
                                    std::vector<ServerID>& servers) {
  READ_LOCKER(readLocker, _planProt.lock);

  if (auto it = _shardsToPlanServers.find(shardId);
      it != _shardsToPlanServers.end()) {
    servers.assign(it->second->begin(), it->second->end());
    return {};
  }

  LOG_TOPIC("16d14", DEBUG, Logger::CLUSTER)
      << "Strange, did not find shard in _shardServers: " << shardId;
  return Result{TRI_ERROR_FAILED};
}

CollectionID ClusterInfo::getCollectionNameForShard(ShardID shardId) {
  auto readLocker =
      basics::ReadLocker(&_planProt.lock, basics::LockerType::BLOCKING, true);
  return getCollectionNameForShard(readLocker, shardId);
}

CollectionID ClusterInfo::getCollectionNameForShard(
    basics::ReadLocker<std::decay_t<decltype(_planProt.lock)>>& readLocker,
    ShardID shardId) {
  TRI_ASSERT(readLocker.getLock() == &_planProt.lock);
  TRI_ASSERT(readLocker.isLocked());
  if (auto it = _shardToName.find(shardId); it != _shardToName.end()) {
    return CollectionID{it->second};
  }
  return StaticStrings::Empty;
}

auto ClusterInfo::getDatabaseNameForShard(ShardID shardId)
    -> std::optional<DatabaseID> {
  auto readLocker =
      basics::ReadLocker(&_planProt.lock, basics::LockerType::BLOCKING, true);
  return getDatabaseNameForShard(readLocker, shardId);
}

auto ClusterInfo::getDatabaseNameForShard(
    [[maybe_unused]] basics::ReadLocker<std::decay_t<decltype(_planProt.lock)>>&
        readLocker,
    ShardID shardId) -> std::optional<DatabaseID> {
  TRI_ASSERT(readLocker.getLock() == &_planProt.lock);
  TRI_ASSERT(readLocker.isLocked());
  if (auto it = _shardToDb.find(shardId); it != _shardToDb.end()) {
    return DatabaseID{it->second};
  } else {
    return std::nullopt;
  }
}

auto ClusterInfo::getReplicatedLogsParticipants(std::string_view database) const
    -> ResultT<
        std::unordered_map<replication2::LogId, std::vector<std::string>>> {
  READ_LOCKER(readLocker, _planProt.lock);

  auto it = _newStuffByDatabase.find(database);
  if (it == std::end(_newStuffByDatabase)) {
    return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND};
  }

  auto replicatedLogs =
      std::unordered_map<replication2::LogId, std::vector<std::string>>{};
  for (auto const& [logId, logPlanSpecification] : it->second->replicatedLogs) {
    std::vector<std::string> participants;
    auto const& planParticipants =
        logPlanSpecification->participantsConfig.participants;
    participants.reserve(planParticipants.size());
    for (auto const& pInfo : planParticipants) {
      participants.push_back(pInfo.first);
    }

    // Move the leader at the top of the list.
    if (logPlanSpecification->currentTerm.has_value() &&
        logPlanSpecification->currentTerm->leader.has_value()) {
      for (auto& p : participants) {
        if (p == logPlanSpecification->currentTerm->leader->serverId) {
          std::swap(p, participants[0]);
          break;
        }
      }
    }

    replicatedLogs.emplace(logId, std::move(participants));
  }

  return replicatedLogs;
}

auto ClusterInfo::getCollectionGroupById(
    replication2::agency::CollectionGroupId id)
    -> std::shared_ptr<
        replication2::agency::CollectionGroupPlanSpecification const> {
  READ_LOCKER(readLocker, _planProt.lock);
  if (auto iter = _collectionGroups.find(id); iter != _collectionGroups.end()) {
    return iter->second;
  }
  return nullptr;
}

auto ClusterInfo::getReplicatedLogLeader(replication2::LogId id) const
    -> ResultT<ServerID> {
  READ_LOCKER(readLocker, _planProt.lock);

  auto it = _replicatedLogs.find(id);
  if (it == std::end(_replicatedLogs)) {
    return Result::fmt(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND, id);
  }

  if (auto const& term = it->second->currentTerm) {
    if (auto const& leader = term->leader) {
      return leader->serverId;
    }
  }

  return {TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
}

Result ClusterInfo::agencyDump(std::shared_ptr<VPackBuilder> const& body) {
  AgencyCommResult dump = _agency.dump();

  if (!dump.successful()) {
    LOG_TOPIC("93c0e", ERR, Logger::CLUSTER)
        << "failed to acquire agency dump: " << dump.errorMessage();
    return Result(dump.errorCode(), dump.errorMessage());
  }

  body->add(dump.slice());
  return Result();
}

Result ClusterInfo::agencyPlan(std::shared_ptr<VPackBuilder> const& body) {
  auto [acb, index] = _agencyCache.read(
      {AgencyCommHelper::path("Plan"), AgencyCommHelper::path("Target"),
       AgencyCommHelper::path("Sync/LatestID")});
  VPackSlice result = acb->slice();

  if (result.isArray()) {
    body->add(acb->slice());
  } else {
    LOG_TOPIC("36ada", DEBUG, Logger::CLUSTER)
        << "Failed to acquire the Plan section from the agency cache: "
        << acb->toJson();
    VPackObjectBuilder g(body.get());
  }
  return Result();
}

Result ClusterInfo::agencyReplan(VPackSlice const plan) {
  TRI_IF_FAILURE("ClusterInfo::failReplanAgency") { return TRI_ERROR_DEBUG; }
  // Apply only Collections and DBServers
  auto trxOperations = std::vector<AgencyOperation>{
      {"Current/Collections", AgencyValueOperationType::SET,
       VPackSlice::emptyObjectSlice()},
      SetOldEntry("Plan/Collections", {"arango", "Plan", "Collections"}, plan),
      {"Current/Databases", AgencyValueOperationType::SET,
       VPackSlice::emptyObjectSlice()},
      SetOldEntry("Plan/Databases", {"arango", "Plan", "Databases"}, plan),
      {"Current/Views", AgencyValueOperationType::SET,
       VPackSlice::emptyObjectSlice()},
      {"Current/CollectionGroups", AgencyValueOperationType::SET,
       VPackSlice::emptyObjectSlice()},
      {"Current/ReplicatedLogs", AgencyValueOperationType::SET,
       VPackSlice::emptyObjectSlice()},
      SetOldEntry("Plan/Analyzers", {"arango", "Plan", "Analyzers"}, plan),
      SetOldEntry("Plan/Views", {"arango", "Plan", "Views"}, plan),
      {"Current/Version", AgencySimpleOperationType::INCREMENT_OP},
      {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP},
      {"Sync/UserVersion", AgencySimpleOperationType::INCREMENT_OP},
      {"Sync/FoxxQueueVersion", AgencySimpleOperationType::INCREMENT_OP},
      {"Sync/HotBackupRestoreDone", AgencySimpleOperationType::INCREMENT_OP}};

  // For replication 2 we need to include some parts of target.
  // As those are not existing in Replication 1, we only append them, if they
  // are part of the export structure.
  if (plan.hasKey({"arango", "Target"})) {
    // If we have entries in Target, we should apply them
    for (auto const& subEntry : {"CollectionGroups", "Collections",
                                 "CollectionNames", "ReplicatedLogs"}) {
      if (auto entry = plan.get({"arango", "Target", subEntry});
          !entry.isNone()) {
        trxOperations.emplace_back(absl::StrCat("Target/", subEntry),
                                   AgencyValueOperationType::SET, entry);
      }
    }
  }

  // For replication 2 we also have other parts in Plan
  for (auto const& subEntry : {"CollectionGroups", "ReplicatedLogs"}) {
    if (auto entry = plan.get({"arango", "Plan", subEntry}); !entry.isNone()) {
      trxOperations.emplace_back(absl::StrCat("Plan/", subEntry),
                                 AgencyValueOperationType::SET, entry);
    }
  }

  // Apply only Collections and DBServers
  AgencyWriteTransaction transaction(std::move(trxOperations));

  VPackSlice latestIdSlice = plan.get({"arango", "Sync", "LatestID"});
  if (!latestIdSlice.isNone()) {
    transaction.operations.emplace_back(
        "Sync/LatestID", AgencyValueOperationType::SET, latestIdSlice);
  }
  AgencyCommResult r = _agency.sendTransactionWithFailover(transaction);
  if (!r.successful()) {
    return Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        basics::StringUtils::concatT("Error reporting to agency: _statusCode: ",
                                     r.errorCode()));
  }

  Result rr;
  if (VPackSlice resultsSlice = r.slice().get("results");
      resultsSlice.length() > 0) {
    auto raftIndex = resultsSlice[0].getNumber<uint64_t>();
    if (raftIndex == 0) {
      // This means the above request was actually illegal
      return {TRI_ERROR_HOT_BACKUP_INTERNAL,
              "Failed to restore agency plan from Hotbackup. Please contact "
              "ArangoDB support immediately."};
    }
    rr = waitForPlan(raftIndex).waitAndGet();
  }

  return rr;
}

std::string const backupKey = "/arango/Target/HotBackup/Create";
std::string const maintenanceKey = "/arango/Supervision/Maintenance";
std::string const supervisionMode = "/arango/Supervision/State/Mode";
std::string const toDoKey = "/arango/Target/ToDo";
std::string const pendingKey = "/arango/Target/Pending";
std::string const writeURL = "_api/agency/write";

Result ClusterInfo::agencyHotBackupLock(std::string_view backupId,
                                        double timeout, bool& supervisionOff) {
  using namespace std::chrono;

  auto const endTime = steady_clock::now() +
                       milliseconds(static_cast<uint64_t>(1.0e3 * timeout));
  supervisionOff = false;

  LOG_TOPIC("e74e5", DEBUG, Logger::BACKUP)
      << "initiating agency lock for hot backup " << backupId;

  auto const timeouti = static_cast<long>(std::ceil(timeout));

  VPackBuilder builder;
  {
    VPackArrayBuilder trxs(&builder);
    {
      VPackArrayBuilder trx(&builder);

      // Operations
      {
        VPackObjectBuilder o(&builder);
        builder.add(  // Backup lock
            backupKey,
            VPackValue(timepointToString(std::chrono::system_clock::now() +
                                         std::chrono::seconds(timeouti))));
        builder.add(  // Turn off supervision
            maintenanceKey,
            VPackValue(timepointToString(std::chrono::system_clock::now() +
                                         std::chrono::seconds(timeouti))));
      }

      // Preconditions
      {
        VPackObjectBuilder precs(&builder);
        builder.add(VPackValue(backupKey));  // Backup key empty
        {
          VPackObjectBuilder oe(&builder);
          builder.add("oldEmpty", VPackValue(true));
        }
        builder.add(VPackValue(pendingKey));  // No jobs pending
        {
          VPackObjectBuilder oe(&builder);
          builder.add("old", VPackSlice::emptyObjectSlice());
        }
        builder.add(VPackValue(toDoKey));  // No jobs to do
        {
          VPackObjectBuilder oe(&builder);
          builder.add("old", VPackSlice::emptyObjectSlice());
        }
        builder.add(VPackValue(supervisionMode));  // Supervision on
        {
          VPackObjectBuilder old(&builder);
          builder.add("old", VPackValue("Normal"));
        }
      }

      builder.add(VPackValue(to_string(boost::uuids::random_generator()())));
    }

    {
      VPackArrayBuilder trx(&builder);

      // Operations
      {
        VPackObjectBuilder o(&builder);
        builder.add(  // Backup lock
            backupKey,
            VPackValue(timepointToString(std::chrono::system_clock::now() +
                                         std::chrono::seconds(timeouti))));
        builder.add(  // Turn off supervision
            maintenanceKey,
            VPackValue(timepointToString(std::chrono::system_clock::now() +
                                         std::chrono::seconds(timeouti))));
      }

      // Preconditions
      {
        VPackObjectBuilder precs(&builder);
        builder.add(VPackValue(backupKey));  // Backup key empty
        {
          VPackObjectBuilder oe(&builder);
          builder.add("oldEmpty", VPackValue(true));
        }
        builder.add(VPackValue(pendingKey));  // No jobs pending
        {
          VPackObjectBuilder oe(&builder);
          builder.add("old", VPackSlice::emptyObjectSlice());
        }
        builder.add(VPackValue(toDoKey));  // No jobs to do
        {
          VPackObjectBuilder oe(&builder);
          builder.add("old", VPackSlice::emptyObjectSlice());
        }
        builder.add(VPackValue(supervisionMode));  // Supervision off
        {
          VPackObjectBuilder old(&builder);
          builder.add("old", VPackValue("Maintenance"));
        }
      }

      builder.add(VPackValue(to_string(boost::uuids::random_generator()())));
    }
  }

  // Try to establish hot backup lock in agency.
  auto result = _agency.sendWithFailover(rest::RequestType::POST, timeout,
                                         writeURL, builder.slice());

  LOG_TOPIC("53a93", DEBUG, Logger::BACKUP)
      << "agency lock for hot backup " << backupId << " scheduled with "
      << builder.toJson();

  // *** ATTENTION ***: Result will always be 412.
  // So we're going to fail, if we have an error OTHER THAN 412:
  if (!result.successful() &&
      result.httpCode() != rest::ResponseCode::PRECONDITION_FAILED) {
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "failed to acquire backup lock in agency");
  }

  LOG_TOPIC("a94d5", DEBUG, Logger::BACKUP)
      << "agency lock response for backup id " << backupId << ": "
      << result.slice().toJson();

  if (!result.slice().isObject() || !result.slice().hasKey("results") ||
      !result.slice().get("results").isArray() ||
      result.slice().get("results").length() != 2) {
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "invalid agency result while acquiring backup lock");
  }
  auto ar = result.slice().get("results");

  uint64_t first = ar[0].getNumber<uint64_t>();
  uint64_t second = ar[1].getNumber<uint64_t>();

  if (first == 0 && second == 0) {  // tough luck
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "preconditions failed while trying to acquire "
                  "backup lock in the agency");
  }

  if (first > 0) {  // Supervision was on
    LOG_TOPIC("b6c98", DEBUG, Logger::BACKUP)
        << "agency lock found supervision on before";
    supervisionOff = false;
  } else {
    LOG_TOPIC("bbb55", DEBUG, Logger::BACKUP)
        << "agency lock found supervision off before";
    supervisionOff = true;
  }

  double wait = 0.1;
  while (!_server.isStopping() && std::chrono::steady_clock::now() < endTime) {
    auto [result, index] = _agencyCache.get("Supervision/State/Mode");

    if (result->slice().isString()) {
      if (result->slice().isEqualString("Maintenance")) {
        LOG_TOPIC("76a2c", DEBUG, Logger::BACKUP)
            << "agency hot backup lock acquired";
        return {};
      }
    }

    LOG_TOPIC("ede54", DEBUG, Logger::BACKUP)
        << "agency hot backup lock waiting: " << result->slice().toJson();

    if (wait < 2.0) {
      wait *= 1.1;
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(wait));
  }

  agencyHotBackupUnlock(backupId, timeout, supervisionOff);

  return Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL,
      "timeout waiting for maintenance mode to be activated in agency");
}

Result ClusterInfo::agencyHotBackupUnlock(std::string_view backupId,
                                          double timeout, bool supervisionOff) {
  using namespace std::chrono;

  auto const endTime = steady_clock::now() +
                       milliseconds(static_cast<uint64_t>(1.0e3 * timeout));

  LOG_TOPIC("6ae41", DEBUG, Logger::BACKUP)
      << "unlocking backup lock for backup " << backupId << "  in agency";

  VPackBuilder builder;
  {
    VPackArrayBuilder trxs(&builder);
    {
      VPackArrayBuilder trx(&builder);
      {
        VPackObjectBuilder o(&builder);
        builder.add(VPackValue(backupKey));  // Remove backup key
        {
          VPackObjectBuilder oo(&builder);
          builder.add("op", VPackValue("delete"));
        }
        if (!supervisionOff) {  // Turn supervision on, if it was on before
          builder.add(VPackValue(maintenanceKey));
          VPackObjectBuilder d(&builder);
          builder.add("op", VPackValue("delete"));
        }
      }
    }
  }

  // Try to establish hot backup lock in agency. Result will always be 412.
  // Question is: How 412?
  auto result = _agency.sendWithFailover(rest::RequestType::POST, timeout,
                                         writeURL, builder.slice());
  if (!result.successful() &&
      result.httpCode() != rest::ResponseCode::PRECONDITION_FAILED) {
    LOG_TOPIC("6ae43", WARN, Logger::BACKUP)
        << "Error when unlocking backup lock for backup " << backupId
        << " in agency, errorCode: " << result.httpCode()
        << ", errorMessage: " << result.errorMessage();
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "failed to release backup lock in agency");
  }

  if (!result.slice().isObject() || !result.slice().hasKey("results") ||
      !result.slice().get("results").isArray()) {
    LOG_TOPIC("6ae44", WARN, Logger::BACKUP)
        << "Illegal response when unlocking backup lock for backup " << backupId
        << " in agency.";
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "invalid agency result while releasing backup lock");
  }

  auto ar = result.slice().get("results");
  if (!ar[0].isNumber()) {
    LOG_TOPIC("6ae45", WARN, Logger::BACKUP)
        << "Invalid agency result when unlocking backup lock for backup "
        << backupId << " in agency: " << result.slice().toJson();
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "invalid agency result while releasing backup lock");
  }

  if (supervisionOff) {
    return {};
  }

  double wait = 0.1;
  while (!_server.isStopping() && std::chrono::steady_clock::now() < endTime) {
    auto [res, index] = _agencyCache.get("Supervision/State/Mode");

    if (!res->slice().isString()) {
      LOG_TOPIC("6ae46", WARN, Logger::BACKUP)
          << "Invalid JSON from agency when deactivating supervision mode for "
             "backup "
          << backupId;
      return Result{
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          absl::StrCat(
              "invalid JSON from agency, when deactivating supervision mode:",
              res->slice().toJson())};
    }

    if (res->slice().isEqualString("Normal")) {
      return {};
    }

    LOG_TOPIC("edf54", DEBUG, Logger::BACKUP)
        << "agency hot backup unlock waiting: " << res->slice().toJson();

    if (wait < 2.0) {
      wait *= 1.1;
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(wait));
  }

  LOG_TOPIC("6ae47", WARN, Logger::BACKUP)
      << "Timeout when deactivating supervision mode for backup " << backupId;

  return Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL,
      "timeout waiting for maintenance mode to be deactivated in agency");
}

ArangodServer& ClusterInfo::server() const { return _server; }

AgencyCallbackRegistry& ClusterInfo::agencyCallbackRegistry() const {
  return _agencyCallbackRegistry;
}

void ClusterInfo::startSyncers() {
  _planSyncer = std::make_unique<SyncerThread>(
      _server, "Plan", [this] { return loadPlan(); }, _agencyCache);
  _curSyncer = std::make_unique<SyncerThread>(
      _server, "Current",
      [this] {
        TRI_IF_FAILURE("ClusterInfo::slowCurrentSyncer") {
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(10ms);
        }
        return loadCurrent();
      },
      _agencyCache);

  if (!_planSyncer->start() || !_curSyncer->start()) {
    LOG_TOPIC("b4fa6", FATAL, Logger::CLUSTER)
        << "unable to start PlanSyncer/CurrentSyncer";
    FATAL_ERROR_EXIT();
  }
}

void ClusterInfo::drainSyncers() {
  auto clearWaitForMaps = [&](auto& mutex, auto& map) {
    std::lock_guard g(mutex);
    for (auto& pit : map) {
      pit.second.setValue(Result(_syncerShutdownCode));
    }
    map.clear();
  };

  clearWaitForMaps(_waitPlanLock, _waitPlan);
  clearWaitForMaps(_waitPlanVersionLock, _waitPlanVersion);
  clearWaitForMaps(_waitCurrentLock, _waitCurrent);
  clearWaitForMaps(_waitCurrentVersionLock, _waitCurrentVersion);
}

void ClusterInfo::beginShutdown() {
  if (_planSyncer != nullptr) {
    _planSyncer->beginShutdown();
  }
  if (_curSyncer != nullptr) {
    _curSyncer->beginShutdown();
  }

  drainSyncers();
}

void ClusterInfo::waitForSyncersToStop() {
  drainSyncers();

  auto start = std::chrono::steady_clock::now();
  while ((_planSyncer != nullptr && _planSyncer->isRunning()) ||
         (_curSyncer != nullptr && _curSyncer->isRunning())) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(60)) {
      LOG_TOPIC("b8a5d", FATAL, Logger::CLUSTER)
          << "exiting prematurely as we failed to end syncer threads in "
             "ClusterInfo";
      FATAL_ERROR_EXIT();
    }
  }

  // make sure syncers threads must be gone
  _planSyncer.reset();
  _curSyncer.reset();
}

ClusterInfo::SyncerThread::SyncerThread(
    Server& server, std::string const& section,
    std::function<consensus::index_t()> const& f, AgencyCache& agencyCache)
    : ServerThread<Server>(server, section + "Syncer"),
      _section(section),
      _f(f),
      _agencyCache(agencyCache) {}

ClusterInfo::SyncerThread::~SyncerThread() { shutdown(); }

void ClusterInfo::SyncerThread::beginShutdown() { Thread::beginShutdown(); }

bool ClusterInfo::SyncerThread::start() {
  ThreadNameFetcher nameFetcher;
  std::string_view name = nameFetcher.get();
  if (name.empty()) {
    name = "unknown syncer thread";
  }

  LOG_TOPIC("38256", DEBUG, Logger::CLUSTER) << "Starting " << name;
  return Thread::start();
}

auto ClusterInfo::SyncerThread::call() noexcept
    -> std::optional<consensus::index_t> try {
  return _f();
} catch (basics::Exception const& ex) {
  if (ex.code() != TRI_ERROR_SHUTTING_DOWN) {
    LOG_TOPIC("9d1f5", WARN, Logger::CLUSTER)
        << "caught an error while loading " << _section << ": [" << ex.code()
        << "] " << ex.message();
  }
  return std::nullopt;
} catch (std::exception const& ex) {
  LOG_TOPIC("752c4", WARN, Logger::CLUSTER)
      << "caught an error while loading " << _section << ": " << ex.what();
  return std::nullopt;
} catch (...) {
  LOG_TOPIC("30968", WARN, Logger::CLUSTER)
      << "caught an error while loading " << _section;
  return std::nullopt;
}

void ClusterInfo::SyncerThread::sendNews() noexcept {
  {
    std::lock_guard lk(_m);
    _news = true;
  }
  _cv.notify_one();
}

void ClusterInfo::SyncerThread::waitForNews() noexcept {
  {
    std::unique_lock lk(_m);
    _cv.wait(lk, [&] { return _news; });
    _news = false;
  }
}

void ClusterInfo::SyncerThread::run() {
  auto const sendNewsCb = [this](auto&&) noexcept { sendNews(); };

  for (auto nextIndex = consensus::index_t{1}; !isStopping(); ++nextIndex) {
    _agencyCache.waitFor(nextIndex, AgencyCache::Executor::Direct)
        .thenFinal(sendNewsCb);

    waitForNews();

    // We update on every change; our _f (loadPlan/loadCurrent) decide for
    // themselves whether they need to do a real update. This way they can at
    // least bump _planIndex/_currentIndex and trigger corresponding callbacks.
    if (auto maybeIdx = call(); maybeIdx.has_value()) {
      nextIndex = *maybeIdx;
    }
  }
}

futures::Future<Result> ClusterInfo::waitForCurrent(
    consensus::index_t raftIndex) {
  READ_LOCKER(readLocker, _currentProt.lock);
  if (raftIndex <= _currentIndex) {
    return futures::makeFuture(Result());
  }

  if (_curSyncer == nullptr || _curSyncer->isStopping()) {
    return _syncerShutdownCode;
  }

  // intentionally don't release _storeLock here until we have inserted the
  // promise
  std::lock_guard w(_waitCurrentLock);
  return _waitCurrent.emplace(raftIndex, futures::Promise<Result>())
      ->second.getFuture();
}

futures::Future<Result> ClusterInfo::waitForCurrentVersion(
    uint64_t currentVersion) {
  READ_LOCKER(readLocker, _currentProt.lock);
  if (currentVersion <= _currentVersion) {
    return futures::makeFuture(Result());
  }

  if (_curSyncer == nullptr || _curSyncer->isStopping()) {
    return _syncerShutdownCode;
  }

  // intentionally don't release _storeLock here until we have inserted the
  // promise
  std::lock_guard w(_waitCurrentVersionLock);
  return _waitCurrentVersion
      .emplace(currentVersion, futures::Promise<Result>())
      ->second.getFuture();
}

void ClusterInfo::syncWaitForAllShardsToEstablishALeader() {
  // We wait for a maximum of 60 seconds for all shards to have a leader.
  // There may be a situation where we could not get a leader (e.g. shard
  // was already without a leader while taking the hot backup) or servers
  // not being responsive right now.
  for (size_t i = 0; i < 600; ++i) {
    READ_LOCKER(readLocker, _planProt.lock);
    READ_LOCKER(readLocker2, _currentProt.lock);
    // First we test that we have planned some shards.
    // This is to protect ourselves against a "no plan loaded yet" situation.
    // We will always have some shards (at least we will need the _users in
    // _system) Next we wait until we have a current entry for all the planned
    // shards. This is not super precise, but should be good enough.
    if (!_shardsToPlanServers.empty() &&
        _shardsToPlanServers.size() == _shardsToCurrentServers.size()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

futures::Future<Result> ClusterInfo::waitForPlan(consensus::index_t raftIndex) {
  READ_LOCKER(readLocker, _planProt.lock);
  if (raftIndex <= _planIndex) {
    return futures::makeFuture(Result());
  }

  if (_planSyncer == nullptr || _planSyncer->isStopping()) {
    return _syncerShutdownCode;
  }

  // intentionally don't release _storeLock here until we have inserted the
  // promise
  std::lock_guard w(_waitPlanLock);
  return _waitPlan.emplace(raftIndex, futures::Promise<Result>())
      ->second.getFuture();
}

futures::Future<Result> ClusterInfo::waitForPlanVersion(uint64_t planVersion) {
  READ_LOCKER(readLocker, _planProt.lock);
  if (planVersion <= _planVersion) {
    return futures::makeFuture(Result());
  }

  if (_planSyncer == nullptr || _planSyncer->isStopping()) {
    return _syncerShutdownCode;
  }

  // intentionally don't release _storeLock here until we have inserted the
  // promise
  std::lock_guard w(_waitPlanVersionLock);
  return _waitPlanVersion.emplace(planVersion, futures::Promise<Result>())
      ->second.getFuture();
}

futures::Future<Result> ClusterInfo::fetchAndWaitForPlanVersion(
    network::Timeout timeout) const {
  // Save the applicationServer, not the ClusterInfo, in case of shutdown.
  return cluster::fetchPlanVersion(timeout).thenValue(
      [&clusterFeature = _clusterFeature](auto maybePlanVersion) {
        if (maybePlanVersion.ok()) {
          auto planVersion = maybePlanVersion.get();
          auto& clusterInfo = clusterFeature.clusterInfo();
          return clusterInfo.waitForPlanVersion(planVersion);
        } else {
          return futures::Future<Result>{maybePlanVersion.result()};
        }
      });
}

futures::Future<Result> ClusterInfo::fetchAndWaitForCurrentVersion(
    network::Timeout timeout) const {
  // Save the applicationServer, not the ClusterInfo, in case of shutdown.
  return cluster::fetchCurrentVersion(timeout).thenValue(
      [&clusterInfo = _clusterFeature.clusterInfo()](auto maybeCurrentVersion) {
        if (maybeCurrentVersion.ok()) {
          auto currentVersion = maybeCurrentVersion.get();
          return clusterInfo.waitForCurrentVersion(currentVersion);
        } else {
          return futures::Future<Result>{maybeCurrentVersion.result()};
        }
      });
}

// Debugging output no need for consistency across locks
VPackBuilder ClusterInfo::toVelocyPack() {
  VPackBuilder dump;
  {
    {
      VPackObjectBuilder c(&dump);
      {
        READ_LOCKER(readLocker, _planProt.lock);
        dump.add(VPackValue("plan"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& i : _plan) {
            dump.add(i.first, i.second->slice());
          }
        }
        dump.add(VPackValue("plannedCollections"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& c : _plannedCollections) {
            dump.add(VPackValue(c.first));
            VPackArrayBuilder cs(&dump);
            for (auto const& col : *c.second) {
              dump.add(VPackValue(col.first));
            }
          }
        }
        dump.add(VPackValue("shardToName"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& s : _shardToName) {
            dump.add(std::string{s.first}, VPackValue(s.second));
          }
        }
        dump.add(VPackValue("shardServers"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& s : _shardsToPlanServers) {
            dump.add(VPackValue(s.first));
            VPackArrayBuilder a(&dump);
            for (auto const& sv : *s.second) {
              dump.add(VPackValue(sv));
            }
          }
        }
        dump.add(VPackValue("shardToShardGroupLeader"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& s : _shardToShardGroupLeader) {
            dump.add(std::string{s.first}, VPackValue(s.second));
          }
        }
        dump.add(VPackValue("shardGroups"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& s : _shardGroups) {
            dump.add(VPackValue(s.first));
            {
              VPackArrayBuilder d2(&dump);
              for (auto const& ss : *s.second) {
                dump.add(VPackValue(ss));
              }
            }
          }
        }
        dump.add(VPackValue("shards"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& s : _shards) {
            dump.add(VPackValue(s.first));
            VPackArrayBuilder a(&dump);
            for (auto const& sh : *s.second) {
              dump.add(VPackValue(sh));
            }
          }
        }
      }
      {
        READ_LOCKER(readLocker, _currentProt.lock);
        dump.add(VPackValue("current"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& i : _current) {
            dump.add(i.first, i.second->slice());
          }
        }
        dump.add(VPackValue("shardIds"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& i : _shardsToCurrentServers) {
            dump.add(VPackValue(i.first));
            VPackArrayBuilder a(&dump);
            for (auto const& i : *i.second) {
              dump.add(VPackValue(i));
            }
          }
        }
      }
      {
        READ_LOCKER(readLocker, _dbServersProt.lock);
        dump.add(VPackValue("DBServers"));
        {
          VPackObjectBuilder d(&dump);
          for (auto const& dbs : _dbServers) {
            dump.add(dbs.first, VPackValue(dbs.second));
          }
        }
      }
      {
        READ_LOCKER(readLocker, _coordinatorsProt.lock);
        dump.add(VPackValue("coordinators"));
        {
          VPackObjectBuilder c(&dump);
          for (auto const& crdn : _coordinators) {
            dump.add(crdn.first, VPackValue(crdn.second));
          }
        }
      }
    }
  }
  return dump;
}

void ClusterInfo::triggerWaiting(
    AssocMultiMap<uint64_t, futures::Promise<Result>>& mm,
    uint64_t commitIndex) {
  auto* scheduler = SchedulerFeature::SCHEDULER;
  auto pit = mm.begin();
  while (pit != mm.end()) {
    if (pit->first > commitIndex) {
      break;
    }
    if (scheduler && !_server.isStopping()) {
      scheduler->queue(
          RequestLane::CLUSTER_INTERNAL,
          [pp = std::move(pit->second)]() mutable { pp.setValue(Result()); });
    } else {
      pit->second.setValue(Result(_syncerShutdownCode));
    }
    pit = mm.erase(pit);
  }
}

auto ClusterInfo::getReplicatedLogPlanSpecification(replication2::LogId id)
    const -> ResultT<
        std::shared_ptr<replication2::agency::LogPlanSpecification const>> {
  READ_LOCKER(readLocker, _planProt.lock);

  auto it = _replicatedLogs.find(id);
  if (it == std::end(_replicatedLogs)) {
    return Result::fmt(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND, id);
  }

  TRI_ASSERT(it->second != nullptr);
  return it->second;
}

AnalyzerModificationTransaction::AnalyzerModificationTransaction(
    std::string_view database, ClusterInfo* ci, bool cleanup)
    : _clusterInfo(ci), _database(database), _cleanupTransaction(cleanup) {
  TRI_ASSERT(_clusterInfo);
}

AnalyzerModificationTransaction::~AnalyzerModificationTransaction() {
  try {
    abort();
  } catch (...) {
  }  // force no exceptions
  TRI_ASSERT(!_rollbackCounter && !_rollbackRevision);
}

int32_t AnalyzerModificationTransaction::getPendingCount() noexcept {
  return _pendingAnalyzerOperationsCount.load(std::memory_order_relaxed);
}

AnalyzersRevision::Revision AnalyzerModificationTransaction::buildingRevision()
    const noexcept {
  TRI_ASSERT(_buildingRevision !=
             AnalyzersRevision::LATEST);  // unstarted transation access
  return _buildingRevision;
}

Result AnalyzerModificationTransaction::start() {
  auto const endTime = TRI_microtime() + 5.0;  // arbitrary value.
  int32_t count =
      _pendingAnalyzerOperationsCount.load(std::memory_order_relaxed);
  // locking stage
  while (true) {
    // Do not let break out of cleanup mode.
    // Cleanup itself could start only from idle state.
    if (count < 0 || _cleanupTransaction) {
      count = 0;
    }
    if (_pendingAnalyzerOperationsCount.compare_exchange_weak(
            count, _cleanupTransaction ? -1 : count + 1,
            std::memory_order_acquire)) {
      break;
    }
    if (TRI_microtime() > endTime) {
      return Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                    "start modifying analyzer for database " + _database +
                        ": failed to acquire operation counter");
    }
  }
  _rollbackCounter = true;  // from now on we must release our counter

  if (_cleanupTransaction) {
    _buildingRevision =
        _clusterInfo->getAnalyzersRevision(_database, false)->getRevision();
    TRI_ASSERT(_clusterInfo->getAnalyzersRevision(_database, false)
                   ->getBuildingRevision() != _buildingRevision);
    return {};
  } else {
    auto res = _clusterInfo->startModifyingAnalyzerCoordinator(_database);
    _rollbackRevision = res.first.ok();
    _buildingRevision = res.second;
    return res.first;
  }
}

Result AnalyzerModificationTransaction::commit() {
  TRI_ASSERT(_rollbackCounter && (_rollbackRevision || _cleanupTransaction));
  auto res = _clusterInfo->finishModifyingAnalyzerCoordinator(
      _database, _cleanupTransaction);
  _rollbackRevision = res.fail() && !_cleanupTransaction;
  // if succesful revert mark our transaction as completed (otherwise postpone
  // it to abort call). for cleanup - always this attempt is completed (cleanup
  // should not waste much time). Will try next time
  if (res.ok() || _cleanupTransaction) {
    revertCounter();
  }
  return res;
}

Result AnalyzerModificationTransaction::abort() {
  if (!_rollbackCounter) {
    TRI_ASSERT(!_rollbackRevision);
    return Result();
  }
  Result res{};
  try {
    if (_rollbackRevision) {
      TRI_ASSERT(
          !_cleanupTransaction);  // cleanup transaction has nothing to rollback
      _rollbackRevision = false;  // ok, we tried. Even if failed -> recovery
                                  // job will do the rest
      res = _clusterInfo->finishModifyingAnalyzerCoordinator(_database, true);
    }
  } catch (...) {  // let`s be as safe as possible
    revertCounter();
    throw;
  }
  revertCounter();
  return res;
}

void AnalyzerModificationTransaction::revertCounter() {
  TRI_ASSERT(_rollbackCounter);
  if (_cleanupTransaction) {
    TRI_ASSERT(_pendingAnalyzerOperationsCount.load() == -1);
    _pendingAnalyzerOperationsCount = 0;
  } else {
    TRI_ASSERT(_pendingAnalyzerOperationsCount.load() > 0);
    _pendingAnalyzerOperationsCount.fetch_sub(1);
  }
  _rollbackCounter = false;
}

namespace {
template<typename T>
futures::Future<ResultT<T>> fetchNumberFromAgency(
    std::shared_ptr<cluster::paths::Path const> path,
    network::Timeout timeout) {
  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    agency::envelope::into_builder(builder)
        .read()
        .key(path->str())
        .end()
        .done();
  }

  auto fAacResult =
      AsyncAgencyComm().sendReadTransaction(timeout, std::move(trx));

  auto fResult =
      std::move(fAacResult).thenValue([path = std::move(path)](auto&& result) {
        if (result.ok() && result.statusCode() == fuerte::StatusOK) {
          return ResultT<T>::success(
              result.slice().at(0).get(path->vec()).template getNumber<T>());
        } else {
          return ResultT<T>::error(result.asResult());
        }
      });

  return fResult;
}
}  // namespace

futures::Future<ResultT<uint64_t>> cluster::fetchPlanVersion(
    network::Timeout timeout) {
  using namespace std::chrono_literals;

  auto planVersionPath = cluster::paths::root()->arango()->plan()->version();

  return fetchNumberFromAgency<uint64_t>(
      std::static_pointer_cast<paths::Path const>(std::move(planVersionPath)),
      timeout);
}

futures::Future<ResultT<uint64_t>> cluster::fetchCurrentVersion(
    network::Timeout timeout) {
  using namespace std::chrono_literals;

  auto currentVersionPath =
      cluster::paths::root()->arango()->current()->version();

  return fetchNumberFromAgency<uint64_t>(
      std::static_pointer_cast<paths::Path const>(
          std::move(currentVersionPath)),
      timeout);
}
