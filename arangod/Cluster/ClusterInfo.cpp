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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "ClusterInfo.h"

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/TimeString.h"
#include "Agency/TransactionBuilder.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberUtils.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/hashes.h"
#include "Basics/system-functions.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterCollectionCreationInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/RebootTracker.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/CommonDefines.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Utils/Events.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/VocbaseInfo.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/SmartVertexCollection.h"
#include "Enterprise/VocBase/VirtualCollection.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <chrono>

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

  void toVelocyPack(arangodb::velocypack::Builder& builder) const {
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

} // namespace

namespace {
void addToShardStatistics(arangodb::ShardStatistics& stats, 
                          std::unordered_set<std::string>& servers, 
                          arangodb::velocypack::Slice databaseSlice,
                          std::string const& restrictServer) {
  bool foundCollection = false;

  for (auto it : VPackObjectIterator(databaseSlice)) {
    VPackSlice collection = it.value;

    bool hasDistributeShardsLike = false;
    if (VPackSlice dsl = collection.get(arangodb::StaticStrings::DistributeShardsLike); dsl.isString()) {
      hasDistributeShardsLike = dsl.getStringLength() > 0;
    }

    bool foundShard = false;
    VPackSlice shards = collection.get("shards");
    for (auto pair : VPackObjectIterator(shards)) {
      int i = 0;
      for (auto const& serv : VPackArrayIterator(pair.value)) {
        if (!restrictServer.empty() && serv.stringRef() != restrictServer) {
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
            ++stats.realLeaders;
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

void addToShardStatistics(std::unordered_map<arangodb::ServerID, arangodb::ShardStatistics>& stats, 
                          arangodb::velocypack::Slice databaseSlice) {
  std::unordered_set<VPackStringRef> serversSeenForDatabase;

  for (auto it : VPackObjectIterator(databaseSlice)) {
    VPackSlice collection = it.value;

    bool hasDistributeShardsLike = false;
    if (VPackSlice dsl = collection.get(arangodb::StaticStrings::DistributeShardsLike); dsl.isString()) {
      hasDistributeShardsLike = dsl.getStringLength() > 0;
    }

    std::unordered_set<VPackStringRef> serversSeenForCollection;

    VPackSlice shards = collection.get("shards");
    for (auto pair : VPackObjectIterator(shards)) {
      int i = 0;
      for (auto const& serv : VPackArrayIterator(pair.value)) {
        auto& stat = stats[serv.copyString()];

        if (serversSeenForCollection.emplace(serv.stringRef()).second) {
          ++stat.collections;

          if (serversSeenForDatabase.emplace(serv.stringRef()).second) {
            ++stat.databases;
          }
        }

        ++stat.shards;
        if (i++ == 0) {
          ++stat.leaders;
          if (!hasDistributeShardsLike) {
            ++stat.realLeaders;
          }
        } else {
          ++stat.followers;
        }
      }
    }
  }
}

static inline arangodb::AgencyOperation IncreaseVersion() {
  return arangodb::AgencyOperation{"Plan/Version",
                                   arangodb::AgencySimpleOperationType::INCREMENT_OP};
}

static inline std::string collectionPath(std::string const& dbName,
                                         std::string const& collection) {
  return "Plan/Collections/" + dbName + "/" + collection;
}

inline std::string analyzersPath(std::string const& dbName) {
  return "Plan/Analyzers/" + dbName;
}

static inline arangodb::AgencyOperation CreateCollectionOrder(std::string const& dbName,
                                                              std::string const& collection,
                                                              VPackSlice const& info) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (!info.get("shards").isEmptyObject() &&
      !arangodb::basics::VelocyPackHelper::getBooleanValue(info, arangodb::StaticStrings::IsSmart,
                                                           false)) {
    TRI_ASSERT(info.hasKey(arangodb::StaticStrings::AttrIsBuilding));
    TRI_ASSERT(info.get(arangodb::StaticStrings::AttrIsBuilding).isBool());
    TRI_ASSERT(info.get(arangodb::StaticStrings::AttrIsBuilding).getBool() == true);
  }
#endif
  arangodb::AgencyOperation op{collectionPath(dbName, collection),
                               arangodb::AgencyValueOperationType::SET, info};
  return op;
}

static inline arangodb::AgencyPrecondition CreateCollectionOrderPrecondition(
    std::string const& dbName, std::string const& collection, VPackSlice const& info) {
  arangodb::AgencyPrecondition prec{collectionPath(dbName, collection),
                                    arangodb::AgencyPrecondition::Type::VALUE, info};
  return prec;
}

static inline arangodb::AgencyOperation CreateCollectionSuccess(
    std::string const& dbName, std::string const& collection, VPackSlice const& info) {
  TRI_ASSERT(!info.hasKey(arangodb::StaticStrings::AttrIsBuilding));
  return arangodb::AgencyOperation{collectionPath(dbName, collection),
                                   arangodb::AgencyValueOperationType::SET, info};
}

}  // namespace

#ifdef _WIN32
// turn off warnings about too long type name for debug symbols blabla in MSVC
// only...
#pragma warning(disable : 4503)
#endif

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::methods;

namespace StringUtils = arangodb::basics::StringUtils;

namespace {
// Read the collection from Plan; this is an object to have a valid VPack
// around to read from and to not have to carry around vpack builders.
// Might want to do the error handling with throw/catch?
class PlanCollectionReader {
 public:
  PlanCollectionReader(PlanCollectionReader const&&) = delete;
  PlanCollectionReader(PlanCollectionReader const&) = delete;
  explicit PlanCollectionReader(LogicalCollection const& collection) {
    std::string databaseName = collection.vocbase().name();
    std::string collectionID = std::to_string(collection.id().id());
    std::vector<std::string> path{
      AgencyCommHelper::path("Plan/Collections/" + databaseName + "/" + collectionID)};

    auto& agencyCache =
      collection.vocbase().server().getFeature<ClusterFeature>().agencyCache();
    consensus::index_t idx = 0;
    std::tie(_read, idx) = agencyCache.read(path);

    if (!_read->slice().isArray()) {
      _state = Result(
        TRI_ERROR_CLUSTER_READING_PLAN_AGENCY,
        "Could not retrieve " + path.front() + " from agency cache: " + _read->toJson());
      return;
    }

    _collection = _read->slice()[0];

    std::vector<std::string> vpath(
      {AgencyCommHelper::path(), "Plan", "Collections", databaseName, collectionID});

    if (!_collection.hasKey(vpath)) {
      _state = Result(
        TRI_ERROR_CLUSTER_READING_PLAN_AGENCY,
        "Could not retrieve " + path.front() + " from agency in version " + std::to_string(idx));
      return;
    }

    _collection = _collection.get(vpath);

    if (!_collection.isObject()) {
      _state = Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      return;
    }

    _state = Result();
  }
  VPackSlice indexes();
  VPackSlice slice() { return _collection; }
  Result state() { return _state; }

 private:
  consensus::query_t _read;
  Result _state;
  velocypack::Slice _collection;
};
} // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the JSON returns an error
////////////////////////////////////////////////////////////////////////////////

static inline bool hasError(VPackSlice const& slice) {
  return arangodb::basics::VelocyPackHelper::getBooleanValue(slice, StaticStrings::Error, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from a JSON
////////////////////////////////////////////////////////////////////////////////

static std::string extractErrorMessage(std::string const& shardId, VPackSlice const& slice) {
  std::string msg = " shardID:" + shardId + ": ";

  // add error message text
  msg += arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                            StaticStrings::ErrorMessage, "");

  // add error number
  if (slice.hasKey(StaticStrings::ErrorNum)) {
    VPackSlice const errorNum = slice.get(StaticStrings::ErrorNum);
    if (errorNum.isNumber()) {
      msg +=
        " (errNum="+arangodb::basics::StringUtils::itoa(errorNum.getNumericValue<uint32_t>())+")";
    }
  }

  return msg;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(uint64_t currentVersion)
    : _currentVersion(currentVersion) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::~CollectionInfoCurrent() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cluster info object
////////////////////////////////////////////////////////////////////////////////

struct ClusterInfoScale {
  static log_scale_t<float> scale() { return { std::exp(1.f), 0.f, 2500.f, 10 }; }
};

DECLARE_LEGACY_COUNTER(arangodb_load_current_accum_runtime_msec_total, "Accumulated runtime of Current loading [ms]");
DECLARE_HISTOGRAM(arangodb_load_current_runtime, ClusterInfoScale, "Current loading runtimes [ms]");
DECLARE_LEGACY_COUNTER(arangodb_load_plan_accum_runtime_msec_total, "Accumulated runtime of Plan loading [ms]");
DECLARE_HISTOGRAM(arangodb_load_plan_runtime, ClusterInfoScale, "Plan loading runtimes [ms]");

ClusterInfo::ClusterInfo(application_features::ApplicationServer& server,
                         AgencyCallbackRegistry* agencyCallbackRegistry,
                         ErrorCode syncerShutdownCode)
  : _server(server),
    _agency(server),
    _agencyCallbackRegistry(agencyCallbackRegistry),
    _rebootTracker(SchedulerFeature::SCHEDULER),
    _syncerShutdownCode(syncerShutdownCode),
    _planVersion(0),
    _planIndex(0),
    _currentVersion(0),
    _currentIndex(0),
    _planLoader(std::thread::id()),
    _uniqid(),
    _lpTimer(_server.getFeature<MetricsFeature>().add(arangodb_load_plan_runtime{})),
    _lpTotal(_server.getFeature<MetricsFeature>().add(arangodb_load_plan_accum_runtime_msec_total{})),
    _lcTimer(_server.getFeature<MetricsFeature>().add(arangodb_load_current_runtime{})),
    _lcTotal(_server.getFeature<MetricsFeature>().add(arangodb_load_current_accum_runtime_msec_total{})) {
  _uniqid._currentValue = 1ULL;
  _uniqid._upperValue = 0ULL;
  _uniqid._nextBatchStart = 1ULL;
  _uniqid._nextUpperValue = 0ULL;
  _uniqid._backgroundJobIsRunning = false;
  // Actual loading into caches is postponed until necessary

#ifdef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_syncerShutdownCode == TRI_ERROR_NO_ERROR || _syncerShutdownCode == TRI_ERROR_SHUTTING_DOWN);
#else
  TRI_ASSERT(_syncerShutdownCode == TRI_ERROR_SHUTTING_DOWN);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::~ClusterInfo() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup method which frees cluster-internal shared ptrs on shutdown
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::cleanup() {
  while (true) {
    {
      MUTEX_LOCKER(mutexLocker, _idLock);
      if (!_uniqid._backgroundJobIsRunning) {
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  MUTEX_LOCKER(mutexLocker, _planProt.mutex);

  {
    WRITE_LOCKER(writeLocker, _planProt.lock);
    _plannedViews.clear();
    _plannedCollections.clear();
    _shards.clear();
  }

  {
    WRITE_LOCKER(writeLocker, _currentProt.lock);
    _currentCollections.clear();
    _shardIds.clear();
  }
}

void ClusterInfo::triggerBackgroundGetIds() {
  // Trigger a new load of batches
  _uniqid._nextBatchStart = 1ULL;
  _uniqid._nextUpperValue = 0ULL;

  try {
    _idLock.assertLockedByCurrentThread();
    if (_uniqid._backgroundJobIsRunning) {
      return;
    }
    _uniqid._backgroundJobIsRunning = true;
    std::thread([this] {
      auto guardRunning = scopeGuard([this] {
        MUTEX_LOCKER(mutexLocker, _idLock);
        _uniqid._backgroundJobIsRunning = false;
      });

      uint64_t result;
      try {
        result = _agency.uniqid(MinIdsPerBatch, 0.0);
      } catch (std::exception const&) {
        return;
      }

      {
        MUTEX_LOCKER(mutexLocker, _idLock);

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
  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = agencyCache.read(std::vector<std::string>{"/"});
  auto res = acb->slice();

  if (!res.isNone()) {
    LOG_TOPIC("fe8ce", INFO, Logger::CLUSTER) << "Agency dump:\n" << res.toJson();
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
  MUTEX_LOCKER(mutexLocker, _idLock);

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

bool ClusterInfo::doesDatabaseExist(DatabaseID const& databaseID) {
  // Wait for sensible data in agency cache
  if (!_planProt.isValid) {
    Result r = waitForPlan(1).get();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).get();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.
  {
    size_t expectedSize;
    {
      READ_LOCKER(readLocker, _DBServersProt.lock);
      expectedSize = _DBServers.size();
    }

    // look up database by name:

    READ_LOCKER(readLocker, _planProt.lock);
    // _plannedDatabases is a map-type<DatabaseID, VPackSlice>
    auto it = _plannedDatabases.find(databaseID);

    if (it != _plannedDatabases.end()) {
      // found the database in Plan
      READ_LOCKER(readLocker, _currentProt.lock);
      // _currentDatabases is
      //     a map-type<DatabaseID, a map-type<ServerID, VPackSlice>>
      auto it2 = _currentDatabases.find(databaseID);

      if (it2 != _currentDatabases.end()) {
        // found the database in Current

        return ((*it2).second.size() >= expectedSize);
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
  std::vector<DatabaseID> result;

  if (_clusterId.empty()) {
    loadClusterId();
  }

  if (!_planProt.isValid) {
    Result r = waitForPlan(1).get();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).get();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  if (!_DBServersProt.isValid) {
    loadCurrentDBServers();
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.

  size_t expectedSize;
  {
    READ_LOCKER(readLocker, _DBServersProt.lock);
    expectedSize = _DBServers.size();
  }

  {
    READ_LOCKER(readLockerPlanned, _planProt.lock);
    READ_LOCKER(readLockerCurrent, _currentProt.lock);
    // _plannedDatabases is a map-type<DatabaseID, VPackSlice>
    auto it = _plannedDatabases.begin();

    while (it != _plannedDatabases.end()) {
      // _currentDatabases is:
      //   a map-type<DatabaseID, a map-type<ServerID, VPackSlice>>
      auto it2 = _currentDatabases.find((*it).first);

      if (it2 != _currentDatabases.end()) {
        if ((*it2).second.size() >= expectedSize) {
          result.push_back((*it).first);
        }
      }

      ++it;
    }
  }
  return result;
}

/// @brief Load cluster ID
void ClusterInfo::loadClusterId() {
  // Contact agency for /<prefix>/Cluster

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.get("Cluster");

  // Parse
  VPackSlice slice = acb->slice();
  if (slice.isString()) {
    _clusterId = slice.copyString();
  }
}

/// @brief create a new collecion object from the data, using the cache if possible
ClusterInfo::CollectionWithHash ClusterInfo::buildCollection(
  bool isBuilding, AllCollections::const_iterator existingCollections,
    std::string const& collectionId, arangodb::velocypack::Slice data,
    TRI_vocbase_t& vocbase, uint64_t planVersion) const {

  std::shared_ptr<LogicalCollection> collection;
  uint64_t hash = 0;

  if (!isBuilding &&
      existingCollections != _plannedCollections.end()) {
    // check if we already know this collection from a previous run...
    auto existing = (*existingCollections).second->find(collectionId);
    if (existing != (*existingCollections).second->end()) {
      CollectionWithHash const& previous = (*existing).second;
      // compare the hash values of what is in the cache with the hash of the collection
      // a hash value of 0 means that the collection must not be read from the cache,
      // potentially because it contains a link to a view (which would require more
      // complex dependency management)
      if (previous.hash != 0) {
        // calculate hash value. we are using Slice::hash() here intentionally in contrast
        // to the slower Slice::normalizedHash(), as the only source for the VelocyPack
        // is the agency/agency cache, which will always create the data in the same way.
        hash = data.hash();
        // if for some reason the generated hash value is 0, too, we simply don't cache
        // this collection. This is not a problem, as it will not affect correctness but
        // will only lead to one collection less being cached.
        if (previous.hash == hash) {
          // hashes are identical. reuse the collection from cache
          // this is very beneficial for performance, because we can avoid rebuilding the
          // entire LogicalCollection object!
          collection = previous.collection;
        }
      }
    }
  }

  // collection may be a nullptr here if no such collection exists in the cache, or if the
  // collection is in building stage
  if (collection == nullptr) {
    // no previous version of the collection exists, or its hash value has changed
    collection = createCollectionObject(data, vocbase);
    TRI_ASSERT(collection != nullptr);

    if (!isBuilding) {
      auto indexes = collection->getIndexes();
      // if the collection has a link to a view, there are dependencies between collection
      // objects and view objects. in this case, we need to disable the collection caching
      // optimization
      bool const hasViewLink = std::any_of(indexes.begin(), indexes.end(), [](auto const& index) {
        return (index->type() == Index::TRI_IDX_TYPE_IRESEARCH_LINK);
      });
      if (hasViewLink) {
        // we do have a view. set hash to 0, which will disable the caching optimization
        hash = 0;
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

/// @brief helper function to build a new LogicalCollection object from the velocypack
/// input
/*static*/ std::shared_ptr<LogicalCollection> ClusterInfo::createCollectionObject(
    arangodb::velocypack::Slice data, TRI_vocbase_t& vocbase) {
#ifdef USE_ENTERPRISE
  auto isSmart = data.get(StaticStrings::IsSmart);

  if (isSmart.isTrue()) {
    auto type = data.get(StaticStrings::DataSourceType);

    if (type.isInteger() && type.getUInt() == TRI_COL_TYPE_EDGE) {
      return std::make_shared<VirtualSmartEdgeCollection>(vocbase, data);
    }
    return std::make_shared<SmartVertexCollection>(vocbase, data);
  }
#endif
  return std::make_shared<LogicalCollection>(vocbase, data, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about our plan
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixPlan = "Plan";

void ClusterInfo::loadPlan() {

  using namespace std::chrono;
  using clock = std::chrono::high_resolution_clock;

  bool const isCoordinator = ServerState::instance()->isCoordinator();

  auto start = clock::now();

  auto& clusterFeature = _server.getFeature<ClusterFeature>();
  auto& databaseFeature = _server.getFeature<DatabaseFeature>();
  auto& agencyCache = clusterFeature.agencyCache();

  // We need to wait for any cluster operation, which needs access to the
  // agency cache for it to become ready. The essentials in the cluster, namely
  // ClusterInfo etc, need to start after first poll result from the agency.
  // This is of great importance to not accidentally delete data facing an
  // empty agency. There are also other measures that guard against such an
  // outcome. But there is also no point continuing with a first agency poll.
  Result r = agencyCache.waitFor(1).get();
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }

  MUTEX_LOCKER(mutexLocker, _planProt.mutex);  // only one may work at a time

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
    _newPlannedViews = _plannedViews;  // Create a copy, since we might not visit all databases
    _planLoader = std::this_thread::get_id();
  }

  // ensure we'll eventually reset plan loader
  auto resetLoader = scopeGuard([this, start]() {
    _planLoader = std::thread::id();
    _newPlannedViews.clear();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto diff = clock::now() - start;
    if (diff > std::chrono::milliseconds(500)) {
      LOG_TOPIC("66666", WARN, Logger::CLUSTER)
        << "Loading the new plan took: " << std::chrono::duration<double>(diff).count() << "s";
    }
#else
    (void)start;
#endif
  });

  bool planValid = true;  // has the loadPlan completed without skipping valid objects
                          // we will set in the end

  uint64_t planIndex;
  uint64_t planVersion;
  {
    READ_LOCKER(guard, _planProt.lock);
    planIndex = _planIndex;
    planVersion = _planVersion;
  }

  bool changed = false;
  auto changeSet = agencyCache.changedSince("Plan", planIndex); // also delivers plan/version
  decltype(_plan) newPlan;
  {
    READ_LOCKER(readLocker, _planProt.lock);
    newPlan = _plan;
    for (auto const& db : changeSet.dbs) { // Databases
      newPlan[db.first] = db.second;
      changed = true;
    }
    if (changeSet.rest != nullptr) {       // Rest
      newPlan[std::string()] = changeSet.rest;
      changed = true;
    }
  }

  if (!changed && planVersion == changeSet.version) {
    WRITE_LOCKER(writeLocker, _planProt.lock);
    _planIndex = changeSet.ind;
    return;
  }

  decltype(_plannedDatabases) newDatabases;
  std::set<std::string> buildingDatabases;
  decltype(_plannedCollections) newCollections;
  decltype(_shards) newShards;
  decltype(_shardServers) newShardServers;
  decltype(_shardToName) newShardToName;
  decltype(_dbAnalyzersRevision) newDbAnalyzersRevision;

  bool swapDatabases = false;
  bool swapCollections = false;
  bool swapViews = false;
  bool swapAnalyzers = false;

  {
    READ_LOCKER(guard, _planProt.lock);
    auto start = std::chrono::steady_clock::now();
    newDatabases = _plannedDatabases;
    newCollections = _plannedCollections;
    newShards = _shards;
    newShardServers = _shardServers;
    newShardToName = _shardToName;
    newDbAnalyzersRevision = _dbAnalyzersRevision;
    auto ende = std::chrono::steady_clock::now();
    LOG_TOPIC("feee1", TRACE, Logger::CLUSTER)
        << "Time for copy operation in loadPlan: "
        << std::chrono::duration_cast<std::chrono::nanoseconds>(ende - start).count()
        << " ns";
  }

  std::string name;

  // mark for swap even if no databases present to ensure dangling datasources are removed
  // TODO: Maybe optimize. Else remove altogether
  if (!changeSet.dbs.empty()) {
    swapDatabases = true;
    swapCollections = true;
    swapViews = true;
    swapAnalyzers = true;
  }

  for (auto const& database : changeSet.dbs) {

    if (database.first.empty()) { // Rest of plan
      continue;
    }

    name = database.first;
    auto dbSlice = database.second->slice()[0];
    std::vector<std::string> dbPath{AgencyCommHelper::path(), "Plan", "Databases", name};

    // Dropped from Plan?
    if (!dbSlice.hasKey(dbPath)) {
      std::shared_ptr<VPackBuilder> plan = nullptr;
      {
        READ_LOCKER(guard, _planProt.lock);
        auto it = _plan.find(name);
        if (it != _plan.end()) {
          plan = it->second;
        }
      }
      if (plan != nullptr) {
        std::vector<std::string> colPath{AgencyCommHelper::path(), "Plan", "Collections", name};
        if (plan->slice()[0].hasKey(colPath)) {
          for (auto const& col : VPackObjectIterator(plan->slice()[0].get(colPath))) {
            if (col.value.hasKey("shards")) {
              for (auto const& shard : VPackObjectIterator(col.value.get("shards"))) {
                auto const& shardName = shard.key.copyString();
                newShards.erase(shardName);
                newShardServers.erase(shardName);
                newShardToName.erase(shardName);
              }
            }
          }
        }
      }
      newDatabases.erase(name);
      newPlan.erase(name);
      continue;
    }

    dbSlice = dbSlice.get(dbPath);
    bool const isBuilding = dbSlice.hasKey(StaticStrings::AttrIsBuilding);

    // We create the database object on the coordinator here, because
    // it is used to create LogicalCollection instances further
    // down in this function.
    if (isCoordinator && !isBuilding) {
      TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(name);
      if (vocbase == nullptr) {
        // database does not yet exist, create it now

        // create a local database object...
        arangodb::CreateDatabaseInfo info(_server, ExecContext::current());
        Result res = info.load(dbSlice, VPackSlice::emptyArraySlice());

        if (res.fail()) {
          LOG_TOPIC("94357", ERR, arangodb::Logger::AGENCY)
            << "validating data for local database '" << name
            << "' failed: " << res.errorMessage();
        } else {
          std::string dbName = info.getName();
          res = databaseFeature.createDatabase(std::move(info), vocbase);
          events::CreateDatabase(dbName, res, ExecContext::current());

          if (res.fail()) {
            LOG_TOPIC("91870", ERR, arangodb::Logger::AGENCY)
              << "creating local database '" << name
              << "' failed: " << res.errorMessage();
          }
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

  // Ensure views are being created BEFORE collections to allow
  // links find them
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

  // Now the same for views: // TODO change
  for (auto const& database : changeSet.dbs) {

    if (database.first.empty()) { // Rest of plan
      continue;
    }
    auto const& databaseName = database.first;
    _newPlannedViews.erase(databaseName);   // clear views for this database
    std::vector<std::string> viewsPath {
      AgencyCommHelper::path(), "Plan", "Views", databaseName};
    auto viewsSlice = database.second->slice()[0];
    if (!viewsSlice.hasKey(viewsPath)) {
      continue;
    }
    viewsSlice = viewsSlice.get(viewsPath);

    auto* vocbase = databaseFeature.lookupDatabase(databaseName);

    if (!vocbase) {
      // No database with this name found.
      // We have an invalid state here.
      LOG_TOPIC("f105f", WARN, Logger::AGENCY)
        << "No database '" << databaseName << "' found,"
        << " corresponding view will be ignored for now and the "
        << "invalid information will be repaired. VelocyPack: "
        << viewsSlice.toJson();
      // cannot find vocbase for defined views (allow empty views for missing vocbase)
      planValid &= !viewsSlice.length();
      continue;
    }

    for (auto const& viewPairSlice : velocypack::ObjectIterator(viewsSlice, true)) {
      auto const& viewSlice = viewPairSlice.value;

      if (!viewSlice.isObject()) {
        LOG_TOPIC("2487b", INFO, Logger::AGENCY)
          << "View entry is not a valid json object."
          << " The view will be ignored for now and the invalid "
          << "information will be repaired. VelocyPack: " << viewSlice.toJson();
        continue;
      }

      auto const viewId = viewPairSlice.key.copyString();

      try {
        LogicalView::ptr view;
        auto res = LogicalView::instantiate(view, *vocbase, viewPairSlice.value);

        if (!res.ok() || !view) {
          LOG_TOPIC("b0d48", ERR, Logger::AGENCY)
            << "Failed to create view '" << viewId
            << "'. The view will be ignored for now and the invalid "
            << "information will be repaired. VelocyPack: " << viewSlice.toJson();
          planValid = false;  // view creation failure
          continue;
        }

        auto& views = _newPlannedViews[databaseName];

        // register with guid/id/name
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
          << "information will be repaired. VelocyPack: " << viewSlice.toJson();
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

    if (database.first.empty()) { // Rest of plan
      continue;
    }
    auto const& databaseName = database.first;
    auto analyzerSlice = database.second->slice()[0];

    std::vector<std::string> analyzersPath {
      AgencyCommHelper::path(), "Plan", "Analyzers", databaseName};
    if(!analyzerSlice.hasKey(analyzersPath)) { // DB Gone
      newDbAnalyzersRevision.erase(databaseName);
      continue;
    }
    analyzerSlice = analyzerSlice.get(analyzersPath);

    auto* vocbase = databaseFeature.lookupDatabase(databaseName);

    if (!vocbase) {
      // No database with this name found.
      // We have an invalid state here.
      LOG_TOPIC("e5a6b", WARN, Logger::AGENCY)
        << "No database '" << databaseName << "' found,"
        << " corresponding analyzer will be ignored for now and the "
        << "invalid information will be repaired. VelocyPack: "
        << analyzerSlice.toJson();
      // cannot find vocbase for defined analyzers (allow empty analyzers for missing vocbase)
      planValid &= !analyzerSlice.length();

      continue;
    }

    std::string revisionError;
    auto revision = AnalyzersRevision::fromVelocyPack(analyzerSlice, revisionError);
    if (revision) {
      newDbAnalyzersRevision[databaseName] = std::move(revision);
    } else {
      LOG_TOPIC("e3f08", WARN, Logger::AGENCY)
        << "Invalid analyzer data for database '" << databaseName << "'"
        << " Error:" << revisionError << ", "
        << " corresponding analyzers revision will be ignored for now and the "
        << "invalid information will be repaired. VelocyPack: "
        << analyzerSlice.toJson();
    }
  }
  TRI_IF_FAILURE("AlwaysSwapAnalyzersRevision") {
    swapAnalyzers = true;
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
  for (auto const& database : changeSet.dbs) {

    if (database.first.empty()) { // Rest of plan
      continue;
    }

    auto const& databaseName = database.first;

    auto collectionsSlice = database.second->slice()[0];

    std::vector<std::string> collectionsPath{
      AgencyCommHelper::path(), "Plan", "Collections", databaseName};
    if (!collectionsSlice.hasKey(collectionsPath)) {
      auto it = newCollections.find(databaseName);
      if (it != newCollections.end()) {
        for (auto const& collection : *(it->second)) {
          auto& collectionId = collection.first;
          newShards.erase(collectionId); // delete from maps with shardID as key
          newShardToName.erase(collectionId);
        }
        it = newCollections.erase(it);
      }
      continue;
    }
    collectionsSlice = collectionsSlice.get(collectionsPath);

    auto databaseCollections = std::make_shared<DatabaseCollections>();

    // Skip databases that are still building.
    if (buildingDatabases.find(databaseName) != buildingDatabases.end()) {
      continue;
    }

    auto* vocbase = databaseFeature.lookupDatabase(databaseName);

    if (!vocbase) {
      // No database with this name found.
      // We have an invalid state here.
      LOG_TOPIC("83d4c", DEBUG, Logger::AGENCY)
        << "No database '" << databaseName << "' found,"
        << " corresponding collection will be ignored for now and the "
        << "invalid information will be repaired. VelocyPack: "
        << collectionsSlice.toJson();
      planValid &= !collectionsSlice.length();  // cannot find vocbase for defined collections (allow empty collections for missing vocbase)

      continue;
    }

    // an iterator to all collections in the current database (from the previous round)
    // we can safely keep this iterator around because we hold the read-lock on _planProt here.
    // reusing the lookup positions helps avoiding redundant lookups into _plannedCollections
    // for the same database

    AllCollections::const_iterator existingCollections,
      stillExistingCollections = newCollections.find(databaseName);
    {
      READ_LOCKER(guard, _planProt.lock);
      existingCollections = _plannedCollections.find(databaseName);
    }

    if (stillExistingCollections != newCollections.end()) {
      auto const& np = newPlan.find(databaseName);
      if (np != newPlan.end()) {
        auto nps = np->second->slice()[0];
        for (auto const& ec : *(stillExistingCollections->second)) {
          auto const& cid = ec.first;
          if (!std::isdigit(cid.front())) {
            continue;
          }
          collectionsPath.push_back(cid);
          if (!nps.hasKey(collectionsPath))  { // collection gone
            collectionsPath.push_back("shards");
            READ_LOCKER(guard, _planProt.lock);
            for (auto const& sh :
                   VPackObjectIterator(_plan.find(databaseName)->second->slice()[0].get(collectionsPath))) {
              auto const& shardId = sh.key.copyString();
              newShards.erase(shardId);
              newShardServers.erase(shardId);
              newShardToName.erase(shardId);
            }
            collectionsPath.pop_back();
          }
          collectionsPath.pop_back();
        }
      }
    }

    for (auto const& collectionPairSlice : velocypack::ObjectIterator(collectionsSlice)) {
      auto const& collectionSlice = collectionPairSlice.value;

      if (!collectionSlice.isObject()) {
        LOG_TOPIC("0f689", WARN, Logger::AGENCY)
          << "Collection entry is not a valid json object."
          << " The collection will be ignored for now and the invalid "
          << "information will be repaired. VelocyPack: " << collectionSlice.toJson();

        continue;
      }

      bool const isBuilding = isCoordinator &&
        arangodb::basics::VelocyPackHelper::getBooleanValue(
          collectionSlice, StaticStrings::AttrIsBuilding, false);

      auto const collectionId = collectionPairSlice.key.copyString();

        // check if we already know this collection (i.e. have it in our local cache).
        // we do this to avoid rebuilding LogicalCollection objects from scratch in
        // every iteration
        // the cache check is very coarse-grained: it simply hashes the Plan VelocyPack
        // data for the collection, and will only reuse a collection from the cache if
        // the hash is identical.
      CollectionWithHash cwh = buildCollection(isBuilding, existingCollections, collectionId, collectionSlice, *vocbase, changeSet.version);
      auto& newCollection = cwh.collection;
      TRI_ASSERT(newCollection != nullptr);

      try {
        auto& collectionName = newCollection->name();

        // NOTE: This is building has the following feature. A collection needs to be working on
        // all DBServers to allow replication to go on, also we require to have the shards planned.
        // BUT: The users should not be able to detect these collections.
        // Hence we simply do NOT add the collection to the coordinator local vocbase, which happens
        // inside the IF

        if (!isBuilding) {
          // register with name as well as with id:
          databaseCollections->try_emplace(collectionName, cwh);
          databaseCollections->try_emplace(collectionId, cwh);
        }

        auto shardIDs = newCollection->shardIds();
        auto shards = std::make_shared<std::vector<std::string>>();
        shards->reserve(shardIDs->size());
        newShardToName.reserve(shardIDs->size());

        for (auto const& p : *shardIDs) {
          TRI_ASSERT(p.first.size() >= 2);
          shards->push_back(p.first);
          newShardServers.insert_or_assign(p.first, p.second);
          newShardToName.insert_or_assign(p.first, newCollection->name());
        }

        // Sort by the number in the shard ID ("s0000001" for example):
        ShardingInfo::sortShardNamesNumerically(*shards);
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
          << "information will be repaired. VelocyPack: " << collectionSlice.toJson();

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
    newCollections.insert_or_assign(std::move(databaseName), std::move(databaseCollections));
  }

  if (isCoordinator) {
    auto systemDB = _server.getFeature<arangodb::SystemDatabaseFeature>().use();
    if (systemDB && systemDB->shardingPrototype() == ShardingPrototype::Undefined) {
      // system database does not have a shardingPrototype set...
      // sharding prototype of _system database defaults to _users nowadays
      systemDB->setShardingPrototype(ShardingPrototype::Users);
      // but for "old" databases it may still be "_graphs". we need to find out!
      // find _system database in Plan
      auto it = newCollections.find(StaticStrings::SystemDatabase);
      if (it != newCollections.end()) {
        // find _graphs collection in Plan
        auto it2 = (*it).second->find(StaticStrings::GraphCollection);
        if (it2 != (*it).second->end()) {
          // found!
          if ((*it2).second.collection->distributeShardsLike().empty()) {
            // _graphs collection has no distributeShardsLike, so it is
            // the prototype!
            systemDB->setShardingPrototype(ShardingPrototype::Graphs);
          }
        }
      }
    }
  }

  WRITE_LOCKER(writeLocker, _planProt.lock);

  _planVersion = changeSet.version;
  _planIndex = changeSet.ind;
  _plan.swap(newPlan);
  LOG_TOPIC("54321", DEBUG, Logger::CLUSTER)
    << "Updating ClusterInfo plan: version=" << _planVersion << " index=" << _planIndex;

  if (swapDatabases) {
    _plannedDatabases.swap(newDatabases);
  }

  if (swapCollections) {
    _plannedCollections.swap(newCollections);
    _shards.swap(newShards);
    _shardServers.swap(newShardServers);
    _shardToName.swap(newShardToName);
  }

  if (swapViews) {
    _plannedViews.swap(_newPlannedViews);
  }
  if (swapAnalyzers) {
    _dbAnalyzersRevision.swap(newDbAnalyzersRevision);
  }

  if (planValid) {
    _planProt.isValid = true;
  }

  clusterFeature.addDirty(changeSet.dbs);

  {
    std::lock_guard w(_waitPlanLock);
    triggerWaiting(_waitPlan, _planIndex);
    auto heartbeatThread = clusterFeature.heartbeatThread();
    if (heartbeatThread) {
      // In the unittests, there is no heartbeatthread, and we do not need to notify
      heartbeatThread->notify();
    }
  }

  {
    std::lock_guard w(_waitPlanVersionLock);
    triggerWaiting(_waitPlanVersion, _planVersion);
  }

  auto diff = duration<float, std::milli>(clock::now() - start).count();
  _lpTotal += static_cast<uint64_t>(diff);
  _lpTimer.count(diff);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrent = "Current";

void ClusterInfo::loadCurrent() {
  using namespace std::chrono;
  using clock = std::chrono::high_resolution_clock;

  auto start = clock::now();

  // We need to update ServersKnown to notice rebootId changes for all servers.
  // To keep things simple and separate, we call loadServers here instead of
  // trying to integrate the servers upgrade code into loadCurrent, even if that
  // means small bits of the plan are read twice.
  loadServers();

  auto& feature = _server.getFeature<ClusterFeature>();
  auto& agencyCache = feature.agencyCache();
  auto currentBuilder = std::make_shared<arangodb::velocypack::Builder>();

  // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _currentProt.mutex);  // only one may work at a time

  uint64_t currentIndex;
  uint64_t currentVersion;
  {
    READ_LOCKER(guard, _currentProt.lock);
    currentIndex = _currentIndex;
    currentVersion = _currentVersion;
  }
  decltype(_current) newCurrent;

  bool changed = false;
  auto changeSet = agencyCache.changedSince("Current", currentIndex);
  {
    READ_LOCKER(readLocker, _currentProt.lock);
    newCurrent = _current;
    for (auto const& db : changeSet.dbs) { // Databases
      newCurrent[db.first] = db.second;
      changed = true;
    }
    if (changeSet.rest != nullptr) {       // Rest
      newCurrent[std::string()] = changeSet.rest;
      changed = true;
    }
  }

  if (!changed && currentVersion == changeSet.version) {
    WRITE_LOCKER(writeLocker, _currentProt.lock);
    _currentIndex = changeSet.ind;
    return;
  }

  decltype(_currentDatabases) newDatabases;
  decltype(_currentCollections) newCollections;
  decltype(_shardIds) newShardIds;

  {
    READ_LOCKER(guard, _currentProt.lock);
    newDatabases = _currentDatabases;
    newCollections = _currentCollections;
    newShardIds = _shardIds;
  }

  bool swapDatabases = false;
  bool swapCollections = false;

  // Current/Databases
  for (auto const& database : changeSet.dbs) {

    auto const databaseName = database.first;
    if (databaseName.empty()) {
      continue;
    }

    std::vector<std::string> colsPath {
      AgencyCommHelper::path(), "Current", "Collections", databaseName};
    std::vector<std::string> dbPath{
      AgencyCommHelper::path(), "Current", "Databases", databaseName};
    auto databaseSlice = database.second->slice()[0];

    // Database missing in Current
    if (!databaseSlice.hasKey(dbPath)) {

      // newShardIds
      std::shared_ptr<VPackBuilder> db = nullptr;
      {
        READ_LOCKER(guard, _currentProt.lock);
        auto const it = _current.find(databaseName);
        if (it != _current.end()) {
          db = it->second;
        }
      }
      std::vector<std::string> colPath {
        AgencyCommHelper::path(), "Current", "Collections", databaseName};

      if (db != nullptr) {
        if (db->slice()[0].hasKey(colPath)) {
          auto const colsSlice = db->slice()[0].get(colPath);
          if (colsSlice.isObject()) {
            for (auto const cc : VPackObjectIterator(colsSlice)) {
              if (cc.value.isObject()) {
                for (auto const cs : VPackObjectIterator(cc.value)) {
                  newShardIds.erase(cs.key.copyString());
                }
              }
            }
          }
        }
      }
      swapDatabases = true;
      continue;
    }
    databaseSlice = databaseSlice.get(dbPath);

    std::unordered_map<ServerID, velocypack::Slice> serverList;
    if (databaseSlice.isObject()) {
      for (auto const& serverSlicePair : VPackObjectIterator(databaseSlice)) {
        serverList.try_emplace(serverSlicePair.key.copyString(), serverSlicePair.value);
      }
    }

    newDatabases.insert_or_assign(std::move(databaseName), std::move(serverList));
    swapDatabases = true;
  }

  // Current/Collections
  for (auto const& database : changeSet.dbs) {

    auto const databaseName = database.first;
    if (databaseName.empty()) {
      continue;
    }
    swapCollections = true;

    std::vector<std::string> dbPath{
      AgencyCommHelper::path(), "Current", "Collections", databaseName};
    auto databaseSlice = database.second->slice()[0];
    if (!databaseSlice.hasKey(dbPath)) {
      newCollections.erase(databaseName);
      swapCollections = true;
      continue;
    }
    databaseSlice = databaseSlice.get(dbPath);

    DatabaseCollectionsCurrent databaseCollections;

    auto const existingCollections = newCollections.find(databaseName);
    if (existingCollections != newCollections.end()) {
      auto const& nc = newCurrent.find(databaseName);
      if (nc != newCurrent.end()) {
        auto ncs = nc->second->slice()[0];
        std::vector<std::string> path{
          AgencyCommHelper::path(), "Current", "Collections", databaseName};
        for (auto const& ec : existingCollections->second) {
          auto const& cid = ec.first;
          path.push_back(cid);
          if (ncs.hasKey(path)) {
            std::shared_ptr<VPackBuilder> cur;
            {
              READ_LOCKER(guard, _currentProt.lock);
              cur = _current.find(databaseName)->second;
            }
            auto const cc = cur->slice()[0].get(path);
            for (auto const& sh : VPackObjectIterator(cc)) {
              path.push_back(sh.key.copyString());
              if (!ncs.hasKey(path)) {
                newShardIds.erase(path.back());
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

      for (auto const& shardSlice : velocypack::ObjectIterator(collectionSlice.value)) {
        std::string shardID = shardSlice.key.copyString();

        collectionDataCurrent->add(shardID, shardSlice.value);

        // Note that we have only inserted the CollectionInfoCurrent under
        // the collection ID and not under the name! It is not possible
        // to query the current collection info by name. This is because
        // the correct place to hold the current name is in the plan.
        // Thus: Look there and get the collection ID from there. Then
        // ask about the current collection info.

        // Now take note of this shard and its responsible server:
        auto servers = std::make_shared<std::vector<ServerID>>(
          collectionDataCurrent->servers(shardID)  // args
          );

        newShardIds.insert_or_assign(std::move(shardID), std::move(servers));
      }

      databaseCollections.try_emplace(std::move(collectionName), std::move(collectionDataCurrent));
    }

    newCollections.insert_or_assign(std::move(databaseName), std::move(databaseCollections));
  }

  // Now set the new value:
  WRITE_LOCKER(writeLocker, _currentProt.lock);

  _current.swap(newCurrent);
  _currentVersion = changeSet.version;
  _currentIndex = changeSet.ind;
  LOG_TOPIC("feddd", DEBUG, Logger::CLUSTER)
      << "Updating current in ClusterInfo: version=" << changeSet.version
      << " index=" << _currentIndex;

  if (swapDatabases) {
    _currentDatabases.swap(newDatabases);
  }

  if (swapCollections) {
    LOG_TOPIC("b4059", TRACE, Logger::CLUSTER)
        << "Have loaded new collections current cache!";
    _currentCollections.swap(newCollections);
    _shardIds.swap(newShardIds);
  }

  _currentProt.isValid = true;
  feature.addDirty(changeSet.dbs);

  {
    std::lock_guard w(_waitCurrentLock);
    triggerWaiting(_waitCurrent, _currentIndex);
    auto heartbeatThread = _server.getFeature<ClusterFeature>().heartbeatThread();
    if (heartbeatThread) {
      // In the unittests, there is no heartbeatthread, and we do not need to notify
      heartbeatThread->notify();
    }
  }

  {
    std::lock_guard w(_waitCurrentVersionLock);
    triggerWaiting(_waitCurrentVersion, _currentVersion);
  }

  auto diff = duration<float, std::milli>(clock::now() - start).count();
  _lcTotal += static_cast<uint64_t>(diff);
  _lcTimer.count(diff);
}

/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once
/// if the collection is not found afterwards, this method will throw an
/// exception
std::shared_ptr<LogicalCollection> ClusterInfo::getCollection(DatabaseID const& databaseID,
                                                              CollectionID const& collectionID) {
  auto c = getCollectionNT(databaseID, collectionID);

  if (c == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   getCollectionNotFoundMsg(databaseID, collectionID)  // message
    );
  }
  return c;
}

std::shared_ptr<LogicalCollection> ClusterInfo::getCollectionNT(DatabaseID const& databaseID,
                                                                CollectionID const& collectionID) {
  if (!_planProt.isValid) {
    Result r = waitForPlan(1).get();
    if (r.fail()) {
      return nullptr;
    }
  }

  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  AllCollections::const_iterator it = _plannedCollections.find(databaseID);

  if (it != _plannedCollections.end()) {
    // look up collection by id (or by name)
    DatabaseCollections::const_iterator it2 = (*it).second->find(collectionID);

    if (it2 != (*it).second->end()) {
      return (*it2).second.collection;
    }
  }

  return nullptr;
}

std::shared_ptr<LogicalDataSource> ClusterInfo::getCollectionOrViewNT(DatabaseID const& databaseID,
                                                                      std::string const& name) {
  if (!_planProt.isValid) {
    Result r = waitForPlan(1).get();
    if (r.fail()) {
      return nullptr;
    }
  }

  READ_LOCKER(readLocker, _planProt.lock);

  // look up collection first
  {
    // look up database by id
    auto it = _plannedCollections.find(databaseID);

    if (it != _plannedCollections.end()) {
      // look up collection by id (or by name)
      auto it2 = (*it).second->find(name);

      if (it2 != (*it).second->end()) {
        return (*it2).second.collection;
      }
    }
  }

  // look up views next
  {
    // look up database by id
    auto it = _plannedViews.find(databaseID);

    if (it != _plannedViews.end()) {
      // look up collection by id (or by name)
      auto it2 = (*it).second.find(name);

      if (it2 != (*it).second.end()) {
        return (*it2).second;
      }
    }
  }

  return nullptr;

}

std::string ClusterInfo::getCollectionNotFoundMsg(DatabaseID const& databaseID,
                                                  CollectionID const& collectionID) {
  return "Collection not found: " + collectionID + " in database " + databaseID;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalCollection>> const ClusterInfo::getCollections(DatabaseID const& databaseID) {
  std::vector<std::shared_ptr<LogicalCollection>> result;

  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  AllCollections::const_iterator it = _plannedCollections.find(databaseID);

  if (it == _plannedCollections.end()) {
    return result;
  }

  // iterate over all collections
  DatabaseCollections::const_iterator it2 = (*it).second->begin();
  while (it2 != (*it).second->end()) {
    char c = (*it2).first[0];

    if (c < '0' || c > '9') {
      // skip collections indexed by id
      result.push_back((*it2).second.collection);
    }

    ++it2;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection in current. This returns information about
/// all shards in the collection.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CollectionInfoCurrent> ClusterInfo::getCollectionCurrent(
    DatabaseID const& databaseID, CollectionID const& collectionID) {

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).get();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  READ_LOCKER(readLocker, _currentProt.lock);
  // look up database by id
  AllCollectionsCurrent::const_iterator it = _currentCollections.find(databaseID);

  if (it != _currentCollections.end()) {
    // look up collection by id
    DatabaseCollectionsCurrent::const_iterator it2 = (*it).second.find(collectionID);

    if (it2 != (*it).second.end()) {
      return (*it2).second;
    }
  }

  return std::make_shared<CollectionInfoCurrent>(0);
}

RebootTracker& ClusterInfo::rebootTracker() noexcept { return _rebootTracker; }

RebootTracker const& ClusterInfo::rebootTracker() const noexcept {
  return _rebootTracker;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief ask about a view
/// If it is not found in the cache, the cache is reloaded once. The second
/// argument can be a view ID or a view name (both cluster-wide).
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LogicalView> ClusterInfo::getView(DatabaseID const& databaseID,
                                                  ViewID const& viewID) {
  if (viewID.empty()) {
    return nullptr;
  }

  auto lookupView = [](AllViews const& dbs, DatabaseID const& databaseID,
                       ViewID const& viewID) noexcept -> std::shared_ptr<LogicalView> {
    // look up database by id
    auto const db = dbs.find(databaseID);

    if (db != dbs.end()) {
      // look up view by id (or by name)
      auto& views = db->second;
      auto const view = views.find(viewID);

      if (view != views.end()) {
        return view->second;
      }
    }

    return nullptr;
  };

  if (std::this_thread::get_id() == _planLoader) {
    // we're loading plan, lookup inside immediately created planned views
    // already protected by _planProt.mutex, don't need to lock there
    return lookupView(_newPlannedViews, databaseID, viewID);
  }

  if (!_planProt.isValid) {
    return nullptr;
  }

  {
    READ_LOCKER(readLocker, _planProt.lock);
    auto const view = lookupView(_plannedViews, databaseID, viewID);

    if (view) {
      return view;
    }
  }

  Result res = fetchAndWaitForPlanVersion(std::chrono::seconds(10)).get();
  if (res.ok()) {
    READ_LOCKER(readLocker, _planProt.lock);
    auto const view = lookupView(_plannedViews, databaseID, viewID);

    if (view) {
      return view;
    }
  }

  LOG_TOPIC("a227e", DEBUG, Logger::CLUSTER)
      << "View not found: '" << viewID << "' in database '" << databaseID << "'";

  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief ask about all views of a database
//////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalView>> const ClusterInfo::getViews(DatabaseID const& databaseID) {
  std::vector<std::shared_ptr<LogicalView>> result;

  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  AllViews::const_iterator it = _plannedViews.find(databaseID);

  if (it == _plannedViews.end()) {
    return result;
  }

  // iterate over all collections
  DatabaseViews::const_iterator it2 = (*it).second.begin();
  while (it2 != it->second.end()) {
    char c = it2->first[0];
    if (c >= '0' && c <= '9') {
      // skip views indexed by name
      result.emplace_back(it2->second);
    }
    ++it2;
  }

  return result;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief ask about analyzers revision
//////////////////////////////////////////////////////////////////////////////

AnalyzersRevision::Ptr ClusterInfo::getAnalyzersRevision(DatabaseID const& databaseID,
                                                         bool forceLoadPlan /* = false */) {
  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  auto it = _dbAnalyzersRevision.find(databaseID);

  if (it != _dbAnalyzersRevision.cend()) {
    return it->second;
  }
  return nullptr;
}

QueryAnalyzerRevisions ClusterInfo::getQueryAnalyzersRevision(
    DatabaseID const& databaseID) {
  if (!_planProt.isValid) {
    loadPlan();
  }
  AnalyzersRevision::Revision currentDbRevision{ AnalyzersRevision::MIN };
  AnalyzersRevision::Revision systemDbRevision{ AnalyzersRevision::MIN };
  // no looping here. As if cluster is freshly updated some databases will
  // never have revisions record (and they do not need one actually)
  // so waiting them to apper is futile. Anyway if database has revision
  // we will see it at best effort basis as soon as plan updates itself
  // and some lag in revision appearing is expected (even with looping )
  {
    READ_LOCKER(readLocker, _planProt.lock);
    // look up database by id
    auto it = _dbAnalyzersRevision.find(databaseID);
    if (it != _dbAnalyzersRevision.cend()) {
      currentDbRevision = it->second->getRevision();
    }
    // analyzers from system also available
    // so grab revision for system database as well
    if (databaseID != StaticStrings::SystemDatabase) {
      auto sysIt = _dbAnalyzersRevision.find(StaticStrings::SystemDatabase);
      // if we have non-system database in plan system should be here for sure!
      // but for freshly updated cluster this is not true so, check is necessary
      if (sysIt != _dbAnalyzersRevision.cend()) {
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
Result ClusterInfo::getShardStatisticsForDatabase(DatabaseID const& dbName,
                                                  std::string const& restrictServer,
                                                  VPackBuilder& builder) const {

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections/" + dbName)});

  velocypack::Slice databaseSlice =
    acb->slice()[0].get(std::vector<std::string>(
                          {AgencyCommHelper::path(), "Plan", "Collections", dbName}));

  if (!databaseSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::unordered_set<std::string> servers;
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
      (!serverExists(restrictServer) || !ClusterHelpers::isDBServerName(restrictServer))) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid DBserver id");
  }

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections")});

  velocypack::Slice databasesSlice =
    acb->slice()[0].get(std::vector<std::string>(
                          {AgencyCommHelper::path(), "Plan", "Collections"}));
  
  if (!databasesSlice.isObject()) {
    return Result(TRI_ERROR_INTERNAL, "invalid Plan structure");
  }

  std::unordered_set<std::string> servers;
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
Result ClusterInfo::getShardStatisticsGlobalDetailed(std::string const& restrictServer,
                                                     VPackBuilder& builder) const {
  if (!restrictServer.empty() &&
      (!serverExists(restrictServer) || !ClusterHelpers::isDBServerName(restrictServer))) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid DBserver id");
  }

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections")});

  velocypack::Slice databasesSlice =
    acb->slice()[0].get(std::vector<std::string>(
                          {AgencyCommHelper::path(), "Plan", "Collections"}));
  
  if (!databasesSlice.isObject()) {
    return Result(TRI_ERROR_INTERNAL, "invalid Plan structure");
  }

  std::unordered_set<std::string> servers;
 
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
Result ClusterInfo::getShardStatisticsGlobalByServer(VPackBuilder& builder) const {
  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections")});

  velocypack::Slice databasesSlice =
    acb->slice()[0].get(std::vector<std::string>(
                          {AgencyCommHelper::path(), "Plan", "Collections"}));
  
  if (!databasesSlice.isObject()) {
    return Result(TRI_ERROR_INTERNAL, "invalid Plan structure");
  }
  
  std::unordered_map<ServerID, ShardStatistics> stats;
  {
    // create empty static objects for each DB server
    READ_LOCKER(readLocker, _DBServersProt.lock);
    for (auto& it : _DBServers) {
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

  auto DBServers = std::make_shared<std::vector<ServerID>>(getCurrentDBServers());
  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    size_t numDbServers = DBServers->size();
    if (result.isObject() && result.length() >= numDbServers) {
      // We use >= here since the number of DBservers could have increased
      // during the creation of the database and we might not yet have
      // the latest list. Thus there could be more reports than we know
      // servers.
      VPackObjectIterator dbs(result);

      std::string tmpMsg = "";
      bool tmpHaveError = false;

      for (VPackObjectIterator::ObjectPair dbserver : dbs) {
        VPackSlice slice = dbserver.value;
        if (arangodb::basics::VelocyPackHelper::getBooleanValue(slice, StaticStrings::Error, false)) {
          tmpHaveError = true;
          tmpMsg += " DBServer:" + dbserver.key.copyString() + ":";
          tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(slice, StaticStrings::ErrorMessage,
                                                                       "");
          if (slice.hasKey(StaticStrings::ErrorNum)) {
            VPackSlice errorNum = slice.get(StaticStrings::ErrorNum);
            if (errorNum.isNumber()) {
              tmpMsg += " (errorNum=";
              tmpMsg += basics::StringUtils::itoa(errorNum.getNumericValue<uint32_t>());
              tmpMsg += ")";
            }
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
  auto agencyCallback =
    std::make_shared<AgencyCallback>(
      _server, "Current/Databases/" + database.getName(), dbServerChanged, true, false);
  Result r =_agencyCallbackRegistry->registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }
  auto cbGuard = scopeGuard(
    [&] { _agencyCallbackRegistry->unregisterCallback(agencyCallback); });


  // TODO: Should this never timeout?
  AgencyComm ac(_server);
  auto res = ac.sendTransactionWithFailover(trx, 0.0);
  if (!res.successful()) {
    if (res._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("duplicate database name '") + database.getName() + "'");
    }
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN);
  }

  if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
    Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
    if (r.fail()) {
      return r;
    }
  }

  // Waits for the database to turn up in Current/Databases
  {
    double const interval = getPollInterval();

    int count = 0;  // this counts, when we have to reload the DBServers
    while (true) {
      if (++count >= static_cast<int>(getReloadServerListTimeout() / interval)) {
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
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(getReloadServerListTimeout() / interval);
      }

      if (_server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

// Start creating a database in a coordinator by entering it into Plan/Databases with,
// status flag `isBuilding`; this makes the database invisible to the outside world.
Result ClusterInfo::createIsBuildingDatabaseCoordinator(CreateDatabaseInfo const& database) {
  AgencyCommResult res;

  // Instruct the Agency to enter the creation of the new database
  // by entering it into Plan/Databases/ but with the fields
  // isBuilding: true, and coordinator and rebootId set to our respective
  // id and rebootId
  VPackBuilder builder;
  buildIsBuildingSlice(database, builder);

  AgencyWriteTransaction trx(
    { AgencyOperation(
        "Plan/Databases/" + database.getName(), AgencyValueOperationType::SET, builder.slice()),
      AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP)},
    { AgencyPrecondition(
        "Plan/Databases/" + database.getName(), AgencyPrecondition::Type::EMPTY, true),
      AgencyPrecondition(
        analyzersPath(database.getName()), AgencyPrecondition::Type::EMPTY, true)});

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

// Finalize creation of database in cluster by removing isBuilding, coordinator, and coordinatorRebootId;
// as precondition that the entry we put in createIsBuildingDatabaseCoordinator is still in Plan/ unchanged.
Result ClusterInfo::createFinalizeDatabaseCoordinator(CreateDatabaseInfo const& database) {
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
       AgencyOperation(analyzersPath(database.getName()), AgencyValueOperationType::SET, analyzersBuilder.slice())},
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
  if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
    r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
  }

  // The transaction was successful and the database should
  // now be visible and usable.
  return r;
}

// This function can only return on success or when the cluster is shutting down.
Result ClusterInfo::cancelCreateDatabaseCoordinator(CreateDatabaseInfo const& database) {
  AgencyComm ac(_server);

  VPackBuilder builder;
  buildIsBuildingSlice(database, builder);

  // delete all collections and the database itself from the agency plan
  AgencyOperation delPlanCollections("Plan/Collections/" + database.getName(),
                                     AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanDatabase("Plan/Databases/" + database.getName(),
                                  AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrPlan("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition preCondition("Plan/Databases/" + database.getName(),
                                  AgencyPrecondition::Type::VALUE, builder.slice());

  AgencyWriteTransaction trx({delPlanCollections, delPlanDatabase, incrPlan}, preCondition);

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
      auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
      auto [acb, index] = agencyCache.read(
        std::vector<std::string>{
          AgencyCommHelper::path("Plan/Databases/" + database.getName())});

      velocypack::Slice databaseSlice = acb->slice()[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Plan", "Databases", database.getName()}));

      if (!databaseSlice.isObject()) {
        // database key in agency does _not_ exist. this can happen if on another
        // coordinator the database gets dropped while on this coordinator we are
        // still trying to create it
        break;
      }

      VPackSlice agencyId = databaseSlice.get("id");
      VPackSlice preconditionId = builder.slice().get("id");
      if (agencyId.isString() && preconditionId.isString() &&
          !agencyId.isEqualString(preconditionId.stringRef())) {
        // database key is there, but has a different id, this can happen if the
        // database has already been dropped in the meantime and recreated, in
        // any case, let's get us out of here...
        break;
      }
    }

    if (tries == 1) {
      events::CreateDatabase(database.getName(), res.asResult(), ExecContext::current());
    }
    
    if (_server.isStopping()) {
      return Result(TRI_ERROR_SHUTTING_DOWN);
    }

    if (tries >= 5) {
      nextTimeout = 5.0;
    }
      
    LOG_TOPIC("b47aa", WARN, arangodb::Logger::CLUSTER)
      << "failed to cancel creation of database " << database.getName() << " with error "
      << res.errorMessage() << ". Retrying.";

    // enhancing our calm a bit here, so this does not put the agency under pressure
    // too much.
    TRI_ASSERT(nextTimeout > 0.0 && nextTimeout <= 5.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(unsigned(1000.0 * nextTimeout)));
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

  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
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
  auto agencyCallback =
      std::make_shared<AgencyCallback>(_server, where, dbServerChanged, true, false);
  Result r = _agencyCallbackRegistry->registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }

  auto cbGuard = scopeGuard([this, &agencyCallback]() -> void {
                              _agencyCallbackRegistry->unregisterCallback(agencyCallback);
  });

  // Transact to agency
  AgencyOperation delPlanDatabases("Plan/Databases/" + name,
                                   AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanCollections("Plan/Collections/" + name,
                                     AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanViews("Plan/Views/" + name, AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanAnalyzers(analyzersPath(name), AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition databaseExists("Plan/Databases/" + name,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans({delPlanDatabases, delPlanCollections, delPlanViews,
                                delPlanAnalyzers, incrementVersion}, databaseExists);
  AgencyCommResult res = ac.sendTransactionWithFailover(trans);
  if (!res.successful()) {
    if (res._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN);
  }
  if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
    Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
    if (r.fail()) {
      return r;
    }
  }

  // Now wait stuff in Current to disappear and thus be complete:
  {
    while (true) {
      if (dbServerResult->load(std::memory_order_acquire).has_value()) {
        cbGuard.fire();  // unregister cb before calling ac.removeValues(...)
        AgencyOperation delCurrentCollection(where, AgencySimpleOperationType::DELETE_OP);
        AgencyOperation incrementCurrentVersion(
          "Current/Version", AgencySimpleOperationType::INCREMENT_OP);
        AgencyWriteTransaction cx({delCurrentCollection, incrementCurrentVersion});
        res = ac.sendTransactionWithFailover(cx);
        if (res.successful()) {
          return Result(TRI_ERROR_NO_ERROR);
        }
        return Result(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT);
      }

      if (TRI_microtime() > endTime) {
        logAgencyDump();
        return Result(TRI_ERROR_CLUSTER_TIMEOUT);
      }

      {
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(interval);
      }

      if (_server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::createCollectionCoordinator(  // create collection
    std::string const& databaseName, std::string const& collectionID, uint64_t numberOfShards,
    uint64_t replicationFactor, uint64_t writeConcern, bool waitForReplication,
    velocypack::Slice const& json,  // collection definition
    double timeout,                 // request timeout,
    bool isNewDatabase, std::shared_ptr<LogicalCollection> const& colToDistributeShardsLike) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  auto serverState = ServerState::instance();
  std::vector<ClusterCollectionCreationInfo> infos{ClusterCollectionCreationInfo{
      collectionID, numberOfShards, replicationFactor, writeConcern, waitForReplication,
      json, serverState->getId(), serverState->getRebootId()}};
  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  return createCollectionsCoordinator(databaseName, infos, endTime,
                                      isNewDatabase, colToDistributeShardsLike);
}

/// @brief this method does an atomic check of the preconditions for the
/// collections to be created, using the currently loaded plan. it populates the
/// plan version used for the checks
Result ClusterInfo::checkCollectionPreconditions(std::string const& databaseName,
                                                 std::vector<ClusterCollectionCreationInfo> const& infos,
                                                 uint64_t& planVersion) {
  for (auto const& info : infos) {
    // Check if name exists.
    if (info.name.empty() || !info.json.isObject() || !info.json.get("shards").isObject()) {
      return TRI_ERROR_BAD_PARAMETER;  // must not be empty
    }

    // Validate that the collection does not exist in the current plan
    {
      AllCollections::const_iterator it = _plannedCollections.find(databaseName);
      if (it != _plannedCollections.end()) {
        DatabaseCollections::const_iterator it2 = (*it).second->find(info.name);

        if (it2 != (*it).second->end()) {
          // collection already exists!
          events::CreateCollection(databaseName, info.name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
          return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("duplicate collection name '") + info.name + "'");
        }
      } else {
        // no collection in plan for this particular database... this may be true for
        // the first collection created in a db
        // now check if there is a planned database at least
        if (_plannedDatabases.find(databaseName) == _plannedDatabases.end()) {
          // no need to create a collection in a database that is not there (anymore)
          events::CreateCollection(databaseName, info.name, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
          return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
        }
      }
    }

    // Validate that there is no view with this name either
    {
      // check against planned views as well
      AllViews::const_iterator it = _plannedViews.find(databaseName);
      if (it != _plannedViews.end()) {
        DatabaseViews::const_iterator it2 = (*it).second.find(info.name);

        if (it2 != (*it).second.end()) {
          // view already exists!
          events::CreateCollection(databaseName, info.name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
          return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("duplicate collection name '") + info.name + "'");
        }
      }
    }
  }

  return {};
}

Result ClusterInfo::createCollectionsCoordinator(
    std::string const& databaseName, std::vector<ClusterCollectionCreationInfo>& infos,
    double endTime, bool isNewDatabase,
    std::shared_ptr<LogicalCollection> const& colToDistributeShardsLike) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  using arangodb::velocypack::Slice;

  LOG_TOPIC("98761", DEBUG, Logger::CLUSTER)
      << "Starting createCollectionsCoordinator for " << infos.size()
      << " collections in database " << databaseName << " isNewDatabase: " << isNewDatabase
      << " first collection name: " << infos[0].name;

  double const interval = getPollInterval();

  // The following three are used for synchronization between the callback
  // closure and the main thread executing this function. Note that it can
  // happen that the callback is called only after we return from this
  // function!
  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  auto nrDone = std::make_shared<std::atomic<size_t>>(0);
  auto errMsg = std::make_shared<std::string>();
  auto cacheMutex = std::make_shared<Mutex>();
  auto cacheMutexOwner = std::make_shared<std::atomic<std::thread::id>>();
  auto isCleaned = std::make_shared<bool>(false);

  AgencyComm ac(_server);
  std::vector<std::shared_ptr<AgencyCallback>> agencyCallbacks;

  auto cbGuard = scopeGuard([&] {
    // We have a subtle race here, that we try to cover against:
    // We register a callback in the agency.
    // For some reason this scopeguard is executed (e.g. error case)
    // While we are in this cleanup, and before a callback is removed from the
    // agency. The callback is triggered by another thread. We have the
    // following guarantees: a) cacheMutex|Owner are valid and locked by cleanup
    // b) isCleaned is valid and now set to true
    // c) the closure is owned by the callback
    // d) info might be deleted, so we cannot use it.
    // e) If the callback is ongoing during cleanup, the callback will
    //    hold the Mutex and delay the cleanup.
    RECURSIVE_MUTEX_LOCKER(*cacheMutex, *cacheMutexOwner);
    *isCleaned = true;
    for (auto& cb : agencyCallbacks) {
      _agencyCallbackRegistry->unregisterCallback(cb);
    }
  });

  std::vector<AgencyOperation> opers({IncreaseVersion()});
  std::vector<AgencyPrecondition> precs;
  std::unordered_set<std::string> conditions;
  std::unordered_set<ServerID> allServers;

  // current thread owning 'cacheMutex' write lock (workaround for non-recursive Mutex)
  for (auto& info : infos) {
    TRI_ASSERT(!info.name.empty());

    if (info.state == ClusterCollectionCreationState::DONE) {
      // This is possible in Enterprise / Smart Collection situation
      (*nrDone)++;
    }

    std::map<ShardID, std::vector<ServerID>> shardServers;
    for (auto pair : VPackObjectIterator(info.json.get("shards"))) {
      ShardID shardID = pair.key.copyString();
      std::vector<ServerID> serverIds;

      for (auto const& serv : VPackArrayIterator(pair.value)) {
        auto const sid = serv.copyString();
        serverIds.emplace_back(sid);
        allServers.emplace(sid);
      }
      shardServers.try_emplace(shardID, serverIds);
    }

    // The AgencyCallback will copy the closure will take responsibilty of it.
    auto closure = [cacheMutex, cacheMutexOwner, &info, dbServerResult, errMsg,
                    nrDone, isCleaned, shardServers, this](VPackSlice const& result) {
      // NOTE: This ordering here is important to cover against a race in cleanup.
      // a) The Guard get's the Mutex, sets isCleaned == true, then removes the callback
      // b) If the callback is acquired it is saved in a shared_ptr, the Mutex will be acquired first, then it will check if it isCleaned
      RECURSIVE_MUTEX_LOCKER(*cacheMutex, *cacheMutexOwner);
      if (*isCleaned) {
        return true;
      }
      TRI_ASSERT(!info.name.empty());
      if (info.state != ClusterCollectionCreationState::INIT) {
        // All leaders have reported either good or bad
        // We might be called by followers if they get in sync fast enough
        // In this IF we are in the followers case, we can safely ignore
        return true;
      }

      // result is the object at the path
      if (result.isObject() && result.length() == (size_t)info.numberOfShards) {
        std::string tmpError = "";

        for (auto const& p : VPackObjectIterator(result)) {
          if (arangodb::basics::VelocyPackHelper::getBooleanValue(p.value,
                                                                  StaticStrings::Error, false)) {
            tmpError += " shardID:" + p.key.copyString() + ":";
            tmpError += arangodb::basics::VelocyPackHelper::getStringValue(
                p.value, StaticStrings::ErrorMessage, "");
            if (p.value.hasKey(StaticStrings::ErrorNum)) {
              VPackSlice const errorNum = p.value.get(StaticStrings::ErrorNum);
              if (errorNum.isNumber()) {
                tmpError += " (errNum=";
                tmpError +=
                    basics::StringUtils::itoa(errorNum.getNumericValue<uint32_t>());
                tmpError += ")";
              }
            }
          }

          // wait that all followers have created our new collection
          if (tmpError.empty() && info.waitForReplication) {
            std::vector<ServerID> plannedServers;
            {
              READ_LOCKER(readLocker, _planProt.lock);
              auto it = shardServers.find(p.key.copyString());
              if (it != shardServers.end()) {
                plannedServers = (*it).second;
              } else {
                LOG_TOPIC("9ed54", ERR, Logger::CLUSTER)
                    << "Did not find shard in _shardServers: " << p.key.copyString()
                    << ". Maybe the collection is already dropped.";
                *errMsg = "Error in creation of collection: " + p.key.copyString() +
                          ". Collection already dropped. " + __FILE__ + ":" +
                          std::to_string(__LINE__);
                dbServerResult->store(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION,
                                      std::memory_order_release);
                TRI_ASSERT(info.state != ClusterCollectionCreationState::DONE);
                info.state = ClusterCollectionCreationState::FAILED;
                return true;
              }
            }
            if (plannedServers.empty()) {
              READ_LOCKER(readLocker, _planProt.lock);
              LOG_TOPIC("a0a76", DEBUG, Logger::CLUSTER)
                  << "This should never have happened, Plan empty. Dumping "
                     "_shards in Plan:";
              for (auto const& p : _shards) {
                LOG_TOPIC("60c7d", DEBUG, Logger::CLUSTER) << "Shard: " << p.first;
                for (auto const& q : *(p.second)) {
                  LOG_TOPIC("c7363", DEBUG, Logger::CLUSTER) << "  Server: " << q;
                }
              }
              TRI_ASSERT(false);
            }
            std::vector<ServerID> currentServers;
            VPackSlice servers = p.value.get("servers");
            if (!servers.isArray()) {
              return true;
            }
            for (auto const& server : VPackArrayIterator(servers)) {
              if (!server.isString()) {
                return true;
              }
              currentServers.push_back(server.copyString());
            }
            if (!ClusterHelpers::compareServerLists(plannedServers, currentServers)) {
              TRI_ASSERT(!info.name.empty());
              LOG_TOPIC("16623", DEBUG, Logger::CLUSTER)
                  << "Still waiting for all servers to ACK creation of " << info.name
                  << ". Planned: " << plannedServers << ", Current: " << currentServers;
              return true;
            }
          }
        }
        if (!tmpError.empty()) {
          *errMsg = "Error in creation of collection:" + tmpError + " " +
                    __FILE__ + std::to_string(__LINE__);
          dbServerResult->store(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION,
                                std::memory_order_release);
          // We cannot get into bad state after a collection was created
          TRI_ASSERT(info.state != ClusterCollectionCreationState::DONE);
          info.state = ClusterCollectionCreationState::FAILED;
        } else {
          // We can have multiple calls to this callback, one per leader and one per follower
          // As soon as all leaders are done we are either FAILED or DONE, this cannot be altered later.
          TRI_ASSERT(info.state != ClusterCollectionCreationState::FAILED);
          info.state = ClusterCollectionCreationState::DONE;
          (*nrDone)++;
        }
      }
      return true;
    };
    // ATTENTION: The following callback calls the above closure in a
    // different thread. Nevertheless, the closure accesses some of our
    // local variables. Therefore we have to protect all accesses to them
    // by a mutex. We use the mutex of the condition variable in the
    // AgencyCallback for this.

    auto agencyCallback =
        std::make_shared<AgencyCallback>(
          _server, "Current/Collections/" + databaseName + "/" + info.collectionID,
          closure, true, false);

    Result r = _agencyCallbackRegistry->registerCallback(agencyCallback);
    if (r.fail()) {
      return r;
    }

    agencyCallbacks.emplace_back(std::move(agencyCallback));
    opers.emplace_back(CreateCollectionOrder(databaseName, info.collectionID,
                                             info.isBuildingSlice()));

    // Ensure preconditions on the agency
    std::shared_ptr<ShardMap> otherCidShardMap = nullptr;
    auto const otherCidString =
        basics::VelocyPackHelper::getStringValue(info.json, StaticStrings::DistributeShardsLike,
                                                 StaticStrings::Empty);
    if (!otherCidString.empty() && conditions.find(otherCidString) == conditions.end()) {
      // Distribute shards like case.
      // Precondition: Master collection is not moving while we create this
      // collection We only need to add these once for every Master, we cannot
      // add multiple because we will end up with duplicate entries.
      // NOTE: We do not need to add all collections created here, as they will not succeed
      // In callbacks if they are moved during creation.
      // If they are moved after creation was reported success they are under protection by Supervision.
      conditions.emplace(otherCidString);
      if (colToDistributeShardsLike != nullptr) {
        otherCidShardMap = colToDistributeShardsLike->shardIds();
      } else {
        otherCidShardMap = getCollection(databaseName, otherCidString)->shardIds();
      }

      auto const dslProtoColPath =
          paths::root()->arango()->plan()->collections()->database(databaseName)->collection(otherCidString);
      // The distributeShardsLike prototype collection should exist in the plan...
      precs.emplace_back(AgencyPrecondition(dslProtoColPath,
                                            AgencyPrecondition::Type::EMPTY, false));
      // ...and should not still be in creation.
      precs.emplace_back(AgencyPrecondition(dslProtoColPath->isBuilding(),
                                            AgencyPrecondition::Type::EMPTY, true));

      // Any of the shards locked?
      for (auto const& shard : *otherCidShardMap) {
        precs.emplace_back(AgencyPrecondition("Supervision/Shards/" + shard.first,
                                              AgencyPrecondition::Type::EMPTY, true));
      }
    }

    // additionally ensure that no such collectionID exists yet in Plan/Collections
    precs.emplace_back(AgencyPrecondition("Plan/Collections/" + databaseName + "/" + info.collectionID,
                                          AgencyPrecondition::Type::EMPTY, true));
  }

  // We need to make sure our plan is up to date.
  LOG_TOPIC("f4b14", DEBUG, Logger::CLUSTER)
      << "createCollectionCoordinator, loading Plan from agency...";

  uint64_t planVersion = 0;  // will be populated by following function call
  {
    READ_LOCKER(readLocker, _planProt.lock);
    planVersion = _planVersion;
    if (!isNewDatabase) {
      Result res = checkCollectionPreconditions(databaseName, infos, planVersion);
      if (res.fail()) {
        LOG_TOPIC("98762", DEBUG, Logger::CLUSTER)
            << "Failed createCollectionsCoordinator for " << infos.size()
            << " collections in database " << databaseName << " isNewDatabase: " << isNewDatabase
            << " first collection name: " << ((infos.size() > 0) ? infos[0].name : std::string());
        return res;
      }
    }
  }

  auto deleteCollectionGuard = scopeGuard([&infos, &databaseName, this, &ac]() {
    using namespace arangodb::cluster::paths;
    using namespace arangodb::cluster::paths::aliases;
    // We need to check isBuilding as a precondition.
    // If the transaction removing the isBuilding flag results in a timeout, the
    // state of the collection is unknown; if it was actually removed, we must
    // not drop the collection, but we must otherwise.

    auto precs = std::vector<AgencyPrecondition>{};
    auto opers = std::vector<AgencyOperation>{};

    // Note that we trust here that either all isBuilding flags are removed in
    // a single transaction, or none is.

    for (auto const& info : infos) {
      using namespace std::string_literals;
      auto const collectionPlanPath = "Plan/Collections/"s + databaseName + "/" + info.collectionID;
      precs.emplace_back(collectionPlanPath + "/" + StaticStrings::AttrIsBuilding,
          AgencyPrecondition::Type::EMPTY, false);
      opers.emplace_back(collectionPlanPath, AgencySimpleOperationType::DELETE_OP);
    }
    opers.emplace_back("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
    auto trx = AgencyWriteTransaction{opers, precs};

    using namespace std::chrono;
    using namespace std::chrono_literals;
    auto const begin = steady_clock::now();
    // After a shutdown, the supervision will clean the collections either due
    // to the coordinator going into FAIL, or due to it changing its rebootId.
    // Otherwise we must under no circumstance give up here, because noone else
    // will clean this up.
    while (!_server.isStopping()) {
      auto res = ac.sendTransactionWithFailover(trx);
      // If the collections were removed (res.ok()), we may abort. If we run
      // into precondition failed, the collections were successfully created, so
      // we're fine too.
      if (res.successful()) {
        if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
          [[maybe_unused]] Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
        }
        return;
      } else if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
        return;
      }

      // exponential backoff, just to be safe,
      auto const durationSinceStart = steady_clock::now() - begin;
      auto constexpr maxWaitTime = 2min;
      auto const waitTime =
          std::min<std::common_type_t<decltype(durationSinceStart), decltype(maxWaitTime)>>(
              durationSinceStart, maxWaitTime);
      std::this_thread::sleep_for(waitTime);
    }

  });

  // now try to update the plan in the agency, using the current plan version as
  // our precondition
  {
    // create a builder with just the version number for comparison
    VPackBuilder versionBuilder;
    versionBuilder.add(VPackValue(planVersion));

    VPackBuilder serversBuilder;
    {
      VPackArrayBuilder a(&serversBuilder);
      for (auto const & i : allServers) {
        serversBuilder.add(VPackValue(i));
      }
    }

    // Preconditions:
    // * plan version unchanged
    precs.emplace_back(
      AgencyPrecondition(
        "Plan/Version", AgencyPrecondition::Type::VALUE, versionBuilder.slice()));
    // * not in to be cleaned server list
    precs.emplace_back(
      AgencyPrecondition(
        "Target/ToBeCleanedServers", AgencyPrecondition::Type::INTERSECTION_EMPTY, serversBuilder.slice()));
    // * not in cleaned server list
    precs.emplace_back(
      AgencyPrecondition(
        "Target/CleanedServers", AgencyPrecondition::Type::INTERSECTION_EMPTY, serversBuilder.slice()));

    AgencyWriteTransaction transaction(opers, precs);

    {  // we hold this mutex from now on until we have updated our cache
      // using loadPlan, this is necessary for the callback closure to
      // see the new planned state for this collection. Otherwise it cannot
      // recognize completion of the create collection operation properly:
      RECURSIVE_MUTEX_LOCKER(*cacheMutex, *cacheMutexOwner);
      auto res = ac.sendTransactionWithFailover(transaction);
      // Only if not precondition failed
      if (!res.successful()) {
        if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
          // use this special error code to signal that we got a precondition failure
          // in this case the caller can try again with an updated version of the plan change
          LOG_TOPIC("98763", DEBUG, Logger::CLUSTER)
              << "Failed createCollectionsCoordinator for " << infos.size()
              << " collections in database " << databaseName
              << " isNewDatabase: " << isNewDatabase
              << " first collection name: " << infos[0].name;
          return {TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED,
                  "operation aborted due to precondition failure"};
        }
        auto errorMsg =
            basics::StringUtils::concatT("HTTP code: ", static_cast<int>(res.httpCode()),
                                         " error message: ", res.errorMessage(),
                                         " error details: ", res.errorDetails(),
                                         " body: ", res.body());
        for (auto const& info : infos) {
          events::CreateCollection(databaseName, info.name,
                                   TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN);
        }
        LOG_TOPIC("98767", DEBUG, Logger::CLUSTER)
            << "Failed createCollectionsCoordinator for " << infos.size()
            << " collections in database " << databaseName << " isNewDatabase: " << isNewDatabase
            << " first collection name: " << (infos.size() > 0 ? infos[0].name : std::string());
        return {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN, std::move(errorMsg)};
      }

      if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
        Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
        if (r.fail()) {
          return r;
        }
      }
    }
  }

  TRI_IF_FAILURE("ClusterInfo::createCollectionsCoordinator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  LOG_TOPIC("98bca", DEBUG, Logger::CLUSTER)
      << "createCollectionCoordinator, Plan changed, waiting for success...";

  do {
    auto tmpRes = dbServerResult->load(std::memory_order_acquire);
    if (TRI_microtime() > endTime) {
      for (auto const& info : infos) {
        LOG_TOPIC("f6b57", ERR, Logger::CLUSTER)
            << "Timeout in _create collection"
            << ": database: " << databaseName << ", collId:" << info.collectionID
            << "\njson: " << info.json.toString();
      }

      // Get a full agency dump for debugging
      logAgencyDump();

      if (!tmpRes.has_value() || *tmpRes == TRI_ERROR_NO_ERROR) {
        tmpRes = TRI_ERROR_CLUSTER_TIMEOUT;
      }
    }

    if (nrDone->load(std::memory_order_acquire) == infos.size()) {
      // We do not need to lock all condition variables
      // we are save by cacheMutex
      cbGuard.fire();
      // Now we need to remove the AttrIsBuilding flag and the creator in the Agency
      opers.clear();
      precs.clear();
      opers.push_back(IncreaseVersion());
      for (auto const& info : infos) {
        opers.emplace_back(
            CreateCollectionSuccess(databaseName, info.collectionID, info.json));
        // NOTE:
        // We cannot do anything better than: "noone" has modified our collections while
        // we tried to create them...
        // Preconditions cover against supervision jobs injecting other leaders / followers during failovers.
        // If they are it is not valid to confirm them here. (bad luck we were almost there)
        precs.emplace_back(CreateCollectionOrderPrecondition(databaseName, info.collectionID,
                                                             info.isBuildingSlice()));
      }

      LOG_TOPIC("98bcb", DEBUG, Logger::CLUSTER)
          << "createCollectionCoordinator, collections ok, removing "
             "isBuilding...";

      AgencyWriteTransaction transaction(opers, precs);

      // This is a best effort, in the worst case the collection stays, but will
      // be cleaned out by deleteCollectionGuard respectively the supervision.
      // This removes *all* isBuilding flags from all collections. This is
      // important so that the creation of all collections is atomic, and
      // the deleteCollectionGuard relies on it, too.
      auto res = ac.sendTransactionWithFailover(transaction);

      LOG_TOPIC("98bcc", DEBUG, Logger::CLUSTER)
          << "createCollectionCoordinator, isBuilding removed, waiting for new "
             "Plan...";

      if (res.successful()) {
        // Note that this is not strictly necessary, just to avoid an
        // unneccessary request when we're sure that we don't need it anymore.
        deleteCollectionGuard.cancel();
        if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
          Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
          if (r.fail()) {
            return r;
          }
        }
      }

      // Report if this operation worked, if it failed collections will be
      // cleaned up by deleteCollectionGuard.
      for (auto const& info : infos) {
        TRI_ASSERT(info.state == ClusterCollectionCreationState::DONE);
        events::CreateCollection(databaseName, info.name, res.errorCode());
      }

      LOG_TOPIC("98764", DEBUG, Logger::CLUSTER)
          << "Finished createCollectionsCoordinator for " << infos.size()
          << " collections in database " << databaseName << " isNewDatabase: " << isNewDatabase
          << " first collection name: " << infos[0].name << " result: " << res.errorCode();
      return res.asResult();

    }
    if (tmpRes.has_value() && tmpRes != TRI_ERROR_NO_ERROR) {
      // We do not need to lock all condition variables
      // we are safe by using cacheMutex
      cbGuard.fire();

      // report error
      for (auto const& info : infos) {
        // Report first error.
        // On timeout report it on all not finished ones.
        if (info.state == ClusterCollectionCreationState::FAILED ||
            (tmpRes == TRI_ERROR_CLUSTER_TIMEOUT &&
             info.state == ClusterCollectionCreationState::INIT)) {
          events::CreateCollection(databaseName, info.name, *tmpRes);
        }
      }
      LOG_TOPIC("98765", DEBUG, Logger::CLUSTER)
          << "Failed createCollectionsCoordinator for " << infos.size()
          << " collections in database " << databaseName << " isNewDatabase: " << isNewDatabase
          << " first collection name: " << infos[0].name << " result: " << *tmpRes;
      return {*tmpRes, *errMsg};
    }

    // If we get here we have not tried anything.
    // Wait on callbacks.

    if (_server.isStopping()) {
      // Report shutdown on all collections
      for (auto const& info : infos) {
        events::CreateCollection(databaseName, info.name, TRI_ERROR_SHUTTING_DOWN);
      }
      return TRI_ERROR_SHUTTING_DOWN;
    }

    // Wait for Callbacks to be triggered, it is sufficent to wait for the first non, done
    TRI_ASSERT(agencyCallbacks.size() == infos.size());
    for (size_t i = 0; i < infos.size(); ++i) {
      if (infos[i].state == ClusterCollectionCreationState::INIT) {
        bool gotTimeout;
        {
          // This one has not responded, wait for it.
          CONDITION_LOCKER(locker, agencyCallbacks[i]->_cv);
          gotTimeout = agencyCallbacks[i]->executeByCallbackOrTimeout(interval);
        }
        if (gotTimeout) {
          ++i;
          // We got woken up by waittime, not by  callback.
          // Let us check if we skipped other callbacks as well
          for (; i < infos.size(); ++i) {
            if (infos[i].state == ClusterCollectionCreationState::INIT) {
              agencyCallbacks[i]->refetchAndUpdate(true, false);
            }
          }
        }
        break;
      }
    }

  } while (!_server.isStopping());
  // If we get here we are not allowed to retry.
  // The loop above does not contain a break
  TRI_ASSERT(_server.isStopping());
  for (auto const& info : infos) {
    events::CreateCollection(databaseName, info.name, TRI_ERROR_SHUTTING_DOWN);
  }
  return Result{TRI_ERROR_SHUTTING_DOWN};
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
  if (dbName.empty() || (dbName[0] > '0' && dbName[0] < '9')) {
    events::DropCollection(dbName, collectionID, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  AgencyComm ac(_server);
  AgencyCommResult res;

  // First check that no other collection has a distributeShardsLike
  // entry pointing to us:
  auto coll = getCollection(dbName, collectionID);
  auto colls = getCollections(dbName);  // reloads plan
  std::vector<std::string> clones;
  for (std::shared_ptr<LogicalCollection> const& p : colls) {
    if (p->distributeShardsLike() == coll->name() || p->distributeShardsLike() == collectionID) {
      clones.push_back(p->name());
    }
  }

  if (!clones.empty()) {
    std::string errorMsg("Collection '");
    errorMsg += coll->name();
    errorMsg += "' must not be dropped while '";
    errorMsg += arangodb::basics::StringUtils::join(clones, "', '");
    if (clones.size() == 1) {
      errorMsg += "' has ";
    } else {
      errorMsg += "' have ";
    };
    errorMsg += "distributeShardsLike set to '";
    errorMsg += coll->name();
    errorMsg += "'.";

    events::DropCollection(dbName, collectionID,
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
  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    if (result.isNone() || result.isEmptyObject()) {
      dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
    }
    return true;
  };

  // monitor the entry for the collection
  std::string const where = "Current/Collections/" + dbName + "/" + collectionID;

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback =
      std::make_shared<AgencyCallback>(_server, where, dbServerChanged, true, false);
  Result r = _agencyCallbackRegistry->registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }

  auto cbGuard = scopeGuard(
    [this, &agencyCallback]() -> void {
      _agencyCallbackRegistry->unregisterCallback(agencyCallback);
  });

  size_t numberOfShards = 0;

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path(
        "Plan/Collections/" + dbName + "/" + collectionID + "/shards")});

  velocypack::Slice databaseSlice =
    acb->slice()[0].get(std::vector<std::string>(
                          {AgencyCommHelper::path(), "Plan", "Collections", dbName}));

  if (!databaseSlice.isObject()) {
    // database dropped in the meantime
    events::DropCollection(dbName, collectionID, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  velocypack::Slice collectionSlice = databaseSlice.get(collectionID);
  if (!collectionSlice.isObject()) {
    // collection dropped in the meantime
    events::DropCollection(dbName, collectionID, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  velocypack::Slice shardsSlice = collectionSlice.get("shards");
  if (shardsSlice.isObject()) {
    numberOfShards = shardsSlice.length();
  } else {
    LOG_TOPIC("d340d", ERR, Logger::CLUSTER)
      << "Missing shards information on dropping " << dbName << "/" << collectionID;

    events::DropCollection(dbName, collectionID, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }


  // Transact to agency
  AgencyOperation delPlanCollection("Plan/Collections/" + dbName + "/" + collectionID,
                                    AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition precondition =
      AgencyPrecondition("Plan/Databases/" + dbName, AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans({delPlanCollection, incrementVersion}, precondition);
  res = ac.sendTransactionWithFailover(trans);

  if (!res.successful()) {
    if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      LOG_TOPIC("279c5", ERR, Logger::CLUSTER)
          << "Precondition failed for this agency transaction: " << trans.toJson()
          << ", return code: " << res.httpCode();
    }

    logAgencyDump();

    // TODO: this should rather be TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, as the
    // precondition is that the database still exists
    events::DropCollection(dbName, collectionID, TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION);
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION);
  }
  if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
    Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
    if (r.fail()) {
      return r;
    }
  }

  if (numberOfShards == 0) {
    events::DropCollection(dbName, collectionID, TRI_ERROR_NO_ERROR);
    return Result(TRI_ERROR_NO_ERROR);
  }

  {
    while (true) {
      auto tmpRes = dbServerResult->load();
      if (tmpRes.has_value()) {
        cbGuard.fire();  // unregister cb before calling ac.removeValues(...)
        // ...remove the entire directory for the collection
        AgencyOperation delCurrentCollection("Current/Collections/" + dbName + "/" + collectionID,
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
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(interval);
      }

      if (_server.isStopping()) {
        events::DropCollection(dbName, collectionID, TRI_ERROR_SHUTTING_DOWN);
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

Result ClusterInfo::setCollectionPropertiesCoordinator(std::string const& databaseName,
                                                       std::string const& collectionID,
                                                       LogicalCollection const* info) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  AgencyComm ac(_server);

  AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections/" + databaseName + "/" + collectionID)});

  velocypack::Slice collection = acb->slice()[0].get(std::vector<std::string>(
      {AgencyCommHelper::path(), "Plan", "Collections", databaseName, collectionID}));

  if (!collection.isObject()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  VPackBuilder temp;
  temp.openObject();
  temp.add(StaticStrings::WaitForSyncString, VPackValue(info->waitForSync()));
  temp.add(StaticStrings::ReplicationFactor, VPackValue(info->replicationFactor()));
  temp.add(StaticStrings::MinReplicationFactor, VPackValue(info->writeConcern())); // deprecated in 3.6
  temp.add(StaticStrings::WriteConcern, VPackValue(info->writeConcern()));
  temp.add(StaticStrings::UsesRevisionsAsDocumentIds,
           VPackValue(info->usesRevisionsAsDocumentIds()));
  temp.add(StaticStrings::SyncByRevision, VPackValue(info->syncByRevision()));
  temp.add(VPackValue(StaticStrings::Schema));
  info->schemaToVelocyPack(temp);
  info->getPhysical()->getPropertiesVPack(temp);
  temp.close();

  VPackBuilder builder = VPackCollection::merge(collection, temp.slice(), true);

  AgencyOperation setColl("Plan/Collections/" + databaseName + "/" + collectionID,
                          AgencyValueOperationType::SET, builder.slice());

  AgencyWriteTransaction trans({setColl, incrementVersion}, databaseExists);
  AgencyCommResult res = ac.sendTransactionWithFailover(trans);

  if (res.successful()) {
    Result r;
    if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
      r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
    }
    return r;
  }

  return Result(TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, res.errorMessage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create view in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::createViewCoordinator(  // create view
    std::string const& databaseName,        // database name
    std::string const& viewID,
    velocypack::Slice json  // view definition
) {
  // TRI_ASSERT(ServerState::instance()->isCoordinator());
  // FIXME TODO is this check required?
  auto const typeSlice = json.get(arangodb::StaticStrings::DataSourceType);

  if (!typeSlice.isString()) {
    std::string name;
    if (json.isObject()) {
      name = basics::VelocyPackHelper::getStringValue(json, StaticStrings::DataSourceName,
                                                      "");
    }
    events::CreateView(databaseName, name, TRI_ERROR_BAD_PARAMETER);
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  auto const name =
      basics::VelocyPackHelper::getStringValue(json, arangodb::StaticStrings::DataSourceName,
                                               StaticStrings::Empty);

  if (name.empty()) {
    events::CreateView(databaseName, name, TRI_ERROR_BAD_PARAMETER);
    return Result(TRI_ERROR_BAD_PARAMETER);  // must not be empty
  }

  {
    // check if a view with the same name is already planned
    READ_LOCKER(readLocker, _planProt.lock);
    {
      AllViews::const_iterator it = _plannedViews.find(databaseName);
      if (it != _plannedViews.end()) {
        DatabaseViews::const_iterator it2 = (*it).second.find(name);

        if (it2 != (*it).second.end()) {
          // view already exists!
          events::CreateView(databaseName, name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
          return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("duplicate view name '") + name + "'");
        }
      }
    }
    {
      // check against planned collections as well
      AllCollections::const_iterator it = _plannedCollections.find(databaseName);
      if (it != _plannedCollections.end()) {
        DatabaseCollections::const_iterator it2 = (*it).second->find(name);

        if (it2 != (*it).second->end()) {
          // collection already exists!
          events::CreateCollection(databaseName, name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
          return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("duplicate view name '") + name + "'");
        }
      }
    }
  }

  auto& cache = _server.getFeature<ClusterFeature>().agencyCache();
  if (!cache.has("Plan/Databases/" + databaseName)) {
    events::CreateView(databaseName, name, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (cache.has("Plan/Views/" + databaseName + "/" + viewID)) {
    events::CreateView(databaseName, name, TRI_ERROR_CLUSTER_VIEW_ID_EXISTS);
    return Result(TRI_ERROR_CLUSTER_VIEW_ID_EXISTS);
  }

  AgencyWriteTransaction const transaction{
      // operations
      {{"Plan/Views/" + databaseName + "/" + viewID, AgencyValueOperationType::SET, json},
       {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      // preconditions
      {{"Plan/Views/" + databaseName + "/" + viewID, AgencyPrecondition::Type::EMPTY, true}}};

  AgencyComm ac(_server);
  auto const res = ac.sendTransactionWithFailover(transaction);

  // Only if not precondition failed
  if (!res.successful()) {
    if (res.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      // Dump agency plan:

      logAgencyDump();

      events::CreateView(databaseName, name, TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN);
      return Result(                                        // result
          TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN,  // code
          std::string("Precondition that view ") + name + " with ID " + viewID +
              " does not yet exist failed. Cannot create view.");
    }

    events::CreateView(databaseName, name, TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN);
    return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN,
                  basics::StringUtils::concatT("file: ", __FILE__, " line: ", __LINE__,
                                               " HTTP code: ", static_cast<int>(res.httpCode()),
                                               " error message: ", res.errorMessage(),
                                               " error details: ", res.errorDetails(),
                                               " body: ", res.body()));
  }

  Result r;
  if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
    r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
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
      {{"Plan/Views/" + databaseName + "/" + viewID, AgencySimpleOperationType::DELETE_OP},
       {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      // preconditions
      {{"Plan/Databases/" + databaseName, AgencyPrecondition::Type::EMPTY, false},
       {"Plan/Views/" + databaseName + "/" + viewID, AgencyPrecondition::Type::EMPTY, false}}};

  AgencyComm ac(_server);
  auto const res = ac.sendTransactionWithFailover(trans);
  
  Result result;

  if (res.successful() && res.slice().get("results").length()) {
    result = waitForPlan(res.slice().get("results")[0].getNumber<uint64_t>()).get();
  }

  if (!res.successful() && !result.fail()) {
    if (res.errorCode() == TRI_ERROR_HTTP_PRECONDITION_FAILED) {
      result = Result(                                            // result
          TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,  // FIXME COULD_NOT_REMOVE_VIEW_IN_PLAN
          std::string("Precondition that view  with ID ") + viewID +
              " already exist failed. Cannot create view.");

      // Dump agency plan:
      logAgencyDump();
    } else {
      // FIXME COULD_NOT_REMOVE_VIEW_IN_PLAN
      result = Result(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,
                 basics::StringUtils::concatT("file: ", __FILE__, " line: ", __LINE__,
                                              " HTTP code: ", static_cast<int>(res.httpCode()),
                                              " error message: ", res.errorMessage(),
                                              " error details: ", res.errorDetails(),
                                              " body: ", res.body()));
    }
  }

  events::DropView(databaseName, viewID, result.errorNumber());

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set view properties in coordinator
////////////////////////////////////////////////////////////////////////////////

Result ClusterInfo::setViewPropertiesCoordinator(std::string const& databaseName,
                                                 std::string const& viewID,
                                                 VPackSlice const& json) {
  // TRI_ASSERT(ServerState::instance()->isCoordinator());
  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Views/" + databaseName + "/" + viewID)});

  if (!acb->slice()[0].hasKey(std::vector<std::string>{
        AgencyCommHelper::path(), "Plan", "Views", databaseName, viewID})) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  auto const view = acb->slice()[0].get(std::vector<std::string>{
      AgencyCommHelper::path(), "Plan", "Views", databaseName, viewID});

  if (!view.isObject()) {
    logAgencyDump();

    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  AgencyWriteTransaction const trans{
      // operations
      {{"Plan/Views/" + databaseName + "/" + viewID, AgencyValueOperationType::SET, json},
       {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      // preconditions
      {"Plan/Databases/" + databaseName, AgencyPrecondition::Type::EMPTY, false}};

  AgencyComm ac(_server);
  AgencyCommResult res = ac.sendTransactionWithFailover(trans);

  if (!res.successful()) {
    return {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, res.errorMessage()};
  }

  Result r;
  if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
    r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
  }
  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start creating or deleting an analyzer in coordinator,
/// the return value is an ArangoDB error code
/// and the errorMsg is set accordingly. One possible error
/// is a timeout.
////////////////////////////////////////////////////////////////////////////////
std::pair<Result, AnalyzersRevision::Revision> ClusterInfo::startModifyingAnalyzerCoordinator(
    DatabaseID const& databaseID) {
  VPackBuilder serverIDBuilder;
  serverIDBuilder.add(VPackValue(ServerState::instance()->getId()));

  VPackBuilder rebootIDBuilder;
  rebootIDBuilder.add(VPackValue(ServerState::instance()->getRebootId().value()));

  AgencyComm ac(_server);

  AnalyzersRevision::Revision revision;
  auto const endTime = TRI_microtime() + getTimeout(checkAnalyzersPreconditionTimeout);

  // do until precondition success or timeout
  do {
    {
      // Get current revision for precondition
      READ_LOCKER(readLocker, _planProt.lock);
      auto const it = _dbAnalyzersRevision.find(databaseID);
      if (it == _dbAnalyzersRevision.cend()) {
        if (TRI_microtime() > endTime) {
          return std::make_pair(Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                                       "start modifying analyzer: unknown database name '" + databaseID + "'"),
                                AnalyzersRevision::LATEST);
        }
        // less possible case - we have just updated database so try to write EmptyRevision with preconditions
        {
          VPackBuilder emptyRevision;
          AnalyzersRevision::getEmptyRevision()->toVelocyPack(emptyRevision);
          auto const anPath = analyzersPath(databaseID) + "/";
          AgencyWriteTransaction const transaction{
              {{anPath, AgencyValueOperationType::SET, emptyRevision.slice()},
               {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
              {{anPath, AgencyPrecondition::Type::EMPTY, true}} };
          auto const res = ac.sendTransactionWithFailover(transaction);
          if (res.successful()) {
            auto results = res.slice().get("results");
            if (results.isArray() && results.length() > 0) {
              readLocker.unlock(); // we want to wait for plan to load - release reader
              Result r = waitForPlan(results[0].getNumber<uint64_t>()).get();
              if (r.fail()) {
                return std::make_pair(r, AnalyzersRevision::LATEST);
              }
            }
          }
        }
        continue;
      }
      revision = it->second->getRevision();
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
      if (res.httpCode() == arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
        if (TRI_microtime() > endTime) {
          // Dump agency plan
          logAgencyDump();
          return std::make_pair(Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                                       "start modifying analyzer precondition for database " +
                                        databaseID + ": Revision " + revisionBuilder.toString() +
                                       " is not equal to BuildingRevision. Cannot modify an analyzer."),
                                AnalyzersRevision::LATEST);
        }

        if (_server.isStopping()) {
          return std::make_pair(Result(TRI_ERROR_SHUTTING_DOWN), AnalyzersRevision::LATEST);
        }

        continue;
      }

      return std::make_pair(Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                                   basics::StringUtils::concatT(
                                       "file: ", __FILE__, " line: ", __LINE__,
                                       " HTTP code: ", static_cast<int>(res.httpCode()),
                                       " error message: ", res.errorMessage(),
                                       " error details: ", res.errorDetails(),
                                       " body: ", res.body())),
                            AnalyzersRevision::LATEST);
    } else {
      auto results = res.slice().get("results");
      if (results.isArray() && results.length() > 0) {
        Result r = waitForPlan(results[0].getNumber<uint64_t>()).get();
        if (r.fail()) {
          return std::make_pair(r, AnalyzersRevision::LATEST);
        }
      }
    }
    break;
  } while (true);

  return std::make_pair(Result(TRI_ERROR_NO_ERROR), revision + 1); // as INCREMENT_OP succeeded
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finish creating or deleting an analyzer in coordinator,
/// the return value is an ArangoDB error code
/// and the errorMsg is set accordingly. One possible error
/// is a timeout.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::finishModifyingAnalyzerCoordinator(DatabaseID const& databaseID, bool restore) {
  TRI_IF_FAILURE("FinishModifyingAnalyzerCoordinator") {
    return Result(TRI_ERROR_DEBUG);
  }

  VPackBuilder serverIDBuilder;
  serverIDBuilder.add(VPackValue(ServerState::instance()->getId()));

  VPackBuilder rebootIDBuilder;
  rebootIDBuilder.add(VPackValue(ServerState::instance()->getRebootId().value()));

  AgencyComm ac(_server);

  AnalyzersRevision::Revision revision;
  auto const endTime = TRI_microtime() + getTimeout(checkAnalyzersPreconditionTimeout);

  // do until precondition success or timeout
  do {
    {
      // Get current revision for precondition
      READ_LOCKER(readLocker, _planProt.lock);
      auto const it = _dbAnalyzersRevision.find(databaseID);
      if (it == _dbAnalyzersRevision.cend()) {
        return Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                      "finish modifying analyzer: unknown database name '" + databaseID + "'");
      }
      revision = it->second->getRevision();
    }

    VPackBuilder revisionBuilder;
    revisionBuilder.add(VPackValue(++revision));

    auto const anPath = analyzersPath(databaseID) + "/";
    AgencyWriteTransaction const transaction{
        {
          [restore, &anPath]{
            if (restore) {
              return AgencyOperation{anPath + StaticStrings::AnalyzersBuildingRevision,
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
      // if preconditions failed -> somebody already finished our revision record.
      // That means agency maintanence already reverted our operation - we must abandon this operation.
      // So it differs from what we do in startModifying.
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
        break; // failed precondition means our revert is indirectly successful!
      } 
      return Result(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
                    "finish modifying analyzer precondition for database " +
                    databaseID + ": Revision " + revisionBuilder.toString() +
                    " is not equal to BuildingRevision " +
                    " or " + serverIDBuilder.toString() + " is not equal to coordinator or " +
                    rebootIDBuilder.toString() + " is not equal to coordinatorRebootId. Cannot finish modify "
                    "an analyzer.");
    } else {
      auto results = res.slice().get("results");
      if (results.isArray() && results.length() > 0) {
        Result r = waitForPlan(results[0].getNumber<uint64_t>()).get();
        if (r.fail()) {
          return r;
        }
      }
    }
    break;
  } while (true);

  return Result(TRI_ERROR_NO_ERROR);
}

AnalyzerModificationTransaction::Ptr ClusterInfo::createAnalyzersCleanupTrans() {
  if (AnalyzerModificationTransaction::getPendingCount() == 0) { // rough check, don`t care about sync much
    READ_LOCKER(readLocker, _planProt.lock);
    for (auto& it : _dbAnalyzersRevision) {
      if (it.second->getRebootID() == ServerState::instance()->getRebootId() &&
          it.second->getServerID() == ServerState::instance()->getId() &&
          it.second->getRevision() != it.second->getBuildingRevision()) {
        // this maybe dangling
        if (AnalyzerModificationTransaction::getPendingCount() == 0) { // still nobody active
          return std::make_unique<AnalyzerModificationTransaction>(it.first, this, true);
        } else {
          break;
        }
      }
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection status in coordinator
////////////////////////////////////////////////////////////////////////////////

Result ClusterInfo::setCollectionStatusCoordinator(std::string const& databaseName,
                                                   std::string const& collectionID,
                                                   TRI_vocbase_col_status_e status) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  AgencyComm ac(_server);

  AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                    AgencyPrecondition::Type::EMPTY, false);


  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>{
      AgencyCommHelper::path("Plan/Collections/" + databaseName + "/" + collectionID)});
  std::vector<std::string> vpath(
    {AgencyCommHelper::path(), "Plan", "Collections", databaseName, collectionID});

  if (!acb->slice()[0].hasKey(vpath)) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  VPackSlice col = acb->slice()[0].get(vpath);

  if (!col.isObject()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  TRI_vocbase_col_status_e old = static_cast<TRI_vocbase_col_status_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          col, "status", static_cast<int>(TRI_VOC_COL_STATUS_CORRUPTED)));

  if (old == status) {
    // no status change
    return Result();
  }

  VPackBuilder builder;
  try {
    VPackObjectBuilder b(&builder);
    for (auto const& entry : VPackObjectIterator(col)) {
      std::string key = entry.key.copyString();
      if (key != "status") {
        builder.add(key, entry.value);
      }
    }
    builder.add("status", VPackValue(status));
  } catch (...) {
    return Result(TRI_ERROR_OUT_OF_MEMORY);
  }

  AgencyOperation setColl("Plan/Collections/" + databaseName + "/" + collectionID,
                          AgencyValueOperationType::SET, builder.slice());
  AgencyOperation incVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);

  AgencyWriteTransaction trans({setColl, incVersion}, databaseExists);
  AgencyCommResult res = ac.sendTransactionWithFailover(trans);

  if (res.successful()) {
    Result r;
    if (VPackSlice resultsSlice = res.slice().get("results"); resultsSlice.length() > 0) {
      r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
    }
    return r;
  }

  return Result(TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, res.errorMessage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::ensureIndexCoordinator(LogicalCollection const& collection,
                                           VPackSlice const& slice, bool create,
                                           VPackBuilder& resultBuilder, double timeout) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  // check index id
  IndexId iid = IndexId::none();
  VPackSlice const idSlice = slice.get(StaticStrings::IndexId);

  if (idSlice.isString()) {  // use predefined index id
    iid = IndexId{arangodb::basics::StringUtils::uint64(idSlice.copyString())};
  }

  if (iid.empty()) {  // no id set, create a new one!
    iid = IndexId{uniqid()};
  }

  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());

  VPackSlice const typeSlice = slice.get(StaticStrings::IndexType);
  if (!typeSlice.isString() || (typeSlice.isEqualString("geo1") || typeSlice.isEqualString("geo2"))) {
    // geo1 and geo2 are disallowed here. Only "geo" should be used
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid index type");
  }

  Result res;

  try {
    auto start = std::chrono::steady_clock::now();

    // Keep trying for 2 minutes, if it's preconditions, which are stopping us
    do {
      resultBuilder.clear();
      res = ensureIndexCoordinatorInner(  // create index
          collection, idString, slice, create, resultBuilder, timeout);

      // Note that this function sets the errorMsg unless it is precondition
      // failed, in which case we retry, if this times out, we need to set
      // it ourselves, otherwise all is done!
      if (res.is(TRI_ERROR_HTTP_PRECONDITION_FAILED)) {
        auto diff = std::chrono::steady_clock::now() - start;

        if (diff < std::chrono::seconds(120)) {
          uint32_t wt = RandomGenerator::interval(static_cast<uint32_t>(1000));
          std::this_thread::sleep_for(std::chrono::steady_clock::duration(wt));
          continue;
        }

        res = Result(                                          // result
            TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN,  // code
            res.errorMessage()                                 // message
        );
      }

      break;
    } while (true);
  } catch (basics::Exception const& ex) {
    res = Result(ex.code(), StringUtils::concatT(TRI_errno_string(ex.code()),
                                                 ", exception: ", ex.what()));
  } catch (...) {
    res = Result(TRI_ERROR_INTERNAL);
  }

  // We get here in any case eventually, regardless of whether we have
  //   - succeeded with lookup or index creation
  //   - failed because of a timeout and rollback
  //   - some other error
  // There is nothing more to do here.

  return res;
}

// The following function does the actual work of index creation: Create
// in Plan, watch Current until all dbservers for all shards have done their
// bit. If this goes wrong with a timeout, the creation operation is rolled
// back. If the `create` flag is false, this is actually a lookup operation.
// In any case, no rollback has to happen in the calling function
// ClusterInfo::ensureIndexCoordinator. Note that this method here
// sets the `isBuilding` attribute to `true`, which leads to the fact
// that the index is not yet used by queries. There is code in the
// Agency Supervision which deletes this flag once everything has been
// built successfully. This is a more robust and self-repairing solution
// than if we would take out the `isBuilding` here, since it survives a
// coordinator crash and failover operations.
// Finally note that the retry loop for the case of a failed precondition
// is outside this function here in `ensureIndexCoordinator`.
Result ClusterInfo::ensureIndexCoordinatorInner(LogicalCollection const& collection,
                                                std::string const& idString,
                                                VPackSlice const& slice, bool create,
                                                VPackBuilder& resultBuilder, double timeout) {
  AgencyComm ac(_server);

  using namespace std::chrono;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  TRI_ASSERT(resultBuilder.isEmpty());

  auto type = slice.get(arangodb::StaticStrings::IndexType);
  if (!type.isString()) {
    return Result(TRI_ERROR_INTERNAL,
                  "expecting string value for \"type\" attribute");
  }

  const size_t numberOfShards = collection.numberOfShards();

  // Get the current entry in Plan for this collection
  PlanCollectionReader collectionFromPlan(collection);
  if (!collectionFromPlan.state().ok()) {
    return collectionFromPlan.state();
  }

  auto& engine = _server.getFeature<EngineSelectorFeature>().engine();
  VPackSlice indexes = collectionFromPlan.indexes();
  for (auto const& other : VPackArrayIterator(indexes)) {
    TRI_ASSERT(other.isObject());
    if (true == arangodb::Index::Compare(engine, slice, other, collection.vocbase().name())) {
      {  // found an existing index... Copy over all elements in slice.
        VPackObjectBuilder b(&resultBuilder);
        resultBuilder.add(VPackObjectIterator(other));
        resultBuilder.add("isNewlyCreated", VPackValue(false));
      }
      return Result(TRI_ERROR_NO_ERROR);
    }

    if (true == arangodb::Index::CompareIdentifiers(slice, other)) {
      // found an existing index with a same identifier (i.e. name)
      // but different definition, throw an error
      return Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                    "duplicate value for `" + arangodb::StaticStrings::IndexId +
                        "` or `" + arangodb::StaticStrings::IndexName + "`");
    }
  }

  // no existing index found.
  if (!create) {
    TRI_ASSERT(resultBuilder.isEmpty());
    return Result(TRI_ERROR_NO_ERROR);
  }

  // will contain the error number and message
  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    if (!result.isObject() || result.length() != numberOfShards) {
      return true;
    }

    size_t found = 0;
    for (auto const& shard : VPackObjectIterator(result)) {
      VPackSlice const slice = shard.value;
      if (slice.hasKey("indexes")) {
        VPackSlice const indexes = slice.get("indexes");
        if (!indexes.isArray()) {
          break;  // no list, so our index is not present. we can abort
                  // searching
        }

        for (VPackSlice v : VPackArrayIterator(indexes)) {
          VPackSlice const k = v.get(StaticStrings::IndexId);
          if (!k.isString() || idString != k.copyString()) {
            continue;  // this is not our index
          }

          // check for errors
          if (hasError(v)) {
            // Note that this closure runs with the mutex in the condition
            // variable of the agency callback, which protects writing
            // to *errMsg:
            *errMsg = extractErrorMessage(shard.key.copyString(), v);
            *errMsg = "Error during index creation: " + *errMsg;
            // Returns the specific error number if set, or the general
            // error otherwise
            auto errNum = arangodb::basics::VelocyPackHelper::getNumericValue<ErrorCode>(
                v, StaticStrings::ErrorNum, TRI_ERROR_ARANGO_INDEX_CREATION_FAILED);
            dbServerResult->store(errNum, std::memory_order_release);
            return true;
          }

          found++;  // found our index
          break;
        }
      }
    }

    if (found == static_cast<size_t>(numberOfShards)) {
      dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
    }

    return true;
  };

  VPackBuilder newIndexBuilder;
  {
    VPackObjectBuilder ob(&newIndexBuilder);
    // Add the new index ignoring "id"
    for (auto const& e : VPackObjectIterator(slice)) {
      TRI_ASSERT(e.key.isString());
      std::string const& key = e.key.copyString();
      if (key != StaticStrings::IndexId && key != StaticStrings::IndexIsBuilding) {
        ob->add(e.key);
        ob->add(e.value);
      }
    }
    if (numberOfShards > 0 &&
        !slice.get(StaticStrings::IndexType).isEqualString("arangosearch")) {
      ob->add(StaticStrings::IndexIsBuilding, VPackValue(true));
    }
    ob->add(StaticStrings::IndexId, VPackValue(idString));
  }

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  std::string databaseName = collection.vocbase().name();
  std::string collectionID = std::to_string(collection.id().id());

  std::string where = "Current/Collections/" + databaseName + "/" + collectionID;
  auto agencyCallback =
      std::make_shared<AgencyCallback>(_server, where, dbServerChanged, true, false);

  Result r = _agencyCallbackRegistry->registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }

  auto cbGuard = scopeGuard(
    [&] { _agencyCallbackRegistry->unregisterCallback(agencyCallback); });

  std::string const planCollKey = "Plan/Collections/" + databaseName + "/" + collectionID;
  std::string const planIndexesKey = planCollKey + "/indexes";
  AgencyOperation newValue(planIndexesKey, AgencyValueOperationType::PUSH,
                           newIndexBuilder.slice());
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);

  AgencyPrecondition oldValue(planCollKey, AgencyPrecondition::Type::VALUE,
                              collectionFromPlan.slice());
  AgencyWriteTransaction trx({newValue, incrementVersion}, oldValue);

  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (result.successful()) {
    if (VPackSlice resultsSlice = result.slice().get("results"); resultsSlice.length() > 0) {
      Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
      if (r.fail()) {
        return r;
      }
    }
  }

  // This object watches whether the collection is still present in Plan
  // It assumes that the collection *is* present and only changes state
  // if the collection disappears
  CollectionWatcher collectionWatcher(_agencyCallbackRegistry, collection);

  if (!result.successful()) {
    if (result.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      // Retry loop is outside!
      return Result(TRI_ERROR_HTTP_PRECONDITION_FAILED);
    }

    return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN,
                  basics::StringUtils::concatT(" Failed to execute ", trx.toJson(),
                                               " ResultCode: ", result.errorCode(),
                                               " HttpCode: ",
                                               static_cast<int>(result.httpCode()),
                                               " ", __FILE__, ":", __LINE__));
  }

  // From here on we want to roll back the index creation if we run into
  // the timeout. If this coordinator crashes, the worst that can happen
  // is that the index stays in some state. In most cases, it will converge
  // against the planned state.
  if (numberOfShards == 0) {  // smart "dummy" collection has no shards
    TRI_ASSERT(collection.isSmart());

    {
      // Copy over all elements in slice.
      VPackObjectBuilder b(&resultBuilder);
      resultBuilder.add(StaticStrings::IsSmart, VPackValue(true));
    }

    return Result(TRI_ERROR_NO_ERROR);
  }

  {
    while (!_server.isStopping()) {
      auto tmpRes = dbServerResult->load(std::memory_order_acquire);

      if (!tmpRes.has_value()) {
        // index has not shown up in Current yet,  follow up check to
        // ensure it is still in plan (not dropped between iterations)
        auto& cache = _server.getFeature<ClusterFeature>().agencyCache();
        auto [acb, index] = cache.get(planIndexesKey);
        auto indexes = acb->slice();

        bool found = false;
        if (indexes.isArray()) {
          for (VPackSlice v : VPackArrayIterator(indexes)) {
            VPackSlice const k = v.get(StaticStrings::IndexId);
            if (k.isString() && k.isEqualString(idString)) {
              // index is still here
              found = true;
              break;
            }
          }
        }

        if (!found) {
          return Result(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED,
                        "index was dropped during creation");
        }
      }

      if (tmpRes.has_value() && tmpRes == TRI_ERROR_NO_ERROR) {
        // Finally, in case all is good, remove the `isBuilding` flag
        // check that the index has appeared. Note that we have to have
        // a precondition since the collection could have been deleted
        // in the meantime:
        VPackBuilder finishedPlanIndex;
        {
          VPackObjectBuilder o(&finishedPlanIndex);
          for (auto const& entry : VPackObjectIterator(newIndexBuilder.slice())) {
            auto const key = entry.key.copyString();
            if (key != StaticStrings::IndexIsBuilding &&
                key != "isNewlyCreated") {
              finishedPlanIndex.add(entry.key.copyString(), entry.value);
            }
          }
        }

        AgencyWriteTransaction trx(
            {AgencyOperation(planIndexesKey, AgencyValueOperationType::REPLACE,
                             finishedPlanIndex.slice(), newIndexBuilder.slice()),
             AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP)},
            AgencyPrecondition(planIndexesKey, AgencyPrecondition::Type::EMPTY, false));
        IndexId indexId{arangodb::basics::StringUtils::uint64(
            newIndexBuilder.slice().get("id").copyString())};
        result = _agency.sendTransactionWithFailover(trx, 0.0);
        if (!result.successful()) {
          // We just log the problem and move on, the Supervision will repair
          // things in due course:
          LOG_TOPIC("d9420", INFO, Logger::CLUSTER)
              << "Could not remove isBuilding flag in new index "
              << indexId.id() << ", this will be repaired automatically.";
        } else {
          if (VPackSlice resultsSlice = result.slice().get("results"); resultsSlice.length() > 0) {
            Result r = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
            if (r.fail()) {
              return r;
            }
          }
        }

        if (!collectionWatcher.isPresent()) {
          return Result(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED,
                        "Collection " + collectionID +
                            " has gone from database " + databaseName +
                            ". Aborting index creation");
        }

        {
          // Copy over all elements in slice.
          VPackObjectBuilder b(&resultBuilder);
          resultBuilder.add(VPackObjectIterator(finishedPlanIndex.slice()));
          resultBuilder.add("isNewlyCreated", VPackValue(true));
        }
        CONDITION_LOCKER(locker, agencyCallback->_cv);

        return Result(*tmpRes, *errMsg);
      }

      if ((tmpRes.has_value() && *tmpRes != TRI_ERROR_NO_ERROR) || TRI_microtime() > endTime) {
        // At this time the index creation has failed and we want to
        // roll back the plan entry, provided the collection still exists:
        AgencyWriteTransaction trx(
            std::vector<AgencyOperation>(
                {AgencyOperation(planIndexesKey, AgencyValueOperationType::ERASE,
                                 newIndexBuilder.slice()),
                 AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP)}),
            AgencyPrecondition(planCollKey, AgencyPrecondition::Type::EMPTY, false));

        int sleepFor = 50;
        auto rollbackEndTime = steady_clock::now() + std::chrono::seconds(10);

        while (true) {
          AgencyCommResult update = _agency.sendTransactionWithFailover(trx, 0.0);

          if (update.successful()) {
            if (VPackSlice updateSlice = update.slice().get("results"); updateSlice.length() > 0) {
              Result r = waitForPlan(updateSlice[0].getNumber<uint64_t>()).get();
              if (r.fail()) {
                return r;
              }
            }

            if (!tmpRes.has_value()) {  // timeout
              return Result(
                  TRI_ERROR_CLUSTER_TIMEOUT,
                  "Index could not be created within timeout, giving up and "
                  "rolling back index creation.");
            }

            // The mutex in the condition variable protects the access to
            // *errMsg:
            CONDITION_LOCKER(locker, agencyCallback->_cv);
            return Result(*tmpRes, *errMsg);
          }

          if (update._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
            // Collection was removed, let's break here and report outside
            break;
          }

          if (steady_clock::now() > rollbackEndTime) {
            LOG_TOPIC("db00b", ERR, Logger::CLUSTER)
                << "Couldn't roll back index creation of " << idString
                << ". Database: " << databaseName << ", Collection " << collectionID;

            if (!tmpRes.has_value()) {  // timeout
              return Result(
                  TRI_ERROR_CLUSTER_TIMEOUT,
                  "Timed out while trying to roll back index creation failure");
            }

            // The mutex in the condition variable protects the access to
            // *errMsg:
            CONDITION_LOCKER(locker, agencyCallback->_cv);
            return Result(*tmpRes, *errMsg);
          }

          if (sleepFor <= 2500) {
            sleepFor *= 2;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(sleepFor));
        }
        // We only get here if the collection was dropped just in the moment
        // when we wanted to roll back the index creation.
      }

      if (!collectionWatcher.isPresent()) {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                      "collection " + collectionID +
                          "appears to have been dropped from database " +
                          databaseName + " during ensureIndex");
      }

      {
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(interval);
      }
    }
  }

  return Result(TRI_ERROR_SHUTTING_DOWN);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////
Result ClusterInfo::dropIndexCoordinator( 
    std::string const& databaseName,      
    std::string const& collectionID, IndexId iid,
    double timeout) {
  double const endTime = TRI_microtime() + getTimeout(timeout);
  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());

  Result res(TRI_ERROR_CLUSTER_TIMEOUT);
  do {
    res = dropIndexCoordinatorInner(databaseName, collectionID, iid, endTime);

    if (res.ok()) {
      // success!
      break;
    }

    // check if we got a precondition failed error
    if (!res.is(TRI_ERROR_HTTP_PRECONDITION_FAILED)) {
      // no, different error. report it!
      break;
    }
      
    if (_server.isStopping()) {
      // do not audit-log the error
      return Result(TRI_ERROR_SHUTTING_DOWN);
    }

    // precondition failed

    // apply a random wait time
    uint32_t wt = RandomGenerator::interval(static_cast<uint32_t>(1000));
    std::this_thread::sleep_for(std::chrono::steady_clock::duration(wt));
  } while (TRI_microtime() < endTime);
      
  events::DropIndex(databaseName, collectionID, idString, res.errorNumber());
  return res;
}

Result ClusterInfo::dropIndexCoordinatorInner(
    std::string const& databaseName,      
    std::string const& collectionID, IndexId iid,
    double endTime) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  AgencyComm ac(_server);

  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());
  double const interval = getPollInterval();

  std::string const planCollKey = "Plan/Collections/" + databaseName + "/" + collectionID;
  std::string const planIndexesKey = planCollKey + "/indexes";

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>{AgencyCommHelper::path(planCollKey)});
  auto previous = acb->slice();

  if (!previous.isArray() || previous.length() == 0) {
    return Result(TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
  }
  velocypack::Slice collection = previous[0].get(std::vector<std::string>(
      {AgencyCommHelper::path(), "Plan", "Collections", databaseName, collectionID}));
  if (!collection.isObject()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  TRI_ASSERT(VPackObjectIterator(collection).size() > 0);
  size_t const numberOfShards =
      basics::VelocyPackHelper::getNumericValue<size_t>(collection,
                                                         StaticStrings::NumberOfShards, 1);

  VPackSlice indexes = collection.get("indexes");
  if (!indexes.isArray()) {
    LOG_TOPIC("63178", DEBUG, Logger::CLUSTER)
        << "Failed to find index " << databaseName << "/" << collectionID << "/"
        << iid.id();
    return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  }

  VPackSlice indexToRemove;

  for (VPackSlice indexSlice : VPackArrayIterator(indexes)) {
    auto idSlice = indexSlice.get(arangodb::StaticStrings::IndexId);
    auto typeSlice = indexSlice.get(arangodb::StaticStrings::IndexType);

    if (!idSlice.isString() || !typeSlice.isString()) {
      continue;
    }

    if (idSlice.isEqualString(idString)) {
      Index::IndexType type = Index::type(typeSlice.copyString());

      if (type == Index::TRI_IDX_TYPE_PRIMARY_INDEX || type == Index::TRI_IDX_TYPE_EDGE_INDEX) {
        return Result(TRI_ERROR_FORBIDDEN);
      }

      indexToRemove = indexSlice;

      break;
    }
  }

  if (!indexToRemove.isObject()) {
    LOG_TOPIC("95fe6", DEBUG, Logger::CLUSTER)
        << "Failed to find index " << databaseName << "/" << collectionID << "/"
        << iid.id();
    return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  }

  std::string where = "Current/Collections/" + databaseName + "/" + collectionID;

  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& current) {
    if (numberOfShards == 0) {
      return false;
    }

    if (!current.isObject()) {
      return true;
    }

    VPackObjectIterator shards(current);

    if (shards.size() == numberOfShards) {
      bool found = false;
      for (auto const& shard : shards) {
        VPackSlice const indexes = shard.value.get("indexes");

        if (indexes.isArray()) {
          for (VPackSlice v : VPackArrayIterator(indexes)) {
            if (v.isObject()) {
              VPackSlice const k = v.get(StaticStrings::IndexId);
              if (k.isString() && k.isEqualString(idString)) {
                // still found the index in some shard
                found = true;
                break;
              }
            }

            if (found) {
              break;
            }
          }
        }
      }

      if (!found) {
        dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
      }
    }
    return true;
  };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback =
      std::make_shared<AgencyCallback>(_server, where, dbServerChanged, true, false);
  Result r = _agencyCallbackRegistry->registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
  }

  auto cbGuard = scopeGuard(
    [&] { _agencyCallbackRegistry->unregisterCallback(agencyCallback); });

  AgencyOperation planErase(planIndexesKey, AgencyValueOperationType::ERASE, indexToRemove);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition prec(planCollKey, AgencyPrecondition::Type::VALUE, collection);
  AgencyWriteTransaction trx({planErase, incrementVersion}, prec);
  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (!result.successful()) {
    if (result.httpCode() == arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      // Retry loop is outside!
      return Result(TRI_ERROR_HTTP_PRECONDITION_FAILED);
    }

    return Result(TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN,
                  basics::StringUtils::concatT(" Failed to execute ", trx.toJson(),
                                               " ResultCode: ", result.errorCode()));
  }
  if (VPackSlice resultSlice = result.slice().get("results"); resultSlice.length() > 0) {
    Result r = waitForPlan(resultSlice[0].getNumber<uint64_t>()).get();
    if (r.fail()) {
      return r;
    }
  }

  if (numberOfShards == 0) {  // smart "dummy" collection has no shards
    TRI_ASSERT(collection.get(StaticStrings::IsSmart).getBool());

    return Result(TRI_ERROR_NO_ERROR);
  }

  {
    while (true) {
      auto const tmpRes = dbServerResult->load();
      if (tmpRes.has_value()) {
        cbGuard.fire();  // unregister cb
        events::DropIndex(databaseName, collectionID, idString, *tmpRes);

        return Result(*tmpRes);
      }

      if (TRI_microtime() > endTime) {
        return Result(TRI_ERROR_CLUSTER_TIMEOUT);
      }

      {
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(interval);
      }

      if (_server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixServersRegistered = "Current/ServersRegistered";
static std::string const prefixServersKnown = "Current/ServersKnown";
static std::string const mapUniqueToShortId = "Target/MapUniqueToShortID";

void ClusterInfo::loadServers() {
  ++_serversProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                 // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _serversProt.mutex);
  uint64_t storedVersion = _serversProt.wantedVersion;  // this is the version
  // we will set in the end
  if (_serversProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>({AgencyCommHelper::path(prefixServersRegistered),
                              AgencyCommHelper::path(mapUniqueToShortId),
                              AgencyCommHelper::path(prefixServersKnown)}));
  auto result = acb->slice();
  if (!result.isArray()) {
    LOG_TOPIC("be98b", DEBUG, Logger::CLUSTER)
      << "Failed to load server lists from the agency cache given " << acb->toJson();
    return;
  }

  VPackSlice serversRegistered, serversAliases, serversKnownSlice;

  std::vector<std::string> serversRegisteredPath {
    AgencyCommHelper::path(), "Current", "ServersRegistered"};
  if (result[0].hasKey(serversRegisteredPath)) {
    serversRegistered = result[0].get(serversRegisteredPath);
  }
  std::vector<std::string> mapUniqueToShortIDPath {
    AgencyCommHelper::path(), "Target", "MapUniqueToShortID"};
  if (result[0].hasKey(mapUniqueToShortIDPath)) {
    serversAliases = result[0].get(mapUniqueToShortIDPath);
  }
  std::vector<std::string> serversKnownPath {
    AgencyCommHelper::path(), "Current", "ServersKnown"};
  if (result[0].hasKey(serversKnownPath)) {
    serversKnownSlice = result[0].get(serversKnownPath);
  }

  if (serversRegistered.isObject()) {
    decltype(_servers) newServers;
    decltype(_serverAliases) newAliases;
    decltype(_serverAdvertisedEndpoints) newAdvertisedEndpoints;
    decltype(_serverTimestamps) newTimestamps;

    std::unordered_set<ServerID> serverIds;

    for (auto const& res : VPackObjectIterator(serversRegistered)) {
      velocypack::Slice slice = res.value;

      if (slice.isObject() && slice.hasKey("endpoint")) {
        std::string server =
          arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                             "endpoint", "");
        std::string advertised = arangodb::basics::VelocyPackHelper::getStringValue(
          slice, "advertisedEndpoint", "");

        std::string serverId = res.key.copyString();
        try {
          velocypack::Slice serverSlice;
          serverSlice = serversAliases.get(serverId);
          if (serverSlice.isObject()) {
            std::string alias = arangodb::basics::VelocyPackHelper::getStringValue(
              serverSlice, "ShortName", "");
            newAliases.try_emplace(std::move(alias), serverId);
          }
        } catch (...) {
        }
        std::string serverTimestamp =
          arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                             "timestamp",
                                                             "");
        newServers.try_emplace(serverId, server);
        newAdvertisedEndpoints.try_emplace(serverId, advertised);
        serverIds.emplace(serverId);
        newTimestamps.try_emplace(serverId, serverTimestamp);
      }
    }

    decltype(_serversKnown) newServersKnown(serversKnownSlice, serverIds);

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
    // Our own RebootId might have changed if we have been FAILED at least once
    // since our last actual reboot, let's update it:
    auto rebootIds = _serversKnown.rebootIds();
    auto* serverState = ServerState::instance();
    auto it = rebootIds.find(serverState->getId());
    if (it != rebootIds.end()) {
      // should always be ok
      if (serverState->getRebootId() != it->second) {
        serverState->setRebootId(it->second);
        LOG_TOPIC("feaab", INFO, Logger::CLUSTER)
            << "Updating my own rebootId to " << it->second.value();
      }
    } else {
      LOG_TOPIC("feaaa", WARN, Logger::CLUSTER)
          << "Cannot find my own rebootId in the list of known servers, this "
             "is very strange and should not happen, if this persists, please "
             "report this error!";
    }
    // RebootTracker has its own mutex, and doesn't strictly need to be in
    // sync with the other members.
    rebootTracker().updateServerState(rebootIds);
    return;
  }

  LOG_TOPIC("449e0", DEBUG, Logger::CLUSTER)
    << "Error while loading " << prefixServersRegistered
    << ", result was " << result.toJson();

}

////////////////////////////////////////////////////////////////////////
/// @brief Hand out copy of reboot ids
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<ServerID, RebootId> ClusterInfo::rebootIds() const {
  MUTEX_LOCKER(mutexLocker, _serversProt.mutex);
  return _serversKnown.rebootIds();
}

////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint(ServerID const& serverID) {
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

  std::string serverID_ = serverID;

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);

      // _serversAliases is a map-type <Alias, ServerID>
      auto ita = _serverAliases.find(serverID_);

      if (ita != _serverAliases.end()) {
        serverID_ = (*ita).second;
      }

      // _servers is a map-type <ServerId, std::string>
      auto it = _servers.find(serverID_);

      if (it != _servers.end()) {
        return (*it).second;
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the advertised endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerAdvertisedEndpoint(ServerID const& serverID) {
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

  std::string serverID_ = serverID;

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);

      // _serversAliases is a map-type <Alias, ServerID>
      auto ita = _serverAliases.find(serverID_);

      if (ita != _serverAliases.end()) {
        serverID_ = (*ita).second;
      }

      // _serversAliases is a map-type <ServerID, std::string>
      auto it = _serverAdvertisedEndpoints.find(serverID_);
      if (it != _serverAdvertisedEndpoints.end()) {
        return (*it).second;
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the ID of a server from its endpoint.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerName(std::string const& endpoint) {
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
          return it.first;
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all coordinators from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentCoordinators = "Current/Coordinators";

void ClusterInfo::loadCurrentCoordinators() {
  ++_coordinatorsProt.wantedVersion;  // Indicate that after *NOW* somebody
                                      // has to reread from the agency!
  MUTEX_LOCKER(mutexLocker, _coordinatorsProt.mutex);
  uint64_t storedVersion = _coordinatorsProt.wantedVersion;  // this is the
  // version we will set in the end
  if (_coordinatorsProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>{AgencyCommHelper::path(prefixCurrentCoordinators)});
  auto result = acb->slice();

  if (result.isArray()) {
    velocypack::Slice currentCoordinators = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Current", "Coordinators"}));

    if (currentCoordinators.isObject()) {
      decltype(_coordinators) newCoordinators;

      for (auto const& coordinator : VPackObjectIterator(currentCoordinators)) {
        newCoordinators.try_emplace(coordinator.key.copyString(), coordinator.value.copyString());
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
      << "Error while loading " << prefixCurrentCoordinators
      << " result was " << result.toJson();
}

static std::string const prefixMappings = "Target/MapUniqueToShortID";

void ClusterInfo::loadCurrentMappings() {
  ++_mappingsProt.wantedVersion;  // Indicate that after *NOW* somebody
                                  // has to reread from the agency!
  MUTEX_LOCKER(mutexLocker, _mappingsProt.mutex);
  uint64_t storedVersion = _mappingsProt.wantedVersion;  // this is the
                                                         // version we will
                                                         // set in the end
  if (_mappingsProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(
    std::vector<std::string>{AgencyCommHelper::path(prefixMappings)});
  auto result = acb->slice();

  if (result.isArray()) {

    velocypack::Slice mappings = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Target", "MapUniqueToShortID"}));

    if (mappings.isObject()) {
      decltype(_coordinatorIdMap) newCoordinatorIdMap;

      for (auto const& mapping : VPackObjectIterator(mappings)) {
        auto mapObject = mapping.value;
        if (mapObject.isObject()) {
          ServerID fullId = mapping.key.copyString();
          ServerShortName shortName = mapObject.get("ShortName").copyString();

          ServerShortID shortId =
              mapObject.get("TransactionID").getNumericValue<ServerShortID>();
          static std::string const expectedPrefix{"Coordinator"};
          if (shortName.size() > expectedPrefix.size() &&
              shortName.substr(0, expectedPrefix.size()) == expectedPrefix) {
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
      << "Error while loading " << prefixMappings << " result was " << result.toJson();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentDBServers = "Current/DBServers";
static std::string const prefixTarget = "Target";

void ClusterInfo::loadCurrentDBServers() {
  ++_DBServersProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                   // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _DBServersProt.mutex);
  uint64_t storedVersion = _DBServersProt.wantedVersion;  // this is the version
  // we will set in the end
  if (_DBServersProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read(std::vector<std::string>{
      AgencyCommHelper::path(prefixCurrentDBServers),
        AgencyCommHelper::path(prefixTarget)});
  auto result = acb->slice();
  if (!result.isArray()) {
    return;
  }

  // Now contact the agency:
  VPackSlice currentDBServers, failedDBServers, cleanedDBServers, toBeCleanedDBServers;

  auto curDBServersPath = std::vector<std::string>{
    AgencyCommHelper::path(), "Current", "DBServers"};
  if (result[0].hasKey(curDBServersPath)) {
    currentDBServers = result[0].get(curDBServersPath);
  }
  auto failedServerPath = std::vector<std::string>{
    AgencyCommHelper::path(), "Target", "FailedServers"};
  if (result[0].hasKey(failedServerPath)) {
    failedDBServers = result[0].get(failedServerPath);
  }
  auto cleanedServersPath = std::vector<std::string>{
    AgencyCommHelper::path(), "Target", "CleanedServers"};
  if (result[0].hasKey(cleanedServersPath)) {
    cleanedDBServers = result[0].get(cleanedServersPath);
  }

  auto toBeCleanedServersPath = std::vector<std::string>{
    AgencyCommHelper::path(), "Target", "ToBeCleanedServers"};
  if (result[0].hasKey(toBeCleanedServersPath)) {
    toBeCleanedDBServers = result[0].get(toBeCleanedServersPath);
  }

  if (currentDBServers.isObject() && failedDBServers.isObject()) {
    decltype(_DBServers) newDBServers;

    for (auto const& dbserver : VPackObjectIterator(currentDBServers)) {
      bool found = false;
      if (failedDBServers.isObject()) {
        for (auto const& failedServer : VPackObjectIterator(failedDBServers)) {
          if (basics::VelocyPackHelper::equal(dbserver.key, failedServer.key, false)) {
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
          if (basics::VelocyPackHelper::equal(dbserver.key, cleanedServer, false)) {
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
        for (auto const& toBeCleanedServer : VPackArrayIterator(toBeCleanedDBServers)) {
          if (basics::VelocyPackHelper::equal(dbserver.key, toBeCleanedServer, false)) {
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
      WRITE_LOCKER(writeLocker, _DBServersProt.lock);
      _DBServers.swap(newDBServers);
      _DBServersProt.doneVersion = storedVersion;
      _DBServersProt.isValid = true;
    }
    return;
  }

  LOG_TOPIC("5a7e1", DEBUG, Logger::CLUSTER)
    << "Error while loading " << prefixCurrentDBServers
    << " result was " << result.toJson();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentDBServers() {
  std::vector<ServerID> result;

  if (!_DBServersProt.isValid) {
    loadCurrentDBServers();
  }
  // return a consistent state of servers
  READ_LOCKER(readLocker, _DBServersProt.lock);

  result.reserve(_DBServers.size());

  for (auto& it : _DBServers) {
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

std::shared_ptr<std::vector<ServerID>> ClusterInfo::getResponsibleServer(ShardID const& shardID) {
  int tries = 0;

  if (!_currentProt.isValid) {
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _currentProt.lock);
      // _shardIds is a map-type <ShardId,
      // std::shared_ptr<std::vector<ServerId>>>
      auto it = _shardIds.find(shardID);

      if (it != _shardIds.end()) {
        auto serverList = (*it).second;
        if (serverList != nullptr && serverList->size() > 0 &&
            (*serverList)[0].size() > 0 && (*serverList)[0][0] == '_') {
          // This is a temporary situation in which the leader has already
          // resigned, let's wait half a second and try again.
          --tries;
          LOG_TOPIC("b1dc5", INFO, Logger::CLUSTER)
              << "getResponsibleServer: found resigned leader,"
              << "waiting for half a second...";
        } else {
          return (*it).second;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (++tries >= 2) {
      break;
    }

  }

  return std::make_shared<std::vector<ServerID>>();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief atomically find all servers who are responsible for the given
/// shards (only the leaders).
/// will throw an exception if no leader can be found for any
/// of the shards. will return an empty result if the shards couldn't be
/// determined after a while - it is the responsibility of the caller to
/// check for an empty result!
//////////////////////////////////////////////////////////////////////////////

std::unordered_map<ShardID, ServerID> ClusterInfo::getResponsibleServers(
    std::unordered_set<ShardID> const& shardIds) {
  TRI_ASSERT(!shardIds.empty());

  std::unordered_map<ShardID, ServerID> result;
  int tries = 0;

  if (!_currentProt.isValid) {
    Result r = waitForCurrent(1).get();
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
  }

  while (true) {
    TRI_ASSERT(result.empty());
    {
      READ_LOCKER(readLocker, _currentProt.lock);
      for (auto const& shardId : shardIds) {
        auto it = _shardIds.find(shardId);

        if (it == _shardIds.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                         "no servers found for shard " + shardId);
        }

        auto serverList = (*it).second;
        if (serverList == nullptr || serverList->empty()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "no servers found for shard " + shardId);
        }

        if ((*serverList)[0].size() > 0 && (*serverList)[0][0] == '_') {
          // This is a temporary situation in which the leader has already
          // resigned, let's wait half a second and try again.
          --tries;
          break;
        }

        // put leader into result
        result.try_emplace(shardId, (*it).second->front());
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
        << "getResponsibleServers: found resigned leader,"
        << "waiting for half a second...";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard list of a collection, sorted numerically
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ShardID>> ClusterInfo::getShardList(CollectionID const& collectionID) {
  TRI_IF_FAILURE("ClusterInfo::failedToGetShardList") {
    // Simulate no results
    return std::make_shared<std::vector<ShardID>>();
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
  return std::make_shared<std::vector<ShardID>>();
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

ServerID ClusterInfo::getCoordinatorByShortID(ServerShortID const& shortId) {
  ServerID result;

  if (!_mappingsProt.isValid) {
    loadCurrentMappings();
  }

  // return a consistent state of servers
  READ_LOCKER(readLocker, _mappingsProt.lock);

  auto it = _coordinatorIdMap.find(shortId);
  if (it != _coordinatorIdMap.end()) {
    result = it->second;
  }

  return result;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate current coordinators
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidateCurrentCoordinators() {
  WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
  _coordinatorsProt.isValid = false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate current mappings
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidateCurrentMappings() {
  WRITE_LOCKER(writeLocker, _mappingsProt.lock);
  _mappingsProt.isValid = false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get current "Plan" structure
//////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::shared_ptr<VPackBuilder>>
ClusterInfo::getPlan(uint64_t& index, std::unordered_set<std::string> const& dirty) {

  // We should never proceed here, until we have seen an
  // initial agency cache through loadPlan
  Result r = waitForPlan(1).get();
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }

  std::unordered_map<std::string,std::shared_ptr<VPackBuilder>> ret;
  READ_LOCKER(readLocker, _planProt.lock);
  index = _planIndex;
  for (auto const& i : dirty) {
    auto it = _plan.find(i);
    if (it == _plan.end()) {
      continue;
    }
    ret.try_emplace(it->first, it->second);
  }
  return ret;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get current "Current" structure
//////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string,std::shared_ptr<VPackBuilder>>
ClusterInfo::getCurrent(uint64_t& index, std::unordered_set<std::string> const& dirty) {

  // We should never proceed here, until we have seen an
  // initial agency cache through loadCurrent
  Result r = waitForCurrent(1).get();
  if (r.fail()) {
    THROW_ARANGO_EXCEPTION(r);
  }

  std::unordered_map<std::string,std::shared_ptr<VPackBuilder>> ret;
  READ_LOCKER(readLocker, _currentProt.lock);
  index = _currentIndex;
  for (auto const& i : dirty) {
    auto it = _current.find(i);
    if (it == _current.end()) {
      continue;
    }
    ret.try_emplace(it->first, it->second);
  }
  return ret;
}
  
std::vector<std::string> ClusterInfo::getFailedServers() const {
  MUTEX_LOCKER(guard, _failedServersMutex);
  return _failedServers;
}

void ClusterInfo::setFailedServers(std::vector<std::string> const& failedServers) {
  MUTEX_LOCKER(guard, _failedServersMutex);
  _failedServers = failedServers;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void ClusterInfo::setServers(std::unordered_map<ServerID, std::string> servers) {
  WRITE_LOCKER(readLocker, _serversProt.lock);
  _servers = std::move(servers);
}

void ClusterInfo::setServerAliases(std::unordered_map<ServerID, std::string> aliases) {
  WRITE_LOCKER(readLocker, _serversProt.lock);
  _serverAliases = std::move(aliases);
}
  
void ClusterInfo::setServerAdvertisedEndpoints(std::unordered_map<ServerID, std::string> advertisedEndpoints) {
  WRITE_LOCKER(readLocker, _serversProt.lock);
  _serverAdvertisedEndpoints = std::move(advertisedEndpoints);
}
  
void ClusterInfo::setServerTimestamps(std::unordered_map<ServerID, std::string> timestamps) {
  WRITE_LOCKER(readLocker, _serversProt.lock);
  _serverTimestamps = std::move(timestamps);
}
#endif
  
bool ClusterInfo::serverExists(ServerID const& serverId) const noexcept {
  READ_LOCKER(readLocker, _serversProt.lock);
  return _servers.find(serverId) != _servers.end();
}

bool ClusterInfo::serverAliasExists(std::string const& alias) const noexcept {
  READ_LOCKER(readLocker, _serversProt.lock);
  return _serverAliases.find(alias) != _serverAliases.end();
}

std::unordered_map<ServerID, std::string> ClusterInfo::getServers() {
  if (!_serversProt.isValid) {
    loadServers();
  }
  READ_LOCKER(readLocker, _serversProt.lock);
  return _servers;
}

std::unordered_map<ServerID, std::string> ClusterInfo::getServerAliases() {
  std::unordered_map<std::string, std::string> ret;
  READ_LOCKER(readLocker, _serversProt.lock);
  // note: don't try to change this to
  //  return _serverAlias
  // because we are returning the aliases in {value, key} order here
  for (const auto& i : _serverAliases) {
    ret.try_emplace(i.second, i.first);
  }
  return ret;
}

std::unordered_map<ServerID, std::string> ClusterInfo::getServerAdvertisedEndpoints() const {
  std::unordered_map<std::string, std::string> ret;
  READ_LOCKER(readLocker, _serversProt.lock);
  // note: don't try to change this to
  //  return _serverAdvertisedEndpoints
  // because we are returning the aliases in {value, key} order here
  for (const auto& i : _serverAdvertisedEndpoints) {
    ret.try_emplace(i.second, i.first);
  }
  return ret;
}

std::unordered_map<ServerID, std::string> ClusterInfo::getServerTimestamps() const {
  READ_LOCKER(readLocker, _serversProt.lock);
  return _serverTimestamps;
}

arangodb::Result ClusterInfo::getShardServers(ShardID const& shardId,
                                              std::vector<ServerID>& servers) {
  READ_LOCKER(readLocker, _planProt.lock);

  auto it = _shardServers.find(shardId);
  if (it != _shardServers.end()) {
    servers = (*it).second;
    return arangodb::Result();
  }

  LOG_TOPIC("16d14", DEBUG, Logger::CLUSTER)
      << "Strange, did not find shard in _shardServers: " << shardId;
  return arangodb::Result(TRI_ERROR_FAILED);
}

CollectionID ClusterInfo::getCollectionNameForShard(ShardID const& shardId) {
  READ_LOCKER(readLocker, _planProt.lock);

  auto it = _shardToName.find(shardId);
  if (it != _shardToName.end()) {
    return it->second;
  }
  return StaticStrings::Empty;
}

arangodb::Result ClusterInfo::agencyDump(std::shared_ptr<VPackBuilder> body) {
  AgencyCommResult dump = _agency.dump();

  if (!dump.successful()) {
    LOG_TOPIC("93c0e", ERR, Logger::CLUSTER)
        << "failed to acquire agency dump: " << dump.errorMessage();
    return Result(dump.errorCode(), dump.errorMessage());
  }

  body->add(dump.slice());
  return Result();
}

arangodb::Result ClusterInfo::agencyPlan(std::shared_ptr<VPackBuilder> body) {
  auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
  auto [acb, index] = agencyCache.read({AgencyCommHelper::path("Plan"),
                                        AgencyCommHelper::path("Sync/LatestID")});
  VPackSlice result = acb->slice();

  if (result.isArray()) {
    body->add(acb->slice());
  } else {
    LOG_TOPIC("36ada", DEBUG, Logger::CLUSTER) <<
      "Failed to acquire the Plan section from the agency cache: " << acb->toJson();
    VPackObjectBuilder g(body.get());
  }
  return Result();
}

arangodb::Result ClusterInfo::agencyReplan(VPackSlice const plan) {
  // Apply only Collections and DBServers
  AgencyWriteTransaction planTransaction(std::vector<AgencyOperation>{
      {"Plan/Collections", AgencyValueOperationType::SET,
        plan.get({"arango", "Plan", "Collections"})},
      {"Plan/Databases", AgencyValueOperationType::SET,
        plan.get({"arango", "Plan", "Databases"})},
      {"Plan/Views", AgencyValueOperationType::SET,
        plan.get({"arango", "Plan", "Views"})},
      {"Plan/Version", AgencySimpleOperationType::INCREMENT_OP},
      {"Sync/UserVersion", AgencySimpleOperationType::INCREMENT_OP},
      {"Sync/HotBackupRestoreDone", AgencySimpleOperationType::INCREMENT_OP}});

  VPackSlice latestIdSlice = plan.get({"arango", "Sync", "LatestID"});
  if (!latestIdSlice.isNone()) {
    planTransaction.operations.push_back(
      {"Sync/LatestID", AgencyValueOperationType::SET, latestIdSlice});
  }
  AgencyCommResult r = _agency.sendTransactionWithFailover(planTransaction);
  if (!r.successful()) {
    arangodb::Result result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        basics::StringUtils::concatT("Error reporting to agency: _statusCode: ",
                                     r.errorCode()));
    return result;
  }

  Result rr;
  if (VPackSlice resultsSlice = r.slice().get("results"); resultsSlice.length() > 0) {
    rr = waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
  }

  return rr;
}

std::string const backupKey = "/arango/Target/HotBackup/Create";
std::string const maintenanceKey = "/arango/Supervision/Maintenance";
std::string const supervisionMode = "/arango/Supervision/State/Mode";
std::string const toDoKey = "/arango/Target/ToDo";
std::string const pendingKey = "/arango/Target/Pending";
std::string const writeURL = "_api/agency/write";

arangodb::Result ClusterInfo::agencyHotBackupLock(std::string const& backupId,
                                                  double const& timeout,
                                                  bool& supervisionOff) {
  using namespace std::chrono;

  auto const endTime =
      steady_clock::now() + milliseconds(static_cast<uint64_t>(1.0e3 * timeout));
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
        builder.add(                                      // Backup lock
          backupKey,
          VPackValue(
            timepointToString(
              std::chrono::system_clock::now() + std::chrono::seconds(timeouti))));
        builder.add(                                      // Turn off supervision
          maintenanceKey,
          VPackValue(
            timepointToString(
              std::chrono::system_clock::now() + std::chrono::seconds(timeouti))));
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
        builder.add(                                      // Backup lock
          backupKey,
          VPackValue(
            timepointToString(
              std::chrono::system_clock::now() + std::chrono::seconds(timeouti))));
        builder.add(                                      // Turn off supervision
          maintenanceKey,
          VPackValue(
            timepointToString(
              std::chrono::system_clock::now() + std::chrono::seconds(timeouti))));
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
  auto result = _agency.sendWithFailover(arangodb::rest::RequestType::POST,
                                         timeout, writeURL, builder.slice());

  LOG_TOPIC("53a93", DEBUG, Logger::BACKUP)
      << "agency lock for hot backup " << backupId << " scheduled with "
      << builder.toJson();

  // *** ATTENTION ***: Result will always be 412.
  // So we're going to fail, if we have an error OTHER THAN 412:
  if (!result.successful() && result.httpCode() != rest::ResponseCode::PRECONDITION_FAILED) {
    return arangodb::Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                            "failed to acquire backup lock in agency");
  }

  LOG_TOPIC("a94d5", DEBUG, Logger::BACKUP)
      << "agency lock response for backup id " << backupId << ": "
      << result.slice().toJson();

  if (!result.slice().isObject() || !result.slice().hasKey("results") ||
      !result.slice().get("results").isArray() ||
      result.slice().get("results").length() != 2) {
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        "invalid agency result while acquiring backup lock");
  }
  auto ar = result.slice().get("results");

  uint64_t first = ar[0].getNumber<uint64_t>();
  uint64_t second = ar[1].getNumber<uint64_t>();

  if (first == 0 && second == 0) {  // tough luck
    return arangodb::Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
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
    auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
    auto [result, index] = agencyCache.get("Supervision/State/Mode");

    if (result->slice().isString()) {
      if (result->slice().isEqualString("Maintenance")) {
        LOG_TOPIC("76a2c", DEBUG, Logger::BACKUP)
          << "agency hot backup lock acquired";
        return arangodb::Result();
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

  return arangodb::Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL,
      "timeout waiting for maintenance mode to be activated in agency");
}

arangodb::Result ClusterInfo::agencyHotBackupUnlock(std::string const& backupId,
                                                    double const& timeout,
                                                    const bool& supervisionOff) {
  using namespace std::chrono;

  auto const endTime =
      steady_clock::now() + milliseconds(static_cast<uint64_t>(1.0e3 * timeout));

  LOG_TOPIC("6ae41", DEBUG, Logger::BACKUP)
      << "unlocking backup lock for backup " + backupId + "  in agency";

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
  auto result = _agency.sendWithFailover(arangodb::rest::RequestType::POST,
                                         timeout, writeURL, builder.slice());
  if (!result.successful() &&
      result.httpCode() != rest::ResponseCode::PRECONDITION_FAILED) {
    LOG_TOPIC("6ae43", WARN, Logger::BACKUP)
        << "Error when unlocking backup lock for backup " << backupId
        << " in agency, errorCode: " << result.httpCode()
        << ", errorMessage: " << result.errorMessage();
    return arangodb::Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                            "failed to release backup lock in agency");
  }

  if (!result.slice().isObject() || !result.slice().hasKey("results") ||
      !result.slice().get("results").isArray()) {
    LOG_TOPIC("6ae44", WARN, Logger::BACKUP)
        << "Illegal response when unlocking backup lock for backup " << backupId
        << " in agency.";
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        "invalid agency result while releasing backup lock");
  }

  auto ar = result.slice().get("results");
  if (!ar[0].isNumber()) {
    LOG_TOPIC("6ae45", WARN, Logger::BACKUP)
        << "Invalid agency result when unlocking backup lock for backup "
        << backupId << " in agency: " << result.slice().toJson();
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        "invalid agency result while releasing backup lock");
  }

  if (supervisionOff) {
    return arangodb::Result();
  }

  double wait = 0.1;
  while (!_server.isStopping() && std::chrono::steady_clock::now() < endTime) {

    auto& agencyCache = _server.getFeature<ClusterFeature>().agencyCache();
    auto [res, index] = agencyCache.get("Supervision/State/Mode");

    if (!res->slice().isString()) {
      LOG_TOPIC("6ae46", WARN, Logger::BACKUP)
          << "Invalid JSON from agency when deactivating supervision mode for "
             "backup "
          << backupId;
      return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string("invalid JSON from agency, when deactivating supervision mode:") +
        res->slice().toJson());
    }

    if (res->slice().isEqualString("Normal")) {
      return arangodb::Result();
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

  return arangodb::Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL,
      "timeout waiting for maintenance mode to be deactivated in agency");
}

application_features::ApplicationServer& ClusterInfo::server() const {
  return _server;
}

ClusterInfo::ServersKnown::ServersKnown(VPackSlice const serversKnownSlice,
                                        std::unordered_set<ServerID> const& serverIds)
    : _serversKnown() {
  TRI_ASSERT(serversKnownSlice.isNone() || serversKnownSlice.isObject());
  if (serversKnownSlice.isObject()) {
    for (auto const it : VPackObjectIterator(serversKnownSlice)) {
      std::string serverId = it.key.copyString();
      VPackSlice const knownServerSlice = it.value;
      TRI_ASSERT(knownServerSlice.isObject());
      if (knownServerSlice.isObject()) {
        VPackSlice const rebootIdSlice = knownServerSlice.get("rebootId");
        TRI_ASSERT(rebootIdSlice.isInteger());
        if (rebootIdSlice.isInteger()) {
          auto const rebootId = RebootId{rebootIdSlice.getNumericValue<uint64_t>()};
          _serversKnown.try_emplace(std::move(serverId), rebootId);
        }
      }
    }
  }
}

std::unordered_map<ServerID, ClusterInfo::ServersKnown::KnownServer> const&
ClusterInfo::ServersKnown::serversKnown() const noexcept {
  return _serversKnown;
}

std::unordered_map<ServerID, RebootId> ClusterInfo::ServersKnown::rebootIds() const {
  std::unordered_map<ServerID, RebootId> rebootIds;
  for (auto const& it : _serversKnown) {
    rebootIds.try_emplace(it.first, it.second.rebootId());
  }
  return rebootIds;
}

void ClusterInfo::startSyncers() {
  _planSyncer = std::make_unique<SyncerThread>(_server, "Plan", std::bind(&ClusterInfo::loadPlan, this), _agencyCallbackRegistry);
  _curSyncer = std::make_unique<SyncerThread>(_server, "Current", std::bind(&ClusterInfo::loadCurrent, this), _agencyCallbackRegistry);
  
  if (!_planSyncer->start() || !_curSyncer->start()) {
    LOG_TOPIC("b4fa6", FATAL, Logger::CLUSTER)
      << "unable to start PlanSyncer/CurrentSYncer";
    FATAL_ERROR_EXIT();
  }
}

void ClusterInfo::drainSyncers() {
  {
    std::lock_guard g(_waitPlanLock);
    auto pit = _waitPlan.begin();
    while (pit != _waitPlan.end()) {
      pit->second.setValue(Result(_syncerShutdownCode));
      ++pit;
    }
    _waitPlan.clear();
  }

  {
    std::lock_guard g(_waitCurrentLock);
    auto pit = _waitCurrent.begin();
    while (pit != _waitCurrent.end()) {
      pit->second.setValue(Result(_syncerShutdownCode));
      ++pit;
    }
    _waitCurrent.clear();
  }
}

void ClusterInfo::shutdownSyncers() {
  drainSyncers();

  if (_planSyncer != nullptr) {
    _planSyncer->beginShutdown();
  }
  if (_curSyncer != nullptr) {
    _curSyncer->beginShutdown();
  }
}

void ClusterInfo::waitForSyncersToStop() {
  drainSyncers();

  auto start = std::chrono::steady_clock::now();
  while ((_planSyncer != nullptr && _planSyncer->isRunning()) || 
         (_curSyncer != nullptr && _curSyncer->isRunning())) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
      LOG_TOPIC("b8a5d", FATAL, Logger::CLUSTER)
        << "exiting prematurely as we failed to end syncer threads in ClusterInfo";
      FATAL_ERROR_EXIT();
    }
  }
}


VPackSlice PlanCollectionReader::indexes() {
  VPackSlice res = _collection.get("indexes");
  if (res.isNone()) {
    return VPackSlice::emptyArraySlice();
  } else {
    TRI_ASSERT(res.isArray());
    return res;
  }
}
  
CollectionWatcher::CollectionWatcher(AgencyCallbackRegistry* agencyCallbackRegistry, LogicalCollection const& collection)
    : _agencyCallbackRegistry(agencyCallbackRegistry), _present(true) {

  std::string databaseName = collection.vocbase().name();
  std::string collectionID = std::to_string(collection.id().id());
  std::string where = "Plan/Collections/" + databaseName + "/" + collectionID;

  _agencyCallback = std::make_shared<AgencyCallback>(
      collection.vocbase().server(), where,
      [this](VPackSlice const& result) {
        if (result.isNone()) {
          _present.store(false);
        }
        return true;
      },
      true, false);
  Result res = _agencyCallbackRegistry->registerCallback(_agencyCallback);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

CollectionWatcher::~CollectionWatcher() {
  try {
    _agencyCallbackRegistry->unregisterCallback(_agencyCallback);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42af2", WARN, Logger::CLUSTER)
      << "caught unexpected exception in CollectionWatcher: " << ex.what();
  }
}


ClusterInfo::SyncerThread::SyncerThread(
  application_features::ApplicationServer& server, std::string const& section,
  std::function<void()> const& f, AgencyCallbackRegistry* cregistry) :
  arangodb::Thread(server, section + "Syncer"), _news(false),
  _section(section), _f(f), _cr(cregistry) {}

ClusterInfo::SyncerThread::~SyncerThread() { shutdown(); }

bool ClusterInfo::SyncerThread::notify(velocypack::Slice const& slice) {
  std::lock_guard<std::mutex> lck(_m);
  _news = true;
  _cv.notify_one();
  return _news;
}

void ClusterInfo::SyncerThread::beginShutdown() {

  using namespace std::chrono_literals;

  // set the shutdown state in parent class
  Thread::beginShutdown();
  {
    std::lock_guard<std::mutex> lck(_m);
    _news = false;
  }
  _cv.notify_one();
}

bool ClusterInfo::SyncerThread::start() {
  LOG_TOPIC("38256", DEBUG, Logger::CLUSTER)
      << "Starting "
      << (currentThreadName() != nullptr ? currentThreadName() : "by unknown thread");
  return Thread::start();
}

void ClusterInfo::SyncerThread::run() {
  using namespace std::chrono_literals;

  std::function<bool(VPackSlice const& result)> update =
    [=](VPackSlice const& result) {
      if (!result.isNumber()) {
        LOG_TOPIC("d068f", ERR, Logger::CLUSTER)
          << "Plan Version is not a number! " << result.toJson();
        return false;
      }
      return notify(result);
    };

  auto acb =
    std::make_shared<AgencyCallback>(_server, _section + "/Version", update, true, false);
  Result res = _cr->registerCallback(std::move(acb));
  if (res.fail()) {
    LOG_TOPIC("70e05", FATAL, arangodb::Logger::CLUSTER)
      << "Failed to register callback with local registry: " << res.errorMessage();
    FATAL_ERROR_EXIT();
  }

  // This first call needs to be done or else we might miss all potential until
  // such time, that we are ready to receive. Under no circumstances can we assume
  // that this first call can be neglected.
  _f();

  while (!isStopping()) {
    bool news = false;
    {
      std::unique_lock<std::mutex> lk(_m);
      if (!_news) {
        // The timeout is strictly speaking not needed. However, we really do
        // not want to be caught in here in production.
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        _cv.wait(lk);
#else
        _cv.wait_for(lk, 100ms);
#endif
      }
      news = _news;
    }

    if (news) {
      {
        std::unique_lock<std::mutex> lk(_m);
        _news = false;
      }
      try {
        _f();
      } catch (basics::Exception const& ex) {
        if (ex.code() != TRI_ERROR_SHUTTING_DOWN) {
          LOG_TOPIC("9d1f5", WARN, arangodb::Logger::CLUSTER)
            << "caught an error while loading " << _section << ": " << ex.what();
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("752c4", WARN, arangodb::Logger::CLUSTER)
          << "caught an error while loading " << _section << ": " << ex.what();
      } catch (...) {
        LOG_TOPIC("30968", WARN, arangodb::Logger::CLUSTER)
          << "caught an error while loading " << _section;
      }
    }
    // next round...
  }

  try {
    _cr->unregisterCallback(acb);
  } catch (basics::Exception const& ex) {
    if (ex.code() != TRI_ERROR_SHUTTING_DOWN) {
      LOG_TOPIC("39336", WARN, arangodb::Logger::CLUSTER)
        << "caught exception while unregistering callback: " << ex.what();
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC("66f2f", WARN, arangodb::Logger::CLUSTER)
      << "caught exception while unregistering callback: " << ex.what();
  } catch (...) {
    LOG_TOPIC("995cd", WARN, arangodb::Logger::CLUSTER)
      << "caught unknown exception while unregistering callback";
  }
}

futures::Future<arangodb::Result> ClusterInfo::waitForCurrent(uint64_t raftIndex) {
  READ_LOCKER(readLocker, _currentProt.lock);
  if (raftIndex <= _currentIndex) {
    return futures::makeFuture(arangodb::Result());
  }
  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitCurrentLock);
  return _waitCurrent.emplace(raftIndex, futures::Promise<arangodb::Result>())->second.getFuture();
}

futures::Future<arangodb::Result> ClusterInfo::waitForCurrentVersion(uint64_t currentVersion) {
  READ_LOCKER(readLocker, _currentProt.lock);
  if (currentVersion <= _currentVersion) {
    return futures::makeFuture(arangodb::Result());
  }
  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitCurrentVersionLock);
  return _waitCurrentVersion
      .emplace(currentVersion, futures::Promise<arangodb::Result>())
      ->second.getFuture();
}

futures::Future<arangodb::Result> ClusterInfo::waitForPlan(uint64_t raftIndex) {
  READ_LOCKER(readLocker, _planProt.lock);
  if (raftIndex <= _planIndex) {
    return futures::makeFuture(arangodb::Result());
  }

  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitPlanLock);
  return _waitPlan.emplace(raftIndex, futures::Promise<arangodb::Result>())->second.getFuture();
}

futures::Future<Result> ClusterInfo::waitForPlanVersion(uint64_t planVersion) {
  READ_LOCKER(readLocker, _planProt.lock);
  if (planVersion <= _planVersion) {
    return futures::makeFuture(arangodb::Result());
  }

  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitPlanVersionLock);
  return _waitPlanVersion
      .emplace(planVersion, futures::Promise<arangodb::Result>())
      ->second.getFuture();
}

futures::Future<Result> ClusterInfo::fetchAndWaitForPlanVersion(network::Timeout timeout) {
  // Save the applicationServer, not the ClusterInfo, in case of shutdown.
  return cluster::fetchPlanVersion(timeout).thenValue(
      [&applicationServer = server()](auto maybePlanVersion) {
        if (maybePlanVersion.ok()) {
          auto planVersion = maybePlanVersion.get();

          auto& clusterInfo =
              applicationServer.getFeature<ClusterFeature>().clusterInfo();

          return clusterInfo.waitForPlanVersion(planVersion);
        } else {
          return futures::Future<Result>{maybePlanVersion.result()};
        }
      });
}


// Debugging output no need for consistency across locks
VPackBuilder ClusterInfo::toVelocyPack() {
  VPackBuilder dump;
  {
    { VPackObjectBuilder c(&dump);
      {
        READ_LOCKER(readLocker, _planProt.lock);
        dump.add(VPackValue("plan"));
        { VPackObjectBuilder d(&dump);
          for (auto const& i : _plan) {
            dump.add(i.first, i.second->slice());
          }
        }
        dump.add(VPackValue("plannedCollections"));
        { VPackObjectBuilder d(&dump);
          for (auto const& c : _plannedCollections) {
            dump.add(VPackValue(c.first));
            VPackArrayBuilder cs(&dump);
            for (auto const& col : *c.second) {
              dump.add(VPackValue(col.first));
            }
          }
        }
        dump.add(VPackValue("shardToName"));
        { VPackObjectBuilder d(&dump);
          for (auto const& s : _shardToName) {
            dump.add(s.first, VPackValue(s.second));
          }
        }
        dump.add(VPackValue("shardServers"));
        { VPackObjectBuilder d(&dump);
          for (auto const& s : _shardServers) {
            dump.add(VPackValue(s.first));
            VPackArrayBuilder a(&dump);
            for (auto const& sv : s.second) {
              dump.add(VPackValue(sv));
            }
          }
        }
        dump.add(VPackValue("shards"));
        { VPackObjectBuilder d(&dump);
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
        { VPackObjectBuilder d(&dump);
          for (auto const& i : _current) {
            dump.add(i.first, i.second->slice());
          }
        }
        dump.add(VPackValue("shardIds"));
        { VPackObjectBuilder d(&dump);
          for (auto const& i : _shardIds) {
            dump.add(VPackValue(i.first));
            VPackArrayBuilder a(&dump);
            for (auto const& i : *i.second){
              dump.add(VPackValue(i));
            }
          }
        }
      }
      {
        READ_LOCKER(readLocker, _DBServersProt.lock);
        dump.add(VPackValue("DBServers"));
        { VPackObjectBuilder d(&dump);
          for (auto const& dbs : _DBServers) {
            dump.add(dbs.first, VPackValue(dbs.second));
          }
        }
      }
      {
        READ_LOCKER(readLocker, _coordinatorsProt.lock);
        dump.add(VPackValue("coordinators"));
        { VPackObjectBuilder c(&dump);
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
  std::multimap<uint64_t, futures::Promise<arangodb::Result>>& mm, uint64_t commitIndex) {

  auto* scheduler = SchedulerFeature::SCHEDULER;
  auto pit = mm.begin();
  while (pit != mm.end()) {
    if (pit->first > commitIndex) {
      break;
    }
    auto pp = std::make_shared<futures::Promise<Result>>(std::move(pit->second));
    if (scheduler && !_server.isStopping()) {
      bool queued = scheduler->queue(
        RequestLane::CLUSTER_INTERNAL, [pp] { pp->setValue(Result()); });
      if (!queued) {
        LOG_TOPIC("4637c", WARN, Logger::AGENCY) <<
          "Failed to schedule logsForTrigger running in main thread";
        pp->setValue(Result());
      }
    } else {
      pp->setValue(Result(_syncerShutdownCode));
    }
    pit = mm.erase(pit);
  }

}

AnalyzerModificationTransaction::AnalyzerModificationTransaction(
    DatabaseID const& database, ClusterInfo* ci, bool cleanup)
  : _clusterInfo(ci), _database(database), _cleanupTransaction(cleanup) {
  TRI_ASSERT(_clusterInfo);
}

AnalyzerModificationTransaction::~AnalyzerModificationTransaction() {
  try {
    abort();
  }
  catch (...) {} // force no exceptions
  TRI_ASSERT(!_rollbackCounter && !_rollbackRevision);
}

int32_t AnalyzerModificationTransaction::getPendingCount() noexcept {
  return _pendingAnalyzerOperationsCount.load(std::memory_order::memory_order_relaxed);
}

AnalyzersRevision::Revision AnalyzerModificationTransaction::buildingRevision() const noexcept {
  TRI_ASSERT(_buildingRevision != AnalyzersRevision::LATEST); // unstarted transation access
  return _buildingRevision;
}

Result AnalyzerModificationTransaction::start() {
  auto const endTime = TRI_microtime() + 5.0; // arbitrary value.
  int32_t count = _pendingAnalyzerOperationsCount.load(std::memory_order::memory_order_relaxed);
  // locking stage
  while (true) {
    // Do not let break out of cleanup mode.
    // Cleanup itself could start only from idle state.
    if (count < 0 || _cleanupTransaction) {
      count = 0;
    }
    if (_pendingAnalyzerOperationsCount.compare_exchange_weak(count,
      _cleanupTransaction ? -1 : count + 1,
      std::memory_order_acquire)) {
      break;
    }
    if (TRI_microtime() > endTime) {
      return Result(
        TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,
        "start modifying analyzer for database " + _database + ": failed to acquire operation counter");
    }
  }
  _rollbackCounter = true; // from now on we must release our counter

  if (_cleanupTransaction) {
    _buildingRevision = _clusterInfo->getAnalyzersRevision(_database, false)->getRevision();
    TRI_ASSERT(_clusterInfo->getAnalyzersRevision(_database, false)->getBuildingRevision() != _buildingRevision);
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
  auto res = _clusterInfo->finishModifyingAnalyzerCoordinator(_database, _cleanupTransaction);
  _rollbackRevision = res.fail() && !_cleanupTransaction;
  // if succesful revert mark our transaction as completed (otherwise postpone it to abort call).
  // for cleanup - always this attempt is completed (cleanup should not waste much time). Will try next time
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
      TRI_ASSERT(!_cleanupTransaction); // cleanup transaction has nothing to rollback
      _rollbackRevision = false; // ok, we tried. Even if failed -> recovery job will do the rest
      res = _clusterInfo->finishModifyingAnalyzerCoordinator(_database, true);
    }
  }
  catch (...) { // let`s be as safe as possible
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

std::atomic<int32_t>  arangodb::AnalyzerModificationTransaction::_pendingAnalyzerOperationsCount{ 0 };

namespace {
template <typename T>
futures::Future<ResultT<T>> fetchNumberFromAgency(std::shared_ptr<cluster::paths::Path const> path,
                                                  network::Timeout timeout) {
  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope::into_builder(builder)
        .read()
        .key(path->str())
        .end()
        .done();
  }

  auto fAacResult = AsyncAgencyComm().sendReadTransaction(timeout, std::move(trx));

  auto fResult = std::move(fAacResult).thenValue([path = std::move(path)](auto&& result) {
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

futures::Future<ResultT<uint64_t>> cluster::fetchPlanVersion(network::Timeout timeout) {
  using namespace std::chrono_literals;

  auto planVersionPath = cluster::paths::root()->arango()->plan()->version();

  return fetchNumberFromAgency<uint64_t>(std::static_pointer_cast<paths::Path const>(
                                             std::move(planVersionPath)),
                                         timeout);
}

futures::Future<ResultT<uint64_t>> cluster::fetchCurrentVersion(network::Timeout timeout) {
  using namespace std::chrono_literals;

  auto currentVersionPath = cluster::paths::root()->arango()->current()->version();

  return fetchNumberFromAgency<uint64_t>(std::static_pointer_cast<paths::Path const>(
                                             std::move(currentVersionPath)),
                                         timeout);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
