////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "LogicalCollection.h"

#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Basics/fasthash.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/PerformanceLogScope.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

namespace {

static std::string translateStatus(TRI_vocbase_col_status_e status) {
  switch (status) {
    case TRI_VOC_COL_STATUS_UNLOADED:
      return "unloaded";
    case TRI_VOC_COL_STATUS_LOADED:
      return "loaded";
    case TRI_VOC_COL_STATUS_UNLOADING:
      return "unloading";
    case TRI_VOC_COL_STATUS_DELETED:
      return "deleted";
    case TRI_VOC_COL_STATUS_LOADING:
      return "loading";
    case TRI_VOC_COL_STATUS_CORRUPTED:
    case TRI_VOC_COL_STATUS_NEW_BORN:
    default:
      return "unknown";
  }
}

static TRI_voc_cid_t ReadCid(VPackSlice info) {
  if (!info.isObject()) {
    // ERROR CASE
    return 0;
  }

  // Somehow the cid is now propagated to dbservers
  TRI_voc_cid_t cid = Helper::extractIdValue(info);

  if (cid == 0) {
    if (ServerState::instance()->isDBServer()) {
      cid = ClusterInfo::instance()->uniqid(1);
    } else if (ServerState::instance()->isCoordinator()) {
      cid = ClusterInfo::instance()->uniqid(1);
    } else {
      cid = TRI_NewTickServer();
    }
  }
  return cid;
}

static TRI_voc_cid_t ReadPlanId(VPackSlice info, TRI_voc_cid_t cid) {
  if (!info.isObject()) {
    // ERROR CASE
    return 0;
  }
  VPackSlice id = info.get("planId");
  if (id.isNone()) {
    return cid;
  }

  if (id.isString()) {
    // string cid, e.g. "9988488"
    return arangodb::basics::StringUtils::uint64(id.copyString());
  } else if (id.isNumber()) {
    // numeric cid, e.g. 9988488
    return id.getNumericValue<uint64_t>();
  }
  // TODO Throw error for invalid type?
  return cid;
}

static std::string const ReadStringValue(VPackSlice info,
                                         std::string const& name,
                                         std::string const& def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getStringValue(info, name, def);
}
}

/// @brief This the "copy" constructor used in the cluster
///        it is required to create objects that survive plan
///        modifications and can be freed
///        Can only be given to V8, cannot be used for functionality.
LogicalCollection::LogicalCollection(LogicalCollection const& other)
    : _internalVersion(0),
      _cid(other.cid()),
      _planId(other.planId()),
      _type(other.type()),
      _name(other.name()),
      _distributeShardsLike(other._distributeShardsLike),
      _avoidServers(other.avoidServers()),
      _isSmart(other.isSmart()),
      _status(other.status()),
      _isLocal(false),
      _isDeleted(other._isDeleted),
      _isSystem(other.isSystem()),
      _version(other._version),
      _waitForSync(other.waitForSync()),
      _replicationFactor(other.replicationFactor()),
      _numberOfShards(other.numberOfShards()),
      _allowUserKeys(other.allowUserKeys()),
      _shardIds(new ShardMap()),  // Not needed
      _vocbase(other.vocbase()),
      _keyOptions(other._keyOptions),
      _keyGenerator(KeyGenerator::factory(VPackSlice(keyOptions()))),
      _physical(other.getPhysical()->clone(this)),
      _clusterEstimateTTL(0),
      _planVersion(other._planVersion) {
  TRI_ASSERT(_physical != nullptr);
  if (ServerState::instance()->isDBServer() ||
      !ServerState::instance()->isRunningInCluster()) {
    _followers.reset(new FollowerInfo(this));
  }
}

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this collection.
LogicalCollection::LogicalCollection(TRI_vocbase_t* vocbase,
                                     VPackSlice const& info)
    : _internalVersion(0),
      _cid(ReadCid(info)),
      _planId(ReadPlanId(info, _cid)),
      _type(Helper::readNumericValue<TRI_col_type_e, int>(
          info, "type", TRI_COL_TYPE_UNKNOWN)),
      _name(ReadStringValue(info, "name", "")),
      _distributeShardsLike(ReadStringValue(info, "distributeShardsLike", "")),
      _isSmart(Helper::readBooleanValue(info, "isSmart", false)),
      _status(Helper::readNumericValue<TRI_vocbase_col_status_e, int>(
          info, "status", TRI_VOC_COL_STATUS_CORRUPTED)),
      _isLocal(!ServerState::instance()->isCoordinator()),
      _isDeleted(Helper::readBooleanValue(info, "deleted", false)),
      _isSystem(IsSystemName(_name) &&
                Helper::readBooleanValue(info, "isSystem", false)),
      _version(Helper::readNumericValue<uint32_t>(info, "version",
                                                  currentVersion())),
      _waitForSync(Helper::readBooleanValue(info, "waitForSync", false)),
      _replicationFactor(1),
      _numberOfShards(
          Helper::readNumericValue<size_t>(info, "numberOfShards", 1)),
      _allowUserKeys(Helper::readBooleanValue(info, "allowUserKeys", true)),
      _shardIds(new ShardMap()),
      _vocbase(vocbase),
      _keyOptions(nullptr),
      _keyGenerator(),
      _physical(
          EngineSelectorFeature::ENGINE->createPhysicalCollection(this, info)),
      _clusterEstimateTTL(0),
      _planVersion(0) {
  // add keyoptions from slice
  TRI_ASSERT(info.isObject());
  VPackSlice keyOpts = info.get("keyOptions");
  _keyGenerator.reset(KeyGenerator::factory(keyOpts));
  if (!keyOpts.isNone()) {
    _keyOptions = VPackBuilder::clone(keyOpts).steal();
  }

  TRI_ASSERT(_physical != nullptr);
  if (!IsAllowedName(info)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // This has to be called AFTER _phyiscal and _logical are properly linked
  // together.
  prepareIndexes(info.get("indexes"));

  if (_version < minimumVersion()) {
    // collection is too "old"
    std::string errorMsg(std::string("collection '") + _name +
                         "' has a too old version. Please start the server "
                         "with the --database.auto-upgrade option.");

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, errorMsg);
  }

  VPackSlice shardKeysSlice = info.get("shardKeys");

  bool const isCluster = ServerState::instance()->isRunningInCluster();
  // Cluster only tests
  if (ServerState::instance()->isCoordinator()) {
    if ((_numberOfShards == 0 && !_isSmart) || _numberOfShards > 1000) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid number of shards");
    }

    VPackSlice keyGenSlice = info.get("keyOptions");
    if (keyGenSlice.isObject()) {
      keyGenSlice = keyGenSlice.get("type");
      if (keyGenSlice.isString()) {
        StringRef tmp(keyGenSlice);
        if (!tmp.empty() && tmp != "traditional") {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_UNSUPPORTED,
                                         "non-traditional key generators are "
                                         "not supported for sharded "
                                         "collections");
        }
      }
    }
  }

  auto replicationFactorSlice = info.get("replicationFactor");
  if (!replicationFactorSlice.isNone()) {
    bool isError = true;
    if (replicationFactorSlice.isNumber()) {
      _replicationFactor = replicationFactorSlice.getNumber<size_t>();
      // mop: only allow satellite collections to be created explicitly
      if (_replicationFactor > 0 && _replicationFactor <= 10) {
        isError = false;
#ifdef USE_ENTERPRISE
      } else if (_replicationFactor == 0) {
        isError = false;
#endif
      }
    }
#ifdef USE_ENTERPRISE
    else if (replicationFactorSlice.isString() &&
             replicationFactorSlice.copyString() == "satellite") {
      _replicationFactor = 0;
      _numberOfShards = 1;
      _distributeShardsLike = "";
      _avoidServers.clear();
      isError = false;
    }
#endif
    if (isError) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid replicationFactor");
    }
  }

  if (shardKeysSlice.isNone() || isSatellite()) {
    // Use default.
    _shardKeys.emplace_back(StaticStrings::KeyString);
  } else {
    if (shardKeysSlice.isArray()) {
      for (auto const& sk : VPackArrayIterator(shardKeysSlice)) {
        if (sk.isString()) {
          std::string key = sk.copyString();
          // remove : char at the beginning or end (for enterprise)
          std::string stripped;
          if (!key.empty()) {
            if (key.front() == ':') {
              stripped = key.substr(1);
            } else if (key.back() == ':') {
              stripped = key.substr(0, key.size() - 1);
            } else {
              stripped = key;
            }
          }
          // system attributes are not allowed (except _key)
          if (!stripped.empty() && stripped != StaticStrings::IdString &&
              stripped != StaticStrings::RevString) {
            _shardKeys.emplace_back(key);
          }
        }
      }
      if (_shardKeys.empty() && !isCluster) {
        // Compatibility. Old configs might store empty shard-keys locally.
        // This is translated to ["_key"]. In cluster-case this always was
        // forbidden.
        _shardKeys.emplace_back(StaticStrings::KeyString);
      }
    }
  }

  if (_shardKeys.empty() || _shardKeys.size() > 8) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid number of shard keys");
  }

  auto shardsSlice = info.get("shards");
  if (shardsSlice.isObject()) {
    for (auto const& shardSlice : VPackObjectIterator(shardsSlice)) {
      if (shardSlice.key.isString() && shardSlice.value.isArray()) {
        ShardID shard = shardSlice.key.copyString();

        std::vector<ServerID> servers;
        for (auto const& serverSlice : VPackArrayIterator(shardSlice.value)) {
          servers.push_back(serverSlice.copyString());
        }
        _shardIds->emplace(shard, servers);
      }
    }
  }

  if (info.hasKey("avoidServers")) {
    auto avoidServersSlice = info.get("avoidServers");
    if (avoidServersSlice.isArray()) {
      for (const auto& i : VPackArrayIterator(avoidServersSlice)) {
        if (i.isString()) {
          _avoidServers.push_back(i.copyString());
        } else {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
              << "avoidServers must be a vector of strings we got "
              << avoidServersSlice.toJson() << ". discarding!";
          _avoidServers.clear();
          break;
        }
      }
    }
  }

  if (ServerState::instance()->isDBServer() ||
      !ServerState::instance()->isRunningInCluster()) {
    _followers.reset(new FollowerInfo(this));
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(_cid));
}

LogicalCollection::~LogicalCollection() {}

void LogicalCollection::prepareIndexes(VPackSlice indexesSlice) {
  TRI_ASSERT(_physical != nullptr);

  if (!indexesSlice.isArray()) {
    // always point to an array
    indexesSlice = basics::VelocyPackHelper::EmptyArrayValue();
  }

  _physical->prepareIndexes(indexesSlice);
}

std::unique_ptr<IndexIterator> LogicalCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) {
  return _physical->getAllIterator(trx, mdr, reverse);
}

std::unique_ptr<IndexIterator> LogicalCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) {
  return _physical->getAnyIterator(trx, mdr);
}

void LogicalCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  _physical->invokeOnAllElements(trx, callback);
}

bool LogicalCollection::IsAllowedName(VPackSlice parameters) {
  bool allowSystem = Helper::readBooleanValue(parameters, "isSystem", false);
  std::string name = ReadStringValue(parameters, "name", "");
  if (name.empty()) {
    return false;
  }

  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (ptr = name.c_str(); *ptr; ++ptr) {
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

/// @brief checks if a collection name is allowed
/// Returns true if the name is allowed and false otherwise
bool LogicalCollection::IsAllowedName(bool allowSystem,
                                      std::string const& name) {
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (ptr = name.c_str(); *ptr; ++ptr) {
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

// @brief Return the number of documents in this collection
uint64_t LogicalCollection::numberDocuments(transaction::Methods* trx) const {
  return getPhysical()->numberDocuments(trx);
}

uint32_t LogicalCollection::internalVersion() const { return _internalVersion; }

std::string LogicalCollection::cid_as_string() const {
  return basics::StringUtils::itoa(_cid);
}

TRI_voc_cid_t LogicalCollection::planId() const { return _planId; }

std::string LogicalCollection::planId_as_string() const {
  return basics::StringUtils::itoa(_planId);
}

TRI_col_type_e LogicalCollection::type() const { return _type; }

std::string LogicalCollection::name() const {
  // TODO Activate this lock. Right now we have some locks outside.
  // READ_LOCKER(readLocker, _lock);
  return _name;
}

std::string const LogicalCollection::distributeShardsLike() const {
  return _distributeShardsLike;
}

void LogicalCollection::distributeShardsLike(std::string const& cid) {
  _distributeShardsLike = cid;
}

std::vector<std::string> const& LogicalCollection::avoidServers() const {
  return _avoidServers;
}

void LogicalCollection::avoidServers(std::vector<std::string> const& a) {
  _avoidServers = a;
}

std::string LogicalCollection::dbName() const {
  TRI_ASSERT(_vocbase != nullptr);
  return _vocbase->name();
}

TRI_vocbase_col_status_e LogicalCollection::status() const { return _status; }

TRI_vocbase_col_status_e LogicalCollection::getStatusLocked() {
  READ_LOCKER(readLocker, _lock);
  return _status;
}

void LogicalCollection::executeWhileStatusWriteLocked(
    std::function<void()> const& callback) {
  WRITE_LOCKER_EVENTUAL(locker, _lock);
  callback();
}

void LogicalCollection::executeWhileStatusLocked(
    std::function<void()> const& callback) {
  READ_LOCKER(locker, _lock);
  callback();
}

bool LogicalCollection::tryExecuteWhileStatusLocked(
    std::function<void()> const& callback) {
  TRY_READ_LOCKER(readLocker, _lock);
  if (!readLocker.isLocked()) {
    return false;
  }

  callback();
  return true;
}

TRI_vocbase_col_status_e LogicalCollection::tryFetchStatus(bool& didFetch) {
  TRY_READ_LOCKER(locker, _lock);
  if (locker.isLocked()) {
    didFetch = true;
    return _status;
  }
  didFetch = false;
  return TRI_VOC_COL_STATUS_CORRUPTED;
}

/// @brief returns a translation of a collection status
std::string LogicalCollection::statusString() const {
  READ_LOCKER(readLocker, _lock);
  return ::translateStatus(_status);
}

// SECTION: Properties
TRI_voc_rid_t LogicalCollection::revision(transaction::Methods* trx) const {
  // TODO CoordinatorCase
  return _physical->revision(trx);
}

bool LogicalCollection::isLocal() const { return _isLocal; }

bool LogicalCollection::deleted() const { return _isDeleted; }

bool LogicalCollection::isSystem() const { return _isSystem; }

bool LogicalCollection::waitForSync() const { return _waitForSync; }

bool LogicalCollection::isSmart() const { return _isSmart; }

std::unique_ptr<FollowerInfo> const& LogicalCollection::followers() const {
  return _followers;
}

void LogicalCollection::setDeleted(bool newValue) { _isDeleted = newValue; }

// SECTION: Indexes
std::unordered_map<std::string, double> LogicalCollection::clusterIndexEstimates(bool doNotUpdate){
  READ_LOCKER(readlock, _clusterEstimatesLock);
  if (doNotUpdate) {
    return _clusterEstimates;
  }

  double ctime = TRI_microtime(); // in seconds
  auto needEstimateUpdate = [this,ctime](){
    if(_clusterEstimates.empty()) {
      LOG_TOPIC(TRACE, Logger::CLUSTER) << "update because estimate is not availabe";
      return true;
    } else if (ctime - _clusterEstimateTTL > 10.0) {
      LOG_TOPIC(TRACE, Logger::CLUSTER) << "update because estimate is too old: " << ctime - _clusterEstimateTTL;
      return true;
    }
    return false;
  };

  if (needEstimateUpdate()){
    readlock.unlock();
    WRITE_LOCKER(writelock, _clusterEstimatesLock);
    if(needEstimateUpdate()){
      selectivityEstimatesOnCoordinator(_vocbase->name(), name(), _clusterEstimates);
      _clusterEstimateTTL = TRI_microtime();
    }
    return _clusterEstimates;
  }
  return _clusterEstimates;
}

void LogicalCollection::clusterIndexEstimates(std::unordered_map<std::string, double>&& estimates){
  WRITE_LOCKER(lock, _clusterEstimatesLock);
  _clusterEstimates = std::move(estimates);
}

std::vector<std::shared_ptr<arangodb::Index>>
LogicalCollection::getIndexes() const {
  return getPhysical()->getIndexes();
}

void LogicalCollection::getIndexesVPack(VPackBuilder& result, bool withFigures,
                                        bool forPersistence) const {
  getPhysical()->getIndexesVPack(result, withFigures, forPersistence);
}

// SECTION: Replication
int LogicalCollection::replicationFactor() const {
  return static_cast<int>(_replicationFactor);
}
void LogicalCollection::replicationFactor(int r) {
  _replicationFactor = static_cast<size_t>(r);
}

// SECTION: Sharding
int LogicalCollection::numberOfShards() const {
  return static_cast<int>(_numberOfShards);
}
void LogicalCollection::numberOfShards(int n) {
  _numberOfShards = static_cast<size_t>(n);
}

bool LogicalCollection::allowUserKeys() const { return _allowUserKeys; }

#ifndef USE_ENTERPRISE
bool LogicalCollection::usesDefaultShardKeys() const {
  return (_shardKeys.size() == 1 && _shardKeys[0] == StaticStrings::KeyString);
}
#endif

std::vector<std::string> const& LogicalCollection::shardKeys() const {
  return _shardKeys;
}

std::shared_ptr<ShardMap> LogicalCollection::shardIds() const {
  // TODO make threadsafe update on the cache.
  return _shardIds;
}

// return a filtered list of the collection's shards
std::shared_ptr<ShardMap> LogicalCollection::shardIds(
    std::unordered_set<std::string> const& includedShards) const {
  if (includedShards.empty()) {
    return _shardIds;
  }

  std::shared_ptr<ShardMap> copy = _shardIds;
  auto result = std::make_shared<ShardMap>();

  for (auto const& it : *copy) {
    if (includedShards.find(it.first) == includedShards.end()) {
      // a shard we are not interested in
      continue;
    }
    result->emplace(it.first, it.second);
  }
  return result;
}

void LogicalCollection::setShardMap(std::shared_ptr<ShardMap>& map) {
  _shardIds = map;
}

// SECTION: Modification Functions

// asks the storage engine to rename the collection to the given name
// and persist the renaming info. It is guaranteed by the server
// that no other active collection with the same name and id exists in the same
// database when this function is called. If this operation fails somewhere in
// the middle, the storage engine is required to fully revert the rename
// operation
// and throw only then, so that subsequent collection creation/rename requests
// will
// not fail. the WAL entry for the rename will be written *after* the call
// to "renameCollection" returns

int LogicalCollection::rename(std::string const& newName) {
  // Should only be called from inside vocbase.
  // Otherwise caching is destroyed.
  TRI_ASSERT(!ServerState::instance()->isCoordinator());  // NOT YET IMPLEMENTED

  // Check for illegal states.
  switch (_status) {
    case TRI_VOC_COL_STATUS_CORRUPTED:
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    case TRI_VOC_COL_STATUS_DELETED:
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    default:
      // Fall through intentional
      break;
  }

  switch (_status) {
    case TRI_VOC_COL_STATUS_UNLOADED:
    case TRI_VOC_COL_STATUS_LOADED:
    case TRI_VOC_COL_STATUS_UNLOADING:
    case TRI_VOC_COL_STATUS_LOADING: {
      break;
    }
    default:
      // Unknown status
      return TRI_ERROR_INTERNAL;
  }

  std::string oldName = _name;
  _name = newName;
  // Okay we can finally rename safely
  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    bool const doSync =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database")
            ->forceSyncProperties();
    engine->changeCollection(_vocbase, _cid, this, doSync);
  } catch (basics::Exception const& ex) {
    // Engine Rename somehow failed. Reset to old name
    _name = oldName;
    return ex.code();
  } catch (...) {
    // Engine Rename somehow failed. Reset to old name
    _name = oldName;
    return TRI_ERROR_INTERNAL;
  }

  // CHECK if this ordering is okay. Before change the version was increased
  // after swapping in vocbase mapping.
  increaseInternalVersion();
  return TRI_ERROR_NO_ERROR;
}

int LogicalCollection::close() {
  // This was unload() in 3.0
  return getPhysical()->close();
}

void LogicalCollection::load() {
  _physical->load();
}

void LogicalCollection::unload() {
  _physical->unload();
}

void LogicalCollection::drop() {
  // make sure collection has been closed
  this->close();

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->destroyCollection(_vocbase, this);
  _isDeleted = true;

  _physical->drop();
}

void LogicalCollection::setStatus(TRI_vocbase_col_status_e status) {
  _status = status;

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    increaseInternalVersion();
  }
}

void LogicalCollection::toVelocyPackForClusterInventory(VPackBuilder& result,
                                                        bool useSystem,
                                                        bool isReady) const {
  if (_isSystem && !useSystem) {
    return;
  }
  result.openObject();
  result.add(VPackValue("parameters"));

  std::unordered_set<std::string> ignoreKeys{"allowUserKeys", "cid", "count",
                                             "objectId",
                                             "statusString", "version",
                                             "distributeShardsLike"};
  VPackBuilder params = toVelocyPackIgnore(ignoreKeys, false, false);
  { VPackObjectBuilder guard(&result);
    for (auto const& p : VPackObjectIterator(params.slice())) {
      result.add(p.key);
      result.add(p.value);
    }
    if (!_distributeShardsLike.empty()) {
      CollectionNameResolver resolver(_vocbase);
      result.add("distributeShardsLike",
                 VPackValue(resolver.getCollectionNameCluster(
                     static_cast<TRI_voc_cid_t>(basics::StringUtils::uint64(
                         distributeShardsLike())))));
    }
  }

  result.add(VPackValue("indexes"));
  getIndexesVPack(result, false, false);
  result.add("planVersion", VPackValue(getPlanVersion()));
  result.add("isReady", VPackValue(isReady));
  result.close();  // CollectionInfo
}

void LogicalCollection::toVelocyPack(VPackBuilder& result, bool translateCids,
                                     bool forPersistence) const {
  // We write into an open object
  TRI_ASSERT(result.isOpenObject());

  // Collection Meta Information
  result.add("cid", VPackValue(std::to_string(_cid)));
  result.add("id", VPackValue(std::to_string(_cid)));
  result.add("name", VPackValue(_name));
  result.add("type", VPackValue(static_cast<int>(_type)));
  result.add("status", VPackValue(_status));
  result.add("statusString", VPackValue(::translateStatus(_status)));
  result.add("version", VPackValue(_version));

  // Collection Flags
  result.add("deleted", VPackValue(_isDeleted));
  result.add("isSystem", VPackValue(_isSystem));
  result.add("waitForSync", VPackValue(_waitForSync));

  // TODO is this still releveant or redundant in keyGenerator?
  result.add("allowUserKeys", VPackValue(_allowUserKeys));

  // keyoptions
  result.add(VPackValue("keyOptions"));
  if (_keyGenerator != nullptr) {
    result.openObject();
    _keyGenerator->toVelocyPack(result);
    result.close();
  } else {
    result.openArray();
    result.close();
  }

  // Physical Information
  getPhysical()->getPropertiesVPack(result);

  // Indexes
  result.add(VPackValue("indexes"));
  getIndexesVPack(result, false, forPersistence);

  // Cluster Specific
  result.add("isSmart", VPackValue(_isSmart));
  result.add("planId", VPackValue(std::to_string(_planId)));
  result.add("numberOfShards", VPackValue(_numberOfShards));
  result.add(VPackValue("shards"));
  result.openObject();
  auto tmpShards = _shardIds;
  for (auto const& shards : *tmpShards) {
    result.add(VPackValue(shards.first));
    result.openArray();
    for (auto const& servers : shards.second) {
      result.add(VPackValue(servers));
    }
    result.close();  // server array
  }
  result.close();  // shards

  if (isSatellite()) {
    result.add("replicationFactor", VPackValue("satellite"));
  } else {
    result.add("replicationFactor", VPackValue(_replicationFactor));
  }
  if (!_distributeShardsLike.empty() &&
      ServerState::instance()->isCoordinator()) {
    if (translateCids) {
      CollectionNameResolver resolver(_vocbase);
      result.add("distributeShardsLike",
                 VPackValue(resolver.getCollectionNameCluster(
                     static_cast<TRI_voc_cid_t>(basics::StringUtils::uint64(
                         distributeShardsLike())))));
    } else {
      result.add("distributeShardsLike", VPackValue(distributeShardsLike()));
    }
  }

  result.add(VPackValue("shardKeys"));
  result.openArray();
  for (auto const& key : _shardKeys) {
    result.add(VPackValue(key));
  }
  result.close();  // shardKeys

  if (!_avoidServers.empty()) {
    result.add(VPackValue("avoidServers"));
    result.openArray();
    for (auto const& server : _avoidServers) {
      result.add(VPackValue(server));
    }
    result.close();
  }

  includeVelocyPackEnterprise(result);

  TRI_ASSERT(result.isOpenObject());
  // We leave the object open
}

VPackBuilder LogicalCollection::toVelocyPackIgnore(
    std::unordered_set<std::string> const& ignoreKeys, bool translateCids,
    bool forPersistence) const {
  VPackBuilder full;
  full.openObject();
  toVelocyPack(full, translateCids, forPersistence);
  full.close();
  return VPackCollection::remove(full.slice(), ignoreKeys);
}

void LogicalCollection::includeVelocyPackEnterprise(VPackBuilder&) const {
  // We ain't no enterprise
}

void LogicalCollection::increaseInternalVersion() { ++_internalVersion; }

arangodb::Result LogicalCollection::updateProperties(VPackSlice const& slice,
                                                     bool doSync) {
  // the following collection properties are intentionally not updated as
  // updating
  // them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...

  WRITE_LOCKER(writeLocker, _infoLock);

  // The physical may first reject illegal properties.
  // After this call it either has thrown or the properties are stored
  Result res = getPhysical()->updateProperties(slice, doSync);

  if (!res.ok()) {
    return res;
  }

  _waitForSync = Helper::getBooleanValue(slice, "waitForSync", _waitForSync);

  if (!_isLocal) {
    // We need to inform the cluster as well
    int tmp = ClusterInfo::instance()->setCollectionPropertiesCoordinator(
        _vocbase->name(), cid_as_string(), this);
    if (tmp == TRI_ERROR_NO_ERROR) {
      return {};
    }
    return {tmp, TRI_errno_string(tmp)};
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->changeCollection(_vocbase, _cid, this, doSync);

  return {};
}

/// @brief return the figures for a collection
std::shared_ptr<arangodb::velocypack::Builder> LogicalCollection::figures() {
  if (ServerState::instance()->isCoordinator()) {
    auto builder = std::make_shared<VPackBuilder>();
    builder->openObject();
    builder->close();
    int res = figuresOnCoordinator(dbName(), cid_as_string(), builder);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    return builder;
  }
  return getPhysical()->figures();
}

/// @brief opens an existing collection
void LogicalCollection::open(bool ignoreErrors) {
  getPhysical()->open(ignoreErrors);
  TRI_UpdateTickServer(_cid);
}

/// SECTION Indexes

std::shared_ptr<Index> LogicalCollection::lookupIndex(
    TRI_idx_iid_t idxId) const {
  return getPhysical()->lookupIndex(idxId);
}

std::shared_ptr<Index> LogicalCollection::lookupIndex(
    VPackSlice const& info) const {
  if (!info.isObject()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return getPhysical()->lookupIndex(info);
}

std::shared_ptr<Index> LogicalCollection::createIndex(transaction::Methods* trx,
                                                      VPackSlice const& info,
                                                      bool& created) {
  return _physical->createIndex(trx, info, created);
}

/// @brief drops an index, including index file removal and replication
bool LogicalCollection::dropIndex(TRI_idx_iid_t iid) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(_vocbase);
#endif
  arangodb::aql::QueryCache::instance()->invalidate(_vocbase, name());
  return _physical->dropIndex(iid);
}

/// @brief Persist the connected physical collection.
///        This should be called AFTER the collection is successfully
///        created and only on Sinlge/DBServer
void LogicalCollection::persistPhysicalCollection() {
  // Coordinators are not allowed to have local collections!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  // We have not yet persisted this collection!
  TRI_ASSERT(getPhysical()->path().empty());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::string path = engine->createCollection(_vocbase, _cid, this);
  getPhysical()->setPath(path);
}

/// @brief Defer a callback to be executed when the collection
///        can be dropped. The callback is supposed to drop
///        the collection and it is guaranteed that no one is using
///        it at that moment.
void LogicalCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> callback) {
  _physical->deferDropCollection(callback);
}

/// @brief reads an element from the document collection
Result LogicalCollection::read(transaction::Methods* trx, StringRef const& key,
                               ManagedDocumentResult& result, bool lock) {
  return getPhysical()->read(trx, key, result, lock);
}

Result LogicalCollection::read(transaction::Methods* trx, arangodb::velocypack::Slice const& key,
            ManagedDocumentResult& result, bool lock) {
  return getPhysical()->read(trx, key, result, lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a truncate operation (note: currently this only clears
/// the read-cache
////////////////////////////////////////////////////////////////////////////////

void LogicalCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  getPhysical()->truncate(trx, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document or edge into the collection
////////////////////////////////////////////////////////////////////////////////

Result LogicalCollection::insert(transaction::Methods* trx,
                                 VPackSlice const slice,
                                 ManagedDocumentResult& result,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick, bool lock) {
  resultMarkerTick = 0;
  return getPhysical()->insert(trx, slice, result, options, resultMarkerTick,
                               lock);
}

/// @brief updates a document or edge in a collection
Result LogicalCollection::update(transaction::Methods* trx,
                                 VPackSlice const newSlice,
                                 ManagedDocumentResult& result,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                                 TRI_voc_rid_t& prevRev,
                                 ManagedDocumentResult& previous) {
  resultMarkerTick = 0;

  if (!newSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  prevRev = 0;

  TRI_voc_rid_t revisionId = 0;
  if (options.isRestore) {
    VPackSlice oldRev = TRI_ExtractRevisionIdAsSlice(newSlice);
    if (!oldRev.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_REV_BAD);
    }
    bool isOld;
    VPackValueLength l;
    char const* p = oldRev.getString(l);
    revisionId = TRI_StringToRid(p, l, isOld, false);
    if (isOld) {
      // Do not tolerate old revision IDs
      revisionId = TRI_HybridLogicalClock();
    }
  } else {
    revisionId = TRI_HybridLogicalClock();
  }

  VPackSlice key = newSlice.get(StaticStrings::KeyString);
  if (key.isNone()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  return getPhysical()->update(trx, newSlice, result, options, resultMarkerTick,
                               lock, prevRev, previous, revisionId, key);
}

/// @brief replaces a document or edge in a collection
Result LogicalCollection::replace(transaction::Methods* trx,
                                  VPackSlice const newSlice,
                                  ManagedDocumentResult& result,
                                  OperationOptions& options,
                                  TRI_voc_tick_t& resultMarkerTick, bool lock,
                                  TRI_voc_rid_t& prevRev,
                                  ManagedDocumentResult& previous) {
  resultMarkerTick = 0;

  if (!newSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  prevRev = 0;
  VPackSlice fromSlice;
  VPackSlice toSlice;

  if (type() == TRI_COL_TYPE_EDGE) {
    fromSlice = newSlice.get(StaticStrings::FromString);
    if (!fromSlice.isString()) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
    toSlice = newSlice.get(StaticStrings::ToString);
    if (!toSlice.isString()) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
  }

  TRI_voc_rid_t revisionId = 0;
  if (options.isRestore) {
    VPackSlice oldRev = TRI_ExtractRevisionIdAsSlice(newSlice);
    if (!oldRev.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_REV_BAD);
    }
    bool isOld;
    VPackValueLength l;
    char const* p = oldRev.getString(l);
    revisionId = TRI_StringToRid(p, l, isOld, false);
    if (isOld || revisionId == UINT64_MAX) {
      // Do not tolerate old revision ticks or invalid ones:
      revisionId = TRI_HybridLogicalClock();
    }
  } else {
    revisionId = TRI_HybridLogicalClock();
  }

  return getPhysical()->replace(trx, newSlice, result, options,
                                resultMarkerTick, lock, prevRev, previous,
                                revisionId, fromSlice, toSlice);
}

/// @brief removes a document or edge
Result LogicalCollection::remove(transaction::Methods* trx,
                                 VPackSlice const slice,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                                 TRI_voc_rid_t& prevRev,
                                 ManagedDocumentResult& previous) {
  resultMarkerTick = 0;

  TRI_voc_rid_t revisionId = 0;
  if (options.isRestore) {
    VPackSlice oldRev = TRI_ExtractRevisionIdAsSlice(slice);
    if (!oldRev.isString()) {
      revisionId = TRI_HybridLogicalClock();
    } else {
      bool isOld;
      VPackValueLength l;
      char const* p = oldRev.getString(l);
      revisionId = TRI_StringToRid(p, l, isOld, false);
      if (isOld || revisionId == UINT64_MAX) {
        // Do not tolerate old revisions or illegal ones
        revisionId = TRI_HybridLogicalClock();
      }
    }
  } else {
    revisionId = TRI_HybridLogicalClock();
  }

  return getPhysical()->remove(trx, slice, previous, options, resultMarkerTick,
                               lock, revisionId, prevRev);
}

bool LogicalCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  return getPhysical()->readDocument(trx, token, result);
}

bool LogicalCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 DocumentIdentifierToken const& token,
                                                 IndexIterator::DocumentCallback const& cb) {
  return getPhysical()->readDocumentWithCallback(trx, token, cb);
}

/// @brief a method to skip certain documents in AQL write operations,
/// this is only used in the enterprise edition for smart graphs
#ifndef USE_ENTERPRISE
bool LogicalCollection::skipForAqlWrite(arangodb::velocypack::Slice document,
                                        std::string const& key) const {
  return false;
}
#endif

bool LogicalCollection::isSatellite() const { return _replicationFactor == 0; }

// SECTION: Key Options
VPackSlice LogicalCollection::keyOptions() const {
  if (_keyOptions == nullptr) {
    return arangodb::basics::VelocyPackHelper::NullValue();
  }
  return VPackSlice(_keyOptions->data());
}

ChecksumResult LogicalCollection::checksum(bool withRevisions, bool withData) const {
  auto transactionContext =
    std::make_shared<transaction::StandaloneContext>(vocbase());
  SingleCollectionTransaction trx(transactionContext, cid(), AccessMode::Type::READ);

  Result res = trx.begin();

  if (!res.ok()) {
    return ChecksumResult(std::move(res));
  }

  trx.pinData(_cid); // will throw when it fails
  
  // get last tick
  LogicalCollection* collection = trx.documentCollection();
  auto physical = collection->getPhysical();
  TRI_ASSERT(physical != nullptr);
  std::string const revisionId = TRI_RidToString(physical->revision(&trx));
  uint64_t hash = 0;
        
  ManagedDocumentResult mmdr;
  trx.invokeOnAllElements(name(), [&hash, &withData, &withRevisions, &trx, &collection, &mmdr](DocumentIdentifierToken const& token) {
      if (collection->readDocument(&trx, token, mmdr)) {
      VPackSlice const slice(mmdr.vpack());

      uint64_t localHash = transaction::helpers::extractKeyFromDocument(slice).hashString(); 

      if (withRevisions) {
      localHash += transaction::helpers::extractRevSliceFromDocument(slice).hash();
      }

      if (withData) {
      // with data
      uint64_t const n = slice.length() ^ 0xf00ba44ba5;
      uint64_t seed = fasthash64_uint64(n, 0xdeadf054);

      for (auto const& it : VPackObjectIterator(slice, false)) {
      // loop over all attributes, but exclude _rev, _id and _key
      // _id is different for each collection anyway, _rev is covered by withRevisions, and _key
      // was already handled before
      VPackValueLength keyLength;
      char const* key = it.key.getString(keyLength);
      if (keyLength >= 3 && 
          key[0] == '_' &&
          ((keyLength == 3 && memcmp(key, "_id", 3) == 0) ||
           (keyLength == 4 && (memcmp(key, "_key", 4) == 0 || memcmp(key, "_rev", 4) == 0)))) {
        // exclude attribute
        continue;
      }

      localHash ^= it.key.hash(seed) ^ 0xba5befd00d; 
      localHash += it.value.normalizedHash(seed) ^ 0xd4129f526421; 
      }
      }

      hash ^= localHash;
      }
    return true;
  });

  trx.finish(res);

  std::string const hashString = std::to_string(hash);

  VPackBuilder b;
  {
    VPackObjectBuilder o(&b);
    b.add("checksum", VPackValue(hashString));
    b.add("revision", VPackValue(revisionId));
  }

  return ChecksumResult(std::move(b));
}

Result LogicalCollection::compareChecksums(VPackSlice checksumSlice, std::string const& referenceChecksum) const {
  if (!checksumSlice.isString()) {
    return Result(
      TRI_ERROR_REPLICATION_WRONG_CHECKSUM_FORMAT,
      std::string("Checksum must be a string but is ") + checksumSlice.typeName()
    );
  }

  auto checksum = checksumSlice.copyString();

  if (checksum != referenceChecksum) {
    return Result(
      TRI_ERROR_REPLICATION_WRONG_CHECKSUM,
      "'checksum' is wrong. Expected: "
        + referenceChecksum
        + ". Actual: " + checksum
    );
  }

  return Result();
}
