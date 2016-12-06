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
////////////////////////////////////////////////////////////////////////////////

#include "LogicalCollection.h"

#include "Basics/Barrier.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/Timers.h"
#include "Basics/ThreadPool.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Aql/QueryCache.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/RocksDBIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/RevisionCacheFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "Utils/Events.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/CollectionRevisionsCache.h"
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/IndexThreadFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/PhysicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/transaction.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"
#include "Wal/Slots.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

/// forward
int TRI_AddOperationTransaction(TRI_transaction_t*,
                                TRI_voc_rid_t,
                                arangodb::wal::DocumentOperation&, 
                                arangodb::wal::Marker const* marker,
                                bool&);

/// @brief helper struct for filling indexes
class IndexFiller {
 public:
  IndexFiller(arangodb::Transaction* trx, arangodb::LogicalCollection* collection,
              arangodb::Index* idx, std::function<void(int)> callback)
      : _trx(trx), _collection(collection), _idx(idx), _callback(callback) {}

  void operator()() {
    int res = TRI_ERROR_INTERNAL;
    TRI_ASSERT(_idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 

    try {
      res = _collection->fillIndex(_trx, _idx);
    } catch (...) {
    }

    _callback(res);
  }

 private:
  arangodb::Transaction* _trx;
  arangodb::LogicalCollection* _collection;
  arangodb::Index* _idx;
  std::function<void(int)> _callback;
};

namespace {

template <typename T>
static T ReadNumericValue(VPackSlice info, std::string const& name, T def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getNumericValue<T>(info, name.c_str(), def);
}

template <typename T, typename BaseType>
static T ReadNumericValue(VPackSlice info, std::string const& name, T def) {
  if (!info.isObject()) {
    return def;
  }
  // nice extra conversion required for Visual Studio pickyness
  return static_cast<T>(Helper::getNumericValue<BaseType>(info, name.c_str(), static_cast<BaseType>(def)));
}

static bool ReadBooleanValue(VPackSlice info, std::string const& name,
                             bool def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getBooleanValue(info, name.c_str(), def);
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

static std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> CopySliceValue(VPackSlice info,
    std::string const& name) {
  if (!info.isObject()) {
    return nullptr;
  }
  info = info.get(name);
  if (info.isNone()) {
    return nullptr;
  }
  return VPackBuilder::clone(info).steal();
}

// Creates an index object.
// It does not modify anything and does not insert things into
// the index.
// Is also save to use in cluster case.
static std::shared_ptr<Index> PrepareIndexFromSlice(VPackSlice info,
                                                    bool generateKey,
                                                    LogicalCollection* col,
                                                    bool isClusterConstructor = false) {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    if (generateKey) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid index type definition");
    }
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  std::shared_ptr<Index> newIdx;

  TRI_idx_iid_t iid = 0;
  value = info.get("id");
  if (value.isString()) {
    iid = basics::StringUtils::uint64(value.copyString());
  } else if (value.isNumber()) { 
    iid = Helper::getNumericValue<TRI_idx_iid_t>(info, "id", 0);
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot restore index without index identifier");
  }

  if (iid == 0 && !isClusterConstructor) {
    // Restore is not allowed to generate in id
    TRI_ASSERT(generateKey);
    iid = arangodb::Index::generateId();
  }
  
  switch (type) {
    case arangodb::Index::TRI_IDX_TYPE_UNKNOWN: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    case arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX: {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create primary index");
      }
      newIdx.reset(new arangodb::PrimaryIndex(col));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX: {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create edge index");
      }
      newIdx.reset(new arangodb::EdgeIndex(iid, col));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX:
    case arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX: {
      newIdx.reset(new arangodb::GeoIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_HASH_INDEX: {
      newIdx.reset(new arangodb::HashIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX: {
      newIdx.reset(new arangodb::SkiplistIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_ROCKSDB_INDEX: {
      newIdx.reset(new arangodb::RocksDBIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX: {
      newIdx.reset(new arangodb::FulltextIndex(iid, col, info));
      break;
    }
  }
  if (newIdx == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return newIdx;
};

}

/// @brief This the "copy" constructor used in the cluster
///        it is required to create objects that survive plan
///        modifications and can be freed
///        Can only be given to V8, cannot be used for functionality.
LogicalCollection::LogicalCollection(
    LogicalCollection const& other)
    : _internalVersion(0),
      _cid(other.cid()),
      _planId(other.planId()),
      _type(other.type()),
      _name(other.name()),
      _distributeShardsLike(other.distributeShardsLike()),
      _isSmart(other.isSmart()),
      _status(other.status()),
      _isLocal(false),
      _isDeleted(other._isDeleted),
      _doCompact(other.doCompact()),
      _isSystem(other.isSystem()),
      _isVolatile(other.isVolatile()),
      _waitForSync(other.waitForSync()),
      _journalSize(static_cast<TRI_voc_size_t>(other.journalSize())),
      _keyOptions(other._keyOptions),
      _version(other._version),
      _indexBuckets(other.indexBuckets()),
      _indexes(),
      _replicationFactor(other.replicationFactor()),
      _numberOfShards(other.numberOfShards()),
      _allowUserKeys(other.allowUserKeys()),
      _shardIds(new ShardMap()),  // Not needed
      _vocbase(other.vocbase()),
      _cleanupIndexes(0),
      _persistentIndexes(0),
      _physical(EngineSelectorFeature::ENGINE->createPhysicalCollection(this)),
      _revisionsCache(nullptr),
      _useSecondaryIndexes(true),
      _maxTick(0),
      _keyGenerator(),
      _nextCompactionStartIndex(0),
      _lastCompactionStatus(nullptr),
      _lastCompactionStamp(0.0),
      _uncollectedLogfileEntries(0),
      _isInitialIteration(false),
      _revisionError(false) {
  _keyGenerator.reset(KeyGenerator::factory(other.keyOptions()));
  
  // TODO Only DBServer? Is this correct?
  if (ServerState::instance()->isDBServer()) {
    _followers.reset(new FollowerInfo(this));
  }

  // Copy over index definitions
  _indexes.reserve(other._indexes.size());
  for (auto const& idx : other._indexes) {
    _indexes.emplace_back(idx);
  }

  setCompactionStatus("compaction not yet started");
}

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this collection.
LogicalCollection::LogicalCollection(TRI_vocbase_t* vocbase,
                                     VPackSlice const& info, bool isPhysical)
    : _internalVersion(0),
      _cid(ReadCid(info)),
      _planId(ReadPlanId(info, _cid)),
      _type(ReadNumericValue<TRI_col_type_e, int>(info, "type",
                                                  TRI_COL_TYPE_UNKNOWN)),
      _name(ReadStringValue(info, "name", "")),
      _distributeShardsLike(ReadStringValue(info, "distributeShardsLike", "")),
      _isSmart(ReadBooleanValue(info, "isSmart", false)),
      _status(ReadNumericValue<TRI_vocbase_col_status_e, int>(
          info, "status", TRI_VOC_COL_STATUS_CORRUPTED)),
      _isLocal(!ServerState::instance()->isCoordinator()),
      _isDeleted(ReadBooleanValue(info, "deleted", false)),
      _doCompact(ReadBooleanValue(info, "doCompact", true)),
      _isSystem(IsSystemName(_name) &&
                ReadBooleanValue(info, "isSystem", false)),
      _isVolatile(ReadBooleanValue(info, "isVolatile", false)),
      _waitForSync(ReadBooleanValue(info, "waitForSync", false)),
      _journalSize(ReadNumericValue<TRI_voc_size_t>(
          info, "maximalSize",  // Backwards compatibility. Agency uses
                                // journalSize. paramters.json uses maximalSize
          ReadNumericValue<TRI_voc_size_t>(info, "journalSize",
                                           TRI_JOURNAL_DEFAULT_SIZE))),
      _keyOptions(CopySliceValue(info, "keyOptions")),
      _version(ReadNumericValue<uint32_t>(info, "version", currentVersion())),
      _indexBuckets(ReadNumericValue<uint32_t>(
          info, "indexBuckets", DatabaseFeature::defaultIndexBuckets())),
      _replicationFactor(1),
      _numberOfShards(ReadNumericValue<size_t>(info, "numberOfShards", 1)),
      _allowUserKeys(ReadBooleanValue(info, "allowUserKeys", true)),
      _shardIds(new ShardMap()),
      _vocbase(vocbase),
      _cleanupIndexes(0),
      _persistentIndexes(0),
      _path(ReadStringValue(info, "path", "")),
      _physical(EngineSelectorFeature::ENGINE->createPhysicalCollection(this)),
      _revisionsCache(nullptr),
      _useSecondaryIndexes(true),
      _maxTick(0),
      _keyGenerator(),
      _nextCompactionStartIndex(0),
      _lastCompactionStatus(nullptr),
      _lastCompactionStamp(0.0),
      _uncollectedLogfileEntries(0),
      _isInitialIteration(false),
      _revisionError(false) {
      
  if (!IsAllowedName(info)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }
  
  if (_version < minimumVersion()) { 
    // collection is too "old"
    std::string errorMsg(std::string("collection '") + _name +
                         "' has a too old version. Please start the server "
                         "with the --database.auto-upgrade option.");

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, errorMsg);
  }


  if (_isVolatile && _waitForSync) {
    // Illegal collection configuration
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (_journalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "<properties>.journalSize too small");
  }

  VPackSlice shardKeysSlice = info.get("shardKeys");

  bool const isCluster = ServerState::instance()->isRunningInCluster();
  // Cluster only tests
  if (ServerState::instance()->isCoordinator()) {
    if ( (_numberOfShards == 0 && !_isSmart) || _numberOfShards > 1000) {
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
    auto replicationFactorSlice = info.get("replicationFactor");
    if (!replicationFactorSlice.isNone()) {
      bool isError = true;
      if (replicationFactorSlice.isString() && replicationFactorSlice.copyString() == "satellite") {
        _replicationFactor = 0;
        _numberOfShards = 1;
        _distributeShardsLike = "";
        isError = false;
      } else if (replicationFactorSlice.isNumber()) {
        _replicationFactor = replicationFactorSlice.getNumber<size_t>();
        // mop: only allow satellite collections to be created explicitly
        if (_replicationFactor > 0 || _replicationFactor <= 10) {
          isError = false;
        }
      }
      if (isError) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
            "invalid replicationFactor");
      }
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
              stripped = key.substr(0, key.size()-1);
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
        // This is translated to ["_key"]. In cluster-case this always was forbidden.
        _shardKeys.emplace_back(StaticStrings::KeyString);
      }
    }
  }

  if (_shardKeys.empty() || _shardKeys.size() > 8) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid number of shard keys");
  }



  _keyGenerator.reset(KeyGenerator::factory(info.get("keyOptions")));

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

  auto indexesSlice = info.get("indexes");
  if (indexesSlice.isArray()) {
    for (auto const& v : VPackArrayIterator(indexesSlice)) {
      if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                              false)) {
        // We have an error here.
        // Do not add index.
        // TODO Handle Properly
        continue;
      }

      auto idx = PrepareIndexFromSlice(v, false, this, true);

      if (isCluster) {
        addIndexCoordinator(idx, false);
      } else {
        addIndex(idx);
      }
    }
  }

  if (_indexes.empty()) {
    createInitialIndexes();
  }

  if (!ServerState::instance()->isCoordinator() && isPhysical) {
    // If we are not in the coordinator we need a path
    // to the physical data.
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    if (_path.empty()) {
      _path = engine->createCollection(_vocbase, _cid, this);
    }
  }

  int64_t count = ReadNumericValue<int64_t>(info, "count", -1);
  if (count != -1) {
    _physical->updateCount(count);
  }

  // TODO Only DBServer? Is this correct?
  if (ServerState::instance()->isDBServer()) {
    _followers.reset(new FollowerInfo(this));
  }
  
  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(_cid));

  setCompactionStatus("compaction not yet started");
}

LogicalCollection::~LogicalCollection() {}

bool LogicalCollection::IsAllowedName(VPackSlice parameters) {
  bool allowSystem = ReadBooleanValue(parameters, "isSystem", false);
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

void LogicalCollection::ensureRevisionsCache() {
  if (!_revisionsCache) {
    _revisionsCache.reset(new CollectionRevisionsCache(this, RevisionCacheFeature::ALLOCATOR));
  }
}

/// @brief checks if a collection name is allowed
/// Returns true if the name is allowed and false otherwise
bool LogicalCollection::IsAllowedName(bool allowSystem, std::string const& name) {
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

/// @brief whether or not a collection is fully collected
bool LogicalCollection::isFullyCollected() {
  int64_t uncollected = _uncollectedLogfileEntries.load();

  return (uncollected == 0);
}

void LogicalCollection::setNextCompactionStartIndex(size_t index) {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _nextCompactionStartIndex = index;
}

size_t LogicalCollection::getNextCompactionStartIndex() {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  return _nextCompactionStartIndex;
}

void LogicalCollection::setCompactionStatus(char const* reason) {
  TRI_ASSERT(reason != nullptr);
  
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _lastCompactionStatus = reason;
}
  
uint64_t LogicalCollection::numberDocuments() const {
  return primaryIndex()->size();
}

size_t LogicalCollection::journalSize() const {
  return _journalSize;
}

uint32_t LogicalCollection::internalVersion() const {
  return _internalVersion;
}

std::string LogicalCollection::cid_as_string() const {
  return basics::StringUtils::itoa(_cid);
}

TRI_voc_cid_t LogicalCollection::planId() const {
  return _planId;
}

TRI_col_type_e LogicalCollection::type() const {
  return _type;
}

std::string LogicalCollection::name() const {
  // TODO Activate this lock. Right now we have some locks outside.
  // READ_LOCKER(readLocker, _lock);
  return _name;
}

std::string const& LogicalCollection::distributeShardsLike() const {
  return _distributeShardsLike;
}

void LogicalCollection::distributeShardsLike(std::string const& cid) {
  _distributeShardsLike = cid;
}

std::string LogicalCollection::dbName() const {
  TRI_ASSERT(_vocbase != nullptr);
  return _vocbase->name();
}

std::string const& LogicalCollection::path() const {
  return _path;
}

TRI_vocbase_col_status_e LogicalCollection::status() const {
  return _status;
}

TRI_vocbase_col_status_e LogicalCollection::getStatusLocked() {
  READ_LOCKER(readLocker, _lock);
  return _status;
}
  
void LogicalCollection::executeWhileStatusLocked(std::function<void()> const& callback) {
  READ_LOCKER(readLocker, _lock);
  callback();
}

bool LogicalCollection::tryExecuteWhileStatusLocked(std::function<void()> const& callback) {
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
std::string LogicalCollection::statusString() {
  READ_LOCKER(readLocker, _lock);
  switch (_status) {
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

// SECTION: Properties
TRI_voc_rid_t LogicalCollection::revision() const {
  // TODO CoordinatorCase
  return _physical->revision();
}

bool LogicalCollection::isLocal() const {
  return _isLocal;
}

bool LogicalCollection::deleted() const {
  return _isDeleted;
}

bool LogicalCollection::doCompact() const {
  return _doCompact;
}

bool LogicalCollection::isSystem() const {
  return _isSystem;
}

bool LogicalCollection::isVolatile() const {
  return _isVolatile;
}

bool LogicalCollection::waitForSync() const {
  return _waitForSync;
}

bool LogicalCollection::isSmart() const {
  return _isSmart;
}

std::unique_ptr<FollowerInfo> const& LogicalCollection::followers() const {
  return _followers;
}

void LogicalCollection::setDeleted(bool newValue) {
  _isDeleted = newValue;
}

/// @brief update statistics for a collection
void LogicalCollection::setRevision(TRI_voc_rid_t revision, bool force) {
  if (revision > 0) {
    // TODO Is this still true?
    /// note: Old version the write-lock for the collection must be held to call this
    _physical->setRevision(revision, force);
  }
}

// SECTION: Key Options
VPackSlice LogicalCollection::keyOptions() const {
  if (_keyOptions == nullptr) {
    return Helper::NullValue();
  }
  return VPackSlice(_keyOptions->data());
}

// SECTION: Indexes
uint32_t LogicalCollection::indexBuckets() const {
  return _indexBuckets;
}

std::vector<std::shared_ptr<arangodb::Index>> const&
LogicalCollection::getIndexes() const {
  return _indexes;
}

/// @brief return the primary index
// WARNING: Make sure that this LogicalCollection Instance
// is somehow protected. If it goes out of all scopes
// or it's indexes are freed the pointer returned will get invalidated.
arangodb::PrimaryIndex* LogicalCollection::primaryIndex() const {
  TRI_ASSERT(!_indexes.empty());
  TRI_ASSERT(_indexes[0]->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  // the primary index must be the index at position #0
  return static_cast<arangodb::PrimaryIndex*>(_indexes[0].get());
}

void LogicalCollection::getIndexesVPack(VPackBuilder& result,
                                        bool withFigures) const {
  result.openArray();
  for (auto const& idx : _indexes) {
    result.openObject();
    idx->toVelocyPack(result, withFigures);
    result.close();
  }
  result.close();
}


// SECTION: Replication
int LogicalCollection::replicationFactor() const {
  return static_cast<int>(_replicationFactor);
}

// SECTION: Sharding
int LogicalCollection::numberOfShards() const {
  return static_cast<int>(_numberOfShards);
}

bool LogicalCollection::allowUserKeys() const {
  return _allowUserKeys;
}

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

void LogicalCollection::setShardMap(std::shared_ptr<ShardMap>& map) {
  _shardIds = map;
}

// SECTION: Modification Functions

// asks the storage engine to rename the collection to the given name
// and persist the renaming info. It is guaranteed by the server 
// that no other active collection with the same name and id exists in the same
// database when this function is called. If this operation fails somewhere in 
// the middle, the storage engine is required to fully revert the rename operation
// and throw only then, so that subsequent collection creation/rename requests will 
// not fail. the WAL entry for the rename will be written *after* the call
// to "renameCollection" returns

int LogicalCollection::rename(std::string const& newName) {
  // Should only be called from inside vocbase.
  // Otherwise caching is destroyed.
  TRI_ASSERT(!ServerState::instance()->isCoordinator()); // NOT YET IMPLEMENTED

  WRITE_LOCKER_EVENTUAL(locker, _lock, 1000);

  // Check for illeagal states.
  switch (_status) {
    case TRI_VOC_COL_STATUS_CORRUPTED:
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    case TRI_VOC_COL_STATUS_DELETED:
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    default:
      // Fall through intentional
      break;
  }

  // Check for duplicate name
  auto other = _vocbase->lookupCollection(newName);
  if (other != nullptr) {
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
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

  // CHECK if this ordering is okay. Before change the version was increased after swapping in vocbase mapping.
  increaseInternalVersion();
  return TRI_ERROR_NO_ERROR;
}

int LogicalCollection::close() {
  // This was unload() in 3.0
  auto primIdx = primaryIndex();
  auto idxSize = primIdx->size();

  if (!_isDeleted &&
      _physical->initialCount() != static_cast<int64_t>(idxSize)) {
    _physical->updateCount(idxSize);

    // save new "count" value
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    bool const doSync = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->forceSyncProperties();
    engine->changeCollection(_vocbase, _cid, this, doSync);
  }

  // We also have to unload the indexes.
  for (auto& idx : _indexes) {
    idx->unload();
  }

  if (_revisionsCache != nullptr) {
    _revisionsCache->clear();
  }

  return getPhysical()->close();
}

void LogicalCollection::unload() {
  if (_revisionsCache != nullptr) {
    _revisionsCache->closeWriteChunk();
  }
}

void LogicalCollection::drop() {
  if (_revisionsCache != nullptr) {
    _revisionsCache->clear();
  }

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->dropCollection(_vocbase, this);
  _isDeleted = true;

  // save some memory by freeing the revisions cache and indexes
  _keyGenerator.reset();
  _revisionsCache.reset(); 
  _indexes.clear();
  _physical.reset();
}

void LogicalCollection::setStatus(TRI_vocbase_col_status_e status) {
  _status = status;

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    increaseInternalVersion(); 
  } 
}

void LogicalCollection::toVelocyPackForAgency(VPackBuilder& result) {
  _status = TRI_VOC_COL_STATUS_LOADED;
  result.openObject();
  toVelocyPackInObject(result);

  result.close(); // Base Object
}

void LogicalCollection::toVelocyPack(VPackBuilder& result, bool withPath) const {
  result.openObject();
  toVelocyPackInObject(result);
  result.add("cid", VPackValue(std::to_string(_cid))); // export cid for compatibility, too
  result.add("planId", VPackValue(std::to_string(_planId))); // export planId for cluster
  result.add("version", VPackValue(_version)); 
  result.add("count", VPackValue(_physical->initialCount()));

  if (withPath) {
    result.add("path", VPackValue(_path));
  }
  result.add("allowUserKeys", VPackValue(_allowUserKeys));

  result.close();
}

// Internal helper that inserts VPack info into an existing object and leaves the object open
void LogicalCollection::toVelocyPackInObject(VPackBuilder& result) const {
  result.add("id", VPackValue(std::to_string(_cid)));
  result.add("name", VPackValue(_name));
  result.add("type", VPackValue(static_cast<int>(_type)));
  result.add("status", VPackValue(_status));
  result.add("deleted", VPackValue(_isDeleted));
  result.add("doCompact", VPackValue(_doCompact));
  result.add("isSystem", VPackValue(_isSystem));
  result.add("isVolatile", VPackValue(_isVolatile));
  result.add("waitForSync", VPackValue(_waitForSync));
  result.add("journalSize", VPackValue(_journalSize));
  result.add("indexBuckets", VPackValue(_indexBuckets));
  result.add("replicationFactor", VPackValue(_replicationFactor));
  result.add("numberOfShards", VPackValue(_numberOfShards));
  if (!_distributeShardsLike.empty()) {
    result.add("distributeShardsLike", VPackValue(_distributeShardsLike));
  }
  
  if (_keyGenerator != nullptr) {
    result.add(VPackValue("keyOptions"));
    result.openObject();
    _keyGenerator->toVelocyPack(result);
    result.close();
  }

  result.add(VPackValue("shardKeys"));
  result.openArray();
  for (auto const& key : _shardKeys) {
    result.add(VPackValue(key));
  }
  result.close(); // shardKeys

  result.add(VPackValue("shards"));
  result.openObject();
  for (auto const& shards : *_shardIds) {
    result.add(VPackValue(shards.first));
    result.openArray();
    for (auto const& servers : shards.second) {
      result.add(VPackValue(servers));
    }
    result.close(); // server array
  }
  result.close(); // shards

  result.add(VPackValue("indexes"));
  getIndexesVPack(result, false);
}

void LogicalCollection::toVelocyPack(VPackBuilder& builder, bool includeIndexes,
                                     TRI_voc_tick_t maxTick) {
  TRI_ASSERT(!builder.isClosed());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getCollectionInfo(_vocbase, _cid, builder, includeIndexes, maxTick);
}

void LogicalCollection::increaseInternalVersion() {
  ++_internalVersion;
}

int LogicalCollection::update(VPackSlice const& slice, bool doSync) {
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

  // some basic validation...      
  if (isVolatile() &&
    arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "waitForSync", waitForSync())) {
    // the combination of waitForSync and isVolatile makes no sense
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (isVolatile() != arangodb::basics::VelocyPackHelper::getBooleanValue(
         slice, "isVolatile", isVolatile())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
        "isVolatile option cannot be changed at runtime");
  }

  uint32_t tmp =
      arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
          slice, "indexBuckets", 2 /*Just for validation, this default Value passes*/);
  if (tmp == 0 || tmp > 1024) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
       "indexBuckets must be a two-power between 1 and 1024");
  }
  // end of validation

  _doCompact = Helper::getBooleanValue(slice, "doCompact", _doCompact);
  _waitForSync = Helper::getBooleanValue(slice, "waitForSync", _waitForSync);
  if (slice.hasKey("journalSize")) {
    _journalSize = Helper::getNumericValue<TRI_voc_size_t>(slice, "journalSize",
                                                           _journalSize);
  } else {
    _journalSize = Helper::getNumericValue<TRI_voc_size_t>(slice, "maximalSize",
                                                           _journalSize);
  }
  _indexBuckets =
      Helper::getNumericValue<uint32_t>(slice, "indexBuckets", _indexBuckets);

  if (!_isLocal) {
    // We need to inform the cluster as well
    return ClusterInfo::instance()->setCollectionPropertiesCoordinator(
        _vocbase->name(), cid_as_string(), this);
  }

  int64_t count = arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
      slice, "count", _physical->initialCount());
  if (count != _physical->initialCount()) {
    _physical->updateCount(count);
  }
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->changeCollection(_vocbase, _cid, this, doSync);

  return TRI_ERROR_NO_ERROR;
}

/// @brief return the figures for a collection
std::shared_ptr<arangodb::velocypack::Builder> LogicalCollection::figures() {
  auto builder = std::make_shared<VPackBuilder>();
  
  if (ServerState::instance()->isCoordinator()) {
    builder->openObject();
    builder->close();
    int res = figuresOnCoordinator(dbName(), cid_as_string(), builder); 
    
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  } else {
    builder->openObject();

    // add index information
    size_t sizeIndexes = getPhysical()->memory();
    size_t numIndexes = 0;
    for (auto const& idx : _indexes) {
      sizeIndexes += static_cast<size_t>(idx->memory());
      ++numIndexes;
    }

    builder->add("indexes", VPackValue(VPackValueType::Object));
    builder->add("count", VPackValue(numIndexes));
    builder->add("size", VPackValue(sizeIndexes));
    builder->close(); // indexes

    builder->add("lastTick", VPackValue(_maxTick));
    builder->add("uncollectedLogfileEntries", VPackValue(_uncollectedLogfileEntries));

    // fills in compaction status
    char const* lastCompactionStatus = "-";
    char lastCompactionStampString[21];
    lastCompactionStampString[0] = '-';
    lastCompactionStampString[1] = '\0';
    
    double lastCompactionStamp;

    {
      MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
      lastCompactionStatus = _lastCompactionStatus;
      lastCompactionStamp = _lastCompactionStamp;
    }

    if (lastCompactionStatus != nullptr) {
      struct tm tb;
      time_t tt = static_cast<time_t>(lastCompactionStamp);
      TRI_gmtime(tt, &tb);
      strftime(&lastCompactionStampString[0], sizeof(lastCompactionStampString), "%Y-%m-%dT%H:%M:%SZ", &tb);
    }
  
    builder->add("compactionStatus", VPackValue(VPackValueType::Object));
    builder->add("message", VPackValue(lastCompactionStatus));
    builder->add("time", VPackValue(&lastCompactionStampString[0]));
    builder->close(); // compactionStatus

    // add engine-specific figures
    getPhysical()->figures(builder);
    builder->close();
  }

  return builder;
}

/// @brief opens an existing collection
void LogicalCollection::open(bool ignoreErrors) {
  ensureRevisionsCache();
  
  VPackBuilder builder;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getCollectionInfo(_vocbase, cid(), builder, true, 0);

  VPackSlice initialCount = builder.slice().get(std::vector<std::string>({ "parameters", "count" }));
  if (initialCount.isNumber()) {
    int64_t count = initialCount.getNumber<int64_t>();
    if (count > 0) {
      _physical->updateCount(count);
    }
  }
  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "open-document-collection { collection: " << _vocbase->name() << "/"
      << _name << " }";

  int res = openWorker(ignoreErrors);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("cannot open document collection from path '") + path() + "': " + TRI_errno_string(res));
  }

  arangodb::SingleCollectionTransaction trx(
      arangodb::StandaloneTransactionContext::Create(_vocbase),
      cid(), TRI_TRANSACTION_WRITE);

  // build the primary index
  double startIterate = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "iterate-markers { collection: " << _vocbase->name() << "/"
      << _name << " }";

  _revisionsCache->allowInvalidation(false);
  _isInitialIteration = true;

  // iterate over all markers of the collection
  res = getPhysical()->iterateMarkersOnLoad(&trx);

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::FIXED(TRI_microtime() - startIterate) << " s, iterate-markers { collection: " << _vocbase->name() << "/" << _name << " }";

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("cannot iterate data of document collection: ") + TRI_errno_string(res));
  }
  
  _isInitialIteration = false;

  // build the indexes meta-data, but do not fill the indexes yet
  {
    auto old = useSecondaryIndexes();

    // turn filling of secondary indexes off. we're now only interested in getting
    // the indexes' definition. we'll fill them below ourselves.
    useSecondaryIndexes(false);

    try {
      detectIndexes(&trx);
      useSecondaryIndexes(old);
    } catch (basics::Exception const& ex) {
      useSecondaryIndexes(old);
      THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), std::string("cannot initialize collection indexes: ") + ex.what());
    } catch (std::exception const& ex) {
      useSecondaryIndexes(old);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("cannot initialize collection indexes: ") + ex.what());
    } catch (...) {
      useSecondaryIndexes(old);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot initialize collection indexes: unknown exception");
    }
  }

  if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    // build the index structures, and fill the indexes
    fillIndexes(&trx);
  }
  
  _revisionsCache->allowInvalidation(true);

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, open-document-collection { collection: " << _vocbase->name() << "/"
      << _name << " }";

  // successfully opened collection. now adjust version number
  if (_version != VERSION_31 && 
      !_revisionError &&
      application_features::ApplicationServer::server->getFeature<DatabaseFeature>("Database")->check30Revisions()) {
    _version = VERSION_31;
    bool const doSync =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database")
            ->forceSyncProperties();
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    engine->changeCollection(_vocbase, _cid, this, doSync);
  }
  
  TRI_UpdateTickServer(_cid);
}

/// @brief opens an existing collection
int LogicalCollection::openWorker(bool ignoreErrors) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "open-collection { collection: " << _vocbase->name() << "/" << name() << " }";

  try {
    // check for journals and datafiles
    int res = engine->openCollection(_vocbase, this, ignoreErrors);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(DEBUG) << "cannot open '" << _path << "', check failed";
      return TRI_ERROR_INTERNAL;
    }

    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "[timer] " << Logger::FIXED(TRI_microtime() - start)
        << " s, open-collection { collection: " << _vocbase->name() << "/"
        << name() << " }";

    return TRI_ERROR_NO_ERROR;
  } catch (basics::Exception const& ex) {
    LOG(ERR) << "cannot load collection parameter file '" << _path << "': " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG(ERR) << "cannot load collection parameter file '" << _path << "': " << ex.what();
    return TRI_ERROR_INTERNAL;
  }
}

/// SECTION Indexes

std::shared_ptr<Index> LogicalCollection::lookupIndex(TRI_idx_iid_t idxId) const {
  for (auto const& idx : _indexes) {
    if (idx->id() == idxId) {
      return idx;
    }
  }
  return nullptr;
}

std::shared_ptr<Index> LogicalCollection::lookupIndex(VPackSlice const& info) const {
  if (!info.isObject()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  for (auto const& idx : _indexes) {
    if (idx->type() == type) {
      // Only check relevant indices
      if (idx->matchesDefinition(info)) {
        // We found an index for this definition.
        return idx;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<Index> LogicalCollection::createIndex(Transaction* trx,
                                                      VPackSlice const& info,
                                                      bool& created) {
  // TODO Get LOCK for the vocbase

  auto idx = lookupIndex(info);
  if (idx != nullptr) {
    created = false;
    // We already have this index.
    // Should we throw instead?
    return idx;
  }

  // We are sure that we do not have an index of this type.
  // We also hold the lock.
  // Create it
 
  idx = PrepareIndexFromSlice(info, true, this);
  TRI_ASSERT(idx != nullptr);
  if (ServerState::instance()->isCoordinator()) {
    // In the coordinator case we do not fill the index
    // We only inform the others.
    addIndexCoordinator(idx, true);
    created = true;
    return idx;
  }

  TRI_ASSERT(idx.get()->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 
  int res = fillIndex(trx, idx.get(), false);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
 
  bool const writeMarker = !arangodb::wal::LogfileManager::instance()->isInRecovery();
  res = saveIndex(idx.get(), writeMarker);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  // Until here no harm is done if sth fails. The shared ptr will clean up. if left before

  addIndex(idx);
  created = true;
  return idx;
}

int LogicalCollection::restoreIndex(Transaction* trx, VPackSlice const& info,
                                    std::shared_ptr<arangodb::Index>& idx) {
  // The coordinator can never get into this state!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  idx.reset(); // Clear it to make sure.
  if (!info.isObject()) {
    return TRI_ERROR_INTERNAL;
  }
  
  // We create a new Index object to make sure that the index
  // is not handed out except for a successful case.
  std::shared_ptr<Index> newIdx;
  try {
    newIdx = PrepareIndexFromSlice(info, false, this);
  } catch (arangodb::basics::Exception const& e) {
    // Something with index creation went wrong.
    // Just report.
    return e.code();
  }
  TRI_ASSERT(newIdx != nullptr);

  TRI_UpdateTickServer(newIdx->id());

  TRI_ASSERT(newIdx.get()->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 
  int res = fillIndex(trx, newIdx.get());

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  addIndex(newIdx);
  idx = newIdx;
  return TRI_ERROR_NO_ERROR;
}

/// @brief saves an index
int LogicalCollection::saveIndex(arangodb::Index* idx, bool writeMarker) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = idx->toVelocyPack(false);
  } catch (...) {
    LOG(ERR) << "cannot save index definition";
    return TRI_ERROR_INTERNAL;
  }
  if (builder == nullptr) {
    LOG(ERR) << "cannot save index definition";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->createIndex(_vocbase, cid(), idx->id(), builder->slice());

  if (!writeMarker) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::CollectionMarker marker(TRI_DF_MARKER_VPACK_CREATE_INDEX,
                                           _vocbase->id(), cid(),
                                           builder->slice());

    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // TODO: what to do here?
  return res;
}

/// @brief removes an index by id
bool LogicalCollection::removeIndex(TRI_idx_iid_t iid) {
  size_t const n = _indexes.size();

  for (size_t i = 0; i < n; ++i) {
    auto idx = _indexes[i];

    if (!idx->canBeDropped()) {
      continue;
    }

    if (idx->id() == iid) {
      // found!
      idx->drop();

      _indexes.erase(_indexes.begin() + i);

      // update statistics
      if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        --_cleanupIndexes;
      }
      if (idx->isPersistent()) {
        --_persistentIndexes;
      }

      return true;
    }
  }

  // not found
  return false;
}



/// @brief drops an index, including index file removal and replication
bool LogicalCollection::dropIndex(TRI_idx_iid_t iid, bool writeMarker) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (iid == 0) {
    // invalid index id or primary index
    events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
    return true;
  }

  {
    arangodb::aql::QueryCache::instance()->invalidate(
        _vocbase, name());
    if (!removeIndex(iid)) {
      // We tried to remove an index that does not exist
      events::DropIndex("", std::to_string(iid), TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
      return false;
    }
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->dropIndex(_vocbase, cid(), iid);

  if (writeMarker) {
    int res = TRI_ERROR_NO_ERROR;

    try {
      VPackBuilder markerBuilder;
      markerBuilder.openObject();
      markerBuilder.add("id", VPackValue(std::to_string(iid)));
      markerBuilder.close();

      arangodb::wal::CollectionMarker marker(TRI_DF_MARKER_VPACK_DROP_INDEX,
                                             _vocbase->id(), cid(),
                                             markerBuilder.slice());

      arangodb::wal::SlotInfoCopy slotInfo =
          arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                      false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
      return true;
    } catch (basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    LOG(WARN) << "could not save index drop marker in log: " << TRI_errno_string(res);
    events::DropIndex("", std::to_string(iid), res);
    // TODO: what to do here?
  }

  return true;
}

/// @brief creates the initial indexes for the collection
void LogicalCollection::createInitialIndexes() {
  // TODO Properly fix this. The outside should make sure that only NEW collections
  // try to create the indexes.
  if (!_indexes.empty()) {
    return;
  }

  // create primary index
  auto primaryIndex = std::make_shared<arangodb::PrimaryIndex>(this);
  addIndex(primaryIndex);

  // create edges index
  if (_type == TRI_COL_TYPE_EDGE) {
    auto edgeIndex = std::make_shared<arangodb::EdgeIndex>(1, this);

    addIndex(edgeIndex);
  }
}

/// @brief iterator for index open
bool LogicalCollection::openIndex(VPackSlice const& description, arangodb::Transaction* trx) {
  // VelocyPack must be an index description
  if (!description.isObject()) {
    return false;
  }

  bool unused = false;
  auto idx = createIndex(trx, description, unused);

  if (idx == nullptr) {
    // error was already printed if we get here
    return false;
  }

  return true;
}

/// @brief enumerate all indexes of the collection, but don't fill them yet
int LogicalCollection::detectIndexes(arangodb::Transaction* trx) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  VPackBuilder builder;
  engine->getCollectionInfo(_vocbase, _cid, builder, true, UINT64_MAX);

  // iterate over all index files
  for (auto const& it : VPackArrayIterator(builder.slice().get("indexes"))) {
    bool ok = openIndex(it, trx);

    if (!ok) {
      LOG(ERR) << "cannot load index for collection '" << name() << "'";
    }
  }

  return TRI_ERROR_NO_ERROR;
}

int LogicalCollection::fillIndexes(arangodb::Transaction* trx) {
  // distribute the work to index threads plus this thread
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  size_t const n = _indexes.size();

  if (n == 1) {
    return TRI_ERROR_NO_ERROR;
  }

  double start = TRI_microtime();

  // only log performance infos for indexes with more than this number of
  // entries
  static size_t const NotificationSizeThreshold = 131072;
  auto primaryIndex = this->primaryIndex();

  if (primaryIndex->size() > NotificationSizeThreshold) {
    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "fill-indexes-document-collection { collection: "
        << _vocbase->name() << "/" << name()
        << " }, indexes: " << (n - 1);
  }

  TRI_ASSERT(n > 1);

  std::atomic<int> result(TRI_ERROR_NO_ERROR);

  {
    arangodb::basics::Barrier barrier(n - 1);

    auto indexPool = application_features::ApplicationServer::getFeature<IndexThreadFeature>("IndexThread")->getThreadPool();

    auto callback = [&barrier, &result](int res) -> void {
      // update the error code
      if (res != TRI_ERROR_NO_ERROR) {
        int expected = TRI_ERROR_NO_ERROR;
        result.compare_exchange_strong(expected, res,
                                       std::memory_order_acquire);
      }

      barrier.join();
    };

    // now actually fill the secondary indexes
    for (size_t i = 1; i < n; ++i) {
      auto idx = _indexes[i];
      TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 

      // index threads must come first, otherwise this thread will block the
      // loop and
      // prevent distribution to threads
      if (indexPool != nullptr && i != (n - 1)) {
        try {
          // move task into thread pool
          IndexFiller indexTask(trx, this, idx.get(), callback);

          static_cast<arangodb::basics::ThreadPool*>(indexPool)
              ->enqueue(indexTask);
        } catch (...) {
          // set error code
          int expected = TRI_ERROR_NO_ERROR;
          result.compare_exchange_strong(expected, TRI_ERROR_INTERNAL,
                                         std::memory_order_acquire);

          barrier.join();
        }
      } else {
        // fill index in this thread
        int res;

        try {
          res = fillIndex(trx, idx.get());
        } catch (...) {
          res = TRI_ERROR_INTERNAL;
        }

        if (res != TRI_ERROR_NO_ERROR) {
          int expected = TRI_ERROR_NO_ERROR;
          result.compare_exchange_strong(expected, res,
                                         std::memory_order_acquire);
        }

        barrier.join();
      }
    }

    // barrier waits here until all threads have joined
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, fill-indexes-document-collection { collection: "
      << _vocbase->name() << "/" << name()
      << " }, indexes: " << (n - 1);

  return result.load();
}

void LogicalCollection::addIndex(std::shared_ptr<arangodb::Index> idx) {
  // primary index must be added at position 0
  TRI_ASSERT(idx->type() != arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX || _indexes.empty());
      
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(idx->id()));

  _indexes.emplace_back(idx);

  // update statistics
  if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    ++_cleanupIndexes;
  }
  if (idx->isPersistent()) {
    ++_persistentIndexes;
  }
}

void LogicalCollection::addIndexCoordinator(std::shared_ptr<arangodb::Index> idx,
                                            bool distribute) {
  _indexes.emplace_back(idx);
  if (distribute) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

/// @brief garbage-collect a collection's indexes
int LogicalCollection::cleanupIndexes() {
  int res = TRI_ERROR_NO_ERROR;

  // cleaning indexes is expensive, so only do it if the flag is set for the
  // collection
  if (_cleanupIndexes > 0) {
    WRITE_LOCKER(writeLocker, _idxLock);

    for (auto& idx : _indexes) {
      if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        res = idx->cleanup();

        if (res != TRI_ERROR_NO_ERROR) {
          break;
        }
      }
    }
  }

  return res;
}

/// @brief reads an element from the document collection
int LogicalCollection::read(Transaction* trx, std::string const& key,
                            ManagedDocumentResult& result, bool lock) {
  return read(trx, StringRef(key.c_str(), key.size()), result, lock);
}

int LogicalCollection::read(Transaction* trx, StringRef const& key,
                            ManagedDocumentResult& result, bool lock) {
  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
  VPackSlice slice = builder->slice();

  TRI_IF_FAILURE("ReadDocumentNoLock") {
    // test what happens if no lock can be acquired
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("ReadDocumentNoLockExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  bool const useDeadlockDetector = (lock && !trx->isSingleOperationTransaction());
  CollectionReadLocker collectionLocker(this, useDeadlockDetector, lock);

  int res = lookupDocument(trx, slice, result);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // we found a document
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a truncate operation (note: currently this only clears
/// the read-cache
////////////////////////////////////////////////////////////////////////////////

int LogicalCollection::truncate(Transaction* trx) {
  TRI_ASSERT(_revisionsCache);
  _revisionsCache->clear();
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document or edge into the collection
////////////////////////////////////////////////////////////////////////////////

int LogicalCollection::insert(Transaction* trx, VPackSlice const slice,
                              ManagedDocumentResult& result, OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock) {
  resultMarkerTick = 0;
  VPackSlice fromSlice;
  VPackSlice toSlice;
    
  bool const isEdgeCollection = (_type == TRI_COL_TYPE_EDGE);

  if (isEdgeCollection) {
    // _from:
    fromSlice = slice.get(StaticStrings::FromString);
    if (!fromSlice.isString()) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    VPackValueLength len;
    char const* docId = fromSlice.getString(len);
    size_t split;
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len), &split)) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    // _to:
    toSlice = slice.get(StaticStrings::ToString);
    if (!toSlice.isString()) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    docId = toSlice.getString(len);
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len), &split)) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
  }

  TransactionBuilderLeaser builder(trx);
  VPackSlice newSlice;
  int res = TRI_ERROR_NO_ERROR;
  if (options.recoveryMarker == nullptr) {
    TIMER_START(TRANSACTION_NEW_OBJECT_FOR_INSERT);
    res = newObjectForInsert(trx, slice, fromSlice, toSlice, isEdgeCollection, *builder.get(), options.isRestore);
    TIMER_STOP(TRANSACTION_NEW_OBJECT_FOR_INSERT);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    newSlice = builder->slice();
  } else {
    TRI_ASSERT(slice.isObject());
    // we can get away with the fast hash function here, as key values are 
    // restricted to strings
    newSlice = slice;
  }
    
  // create marker
  arangodb::wal::CrudMarker insertMarker(TRI_DF_MARKER_VPACK_DOCUMENT, TRI_MarkerIdTransaction(trx->getInternals()), newSlice);

  arangodb::wal::Marker const* marker;
  if (options.recoveryMarker == nullptr) {
    marker = &insertMarker;
  } else {
    marker = options.recoveryMarker;
  }

  // now insert into indexes
  TRI_IF_FAILURE("InsertDocumentNoLock") {
    // test what happens if no lock can be acquired
    return TRI_ERROR_DEBUG;
  }

  arangodb::wal::DocumentOperation operation(this, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  TRI_IF_FAILURE("InsertDocumentNoHeader") {
    // test what happens if no header can be acquired
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("InsertDocumentNoHeaderExcept") {
    // test what happens if no header can be acquired
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  
  TRI_voc_rid_t revisionId = Transaction::extractRevFromDocument(newSlice); 
  VPackSlice doc(marker->vpack());
  operation.setRevisions(DocumentDescriptor(), DocumentDescriptor(revisionId, doc.begin()));
 
  { 
    // use lock 
    bool const useDeadlockDetector = (lock && !trx->isSingleOperationTransaction());
    arangodb::CollectionWriteLocker collectionLocker(this, useDeadlockDetector, lock);

    try {
      insertRevision(revisionId, marker->vpack(), 0, true);

      // insert into indexes
      res = insertDocument(trx, revisionId, doc, operation, marker, options.waitForSync);
    } catch (basics::Exception const& ex) {
      res = ex.code();
    } catch (std::bad_alloc const&) {
      res = TRI_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert(trx);
    } else {
      readRevision(trx, result, revisionId);

      // store the tick that was used for writing the document        
      resultMarkerTick = operation.tick();
    }
  }
      
  return res;
}

/// @brief updates a document or edge in a collection
int LogicalCollection::update(Transaction* trx, VPackSlice const newSlice,
                              ManagedDocumentResult& result, OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous) {
  resultMarkerTick = 0;

  if (!newSlice.isObject()) {
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }
 
  prevRev = 0;

  TRI_voc_rid_t revisionId = 0;
  if (options.isRestore) {
    VPackSlice oldRev = TRI_ExtractRevisionIdAsSlice(newSlice);
    if (!oldRev.isString()) {
      return TRI_ERROR_ARANGO_DOCUMENT_REV_BAD;
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
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }
  
  bool const isEdgeCollection = (type() == TRI_COL_TYPE_EDGE);
  
  TRI_IF_FAILURE("UpdateDocumentNoLock") { return TRI_ERROR_DEBUG; }

  bool const useDeadlockDetector = (lock && !trx->isSingleOperationTransaction());
  arangodb::CollectionWriteLocker collectionLocker(this, useDeadlockDetector, lock);
  
  // get the previous revision
  int res = lookupDocument(trx, key, previous);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  uint8_t const* vpack = previous.vpack();
  VPackSlice oldDoc(vpack);
  TRI_voc_rid_t oldRevisionId = Transaction::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  TRI_IF_FAILURE("UpdateDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("UpdateDocumentNoMarkerExcept") {
    // test what happens when no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // Check old revision:
  if (!options.ignoreRevs) {
    TRI_voc_rid_t expectedRev = 0;
    if (newSlice.isObject()) {
      expectedRev = TRI_ExtractRevisionId(newSlice);
    }
    int res = checkRevision(trx, expectedRev, prevRev);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  if (newSlice.length() <= 1) {
    // no need to do anything
    result = previous;
    return TRI_ERROR_NO_ERROR;
  }

  // merge old and new values 
  TransactionBuilderLeaser builder(trx);
  if (options.recoveryMarker == nullptr) {
    mergeObjectsForUpdate(
      trx, oldDoc, newSlice, isEdgeCollection,
      TRI_RidToString(revisionId), options.mergeObjects, options.keepNull,
      *builder.get());

    if (ServerState::isDBServer(trx->serverRole())) {
      // Need to check that no sharding keys have changed:
      if (arangodb::shardKeysChanged(
              _vocbase->name(),
              trx->resolver()->getCollectionNameCluster(planId()),
              oldDoc, builder->slice(), false)) {
        return TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
      }
    }
  }

  // create marker
  arangodb::wal::CrudMarker updateMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      TRI_MarkerIdTransaction(trx->getInternals()), builder->slice());

  arangodb::wal::Marker const* marker;
  if (options.recoveryMarker == nullptr) {
    marker = &updateMarker;
  } else {
    marker = options.recoveryMarker;
  }
  
  VPackSlice const newDoc(marker->vpack());
  
  arangodb::wal::DocumentOperation operation(this, TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  
  try {
    insertRevision(revisionId, marker->vpack(), 0, true);
    operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()), DocumentDescriptor(revisionId, newDoc.begin()));

    res = updateDocument(trx, oldRevisionId, oldDoc, revisionId, newDoc, operation, marker, options.waitForSync);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    operation.revert(trx);
  } else {
    readRevision(trx, result, revisionId);

    if (options.waitForSync) {
      // store the tick that was used for writing the new document        
      resultMarkerTick = operation.tick();
    }
  }
  
  return res;
}

/// @brief replaces a document or edge in a collection
int LogicalCollection::replace(Transaction* trx, VPackSlice const newSlice,
                               ManagedDocumentResult& result, OperationOptions& options,
                               TRI_voc_tick_t& resultMarkerTick, bool lock,
                               TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous) {
  resultMarkerTick = 0;

  if (!newSlice.isObject()) {
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }

  prevRev = 0;
  VPackSlice fromSlice;
  VPackSlice toSlice;

  bool const isEdgeCollection = (type() == TRI_COL_TYPE_EDGE);
  
  if (isEdgeCollection) {
    fromSlice = newSlice.get(StaticStrings::FromString);
    if (!fromSlice.isString()) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    toSlice = newSlice.get(StaticStrings::ToString);
    if (!toSlice.isString()) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
  }

  TRI_voc_rid_t revisionId = 0;
  if (options.isRestore) {
    VPackSlice oldRev = TRI_ExtractRevisionIdAsSlice(newSlice);
    if (!oldRev.isString()) {
      return TRI_ERROR_ARANGO_DOCUMENT_REV_BAD;
    }
    bool isOld;
    VPackValueLength l;
    char const* p = oldRev.getString(l);
    revisionId = TRI_StringToRid(p, l, isOld, false);
    if (isOld) {
      // Do not tolerate old revision ticks:
      revisionId = TRI_HybridLogicalClock();
    }
  } else {
    revisionId = TRI_HybridLogicalClock();
  }
  
  TRI_IF_FAILURE("ReplaceDocumentNoLock") { return TRI_ERROR_DEBUG; }

  // get the previous revision
  VPackSlice key = newSlice.get(StaticStrings::KeyString);
  if (key.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }
  
  bool const useDeadlockDetector = (lock && !trx->isSingleOperationTransaction());
  arangodb::CollectionWriteLocker collectionLocker(this, useDeadlockDetector, lock);

  // get the previous revision
  int res = lookupDocument(trx, key, previous);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  TRI_IF_FAILURE("ReplaceDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("ReplaceDocumentNoMarkerExcept") {
    // test what happens when no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  uint8_t const* vpack = previous.vpack();
  VPackSlice oldDoc(vpack);
  TRI_voc_rid_t oldRevisionId = Transaction::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs) {
    TRI_voc_rid_t expectedRev = 0;
    if (newSlice.isObject()) {
      expectedRev = TRI_ExtractRevisionId(newSlice);
    }
    int res = checkRevision(trx, expectedRev, prevRev);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // merge old and new values 
  TransactionBuilderLeaser builder(trx);
  newObjectForReplace(
      trx, oldDoc, newSlice, fromSlice, toSlice, isEdgeCollection,
      TRI_RidToString(revisionId), *builder.get());

  if (ServerState::isDBServer(trx->serverRole())) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(
            _vocbase->name(),
            trx->resolver()->getCollectionNameCluster(_planId),
            oldDoc, builder->slice(), false)) {
      return TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
    }
  }

  // create marker
  arangodb::wal::CrudMarker replaceMarker(TRI_DF_MARKER_VPACK_DOCUMENT, TRI_MarkerIdTransaction(trx->getInternals()), builder->slice());

  arangodb::wal::Marker const* marker;
  if (options.recoveryMarker == nullptr) {
    marker = &replaceMarker;
  } else {
    marker = options.recoveryMarker;
  }

  VPackSlice const newDoc(marker->vpack());
  
  arangodb::wal::DocumentOperation operation(this, TRI_VOC_DOCUMENT_OPERATION_REPLACE);
  
  try {
    insertRevision(revisionId, marker->vpack(), 0, true);
    operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()), DocumentDescriptor(revisionId, newDoc.begin()));

    res = updateDocument(trx, oldRevisionId, oldDoc, revisionId, newDoc, operation, marker, options.waitForSync);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    operation.revert(trx);
  } else {
    readRevision(trx, result, revisionId);
    
    if (options.waitForSync) {
      // store the tick that was used for writing the new document        
      resultMarkerTick = operation.tick();
    }
  }
  
  return res;
}

/// @brief removes a document or edge
int LogicalCollection::remove(arangodb::Transaction* trx,
                              VPackSlice const slice, OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous) {
  resultMarkerTick = 0;

  // create remove marker
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
      if (isOld) {
        // Do not tolerate old revisions
        revisionId = TRI_HybridLogicalClock();
      }
    }
  } else {
    revisionId = TRI_HybridLogicalClock();
  }
  
  TransactionBuilderLeaser builder(trx);
  newObjectForRemove(
      trx, slice, TRI_RidToString(revisionId), *builder.get());

  prevRev = 0;

  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  
  // create marker
  arangodb::wal::CrudMarker removeMarker(TRI_DF_MARKER_VPACK_REMOVE, TRI_MarkerIdTransaction(trx->getInternals()), builder->slice());
  
  arangodb::wal::Marker const* marker;
  if (options.recoveryMarker == nullptr) {
    marker = &removeMarker;
  } else {
    marker = options.recoveryMarker;
  }

  TRI_IF_FAILURE("RemoveDocumentNoLock") {
    // test what happens if no lock can be acquired
    return TRI_ERROR_DEBUG;
  }

  VPackSlice key;
  if (slice.isString()) {
    key = slice;
  } else {
    key = slice.get(StaticStrings::KeyString);
  }
  TRI_ASSERT(!key.isNone());
  
  arangodb::wal::DocumentOperation operation(this, TRI_VOC_DOCUMENT_OPERATION_REMOVE);
  
  bool const useDeadlockDetector = (lock && !trx->isSingleOperationTransaction());
  arangodb::CollectionWriteLocker collectionLocker(this, useDeadlockDetector, lock);
  
  // get the previous revision
  int res = lookupDocument(trx, key, previous);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  uint8_t const* vpack = previous.vpack();
  prevRev = Transaction::extractRevFromDocument(VPackSlice(vpack));

  // Check old revision:
  if (!options.ignoreRevs && slice.isObject()) {
    TRI_voc_rid_t expectedRev = TRI_ExtractRevisionId(slice);
    int res = checkRevision(trx, expectedRev, prevRev);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  VPackSlice oldDoc(vpack);
  TRI_voc_rid_t oldRevisionId = Transaction::extractRevFromDocument(oldDoc);

  // we found a document to remove
  try {
    operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()), DocumentDescriptor());

    // delete from indexes
    res = deleteSecondaryIndexes(trx, oldRevisionId, oldDoc, false);

    if (res != TRI_ERROR_NO_ERROR) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res);
    }

    res = deletePrimaryIndex(trx, oldRevisionId, oldDoc);

    if (res != TRI_ERROR_NO_ERROR) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res);
    }

    operation.indexed();

    TRI_IF_FAILURE("RemoveDocumentNoOperation") { THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG); }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = TRI_AddOperationTransaction(trx->getInternals(), revisionId, operation, marker, options.waitForSync);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    operation.revert(trx);
  } else {
    // store the tick that was used for removing the document        
    resultMarkerTick = operation.tick();
  }

  return res;
}

/// @brief removes a document or edge, fast path function for database documents
int LogicalCollection::remove(arangodb::Transaction* trx,
                              TRI_voc_rid_t oldRevisionId, VPackSlice const oldDoc, 
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock) {
  resultMarkerTick = 0;
      
  TRI_voc_rid_t revisionId = TRI_HybridLogicalClock();

  // create remove marker
  TransactionBuilderLeaser builder(trx);
  newObjectForRemove(
      trx, oldDoc, TRI_RidToString(revisionId), *builder.get());

  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  
  // create marker
  arangodb::wal::CrudMarker removeMarker(TRI_DF_MARKER_VPACK_REMOVE, TRI_MarkerIdTransaction(trx->getInternals()), builder->slice());
  
  arangodb::wal::Marker const* marker = &removeMarker;

  TRI_IF_FAILURE("RemoveDocumentNoLock") {
    // test what happens if no lock can be acquired
    return TRI_ERROR_DEBUG;
  }

  VPackSlice key = Transaction::extractKeyFromDocument(oldDoc);
  TRI_ASSERT(!key.isNone());
  
  arangodb::wal::DocumentOperation operation(this, TRI_VOC_DOCUMENT_OPERATION_REMOVE);
  
  bool const useDeadlockDetector = (lock && !trx->isSingleOperationTransaction());
  arangodb::CollectionWriteLocker collectionLocker(this, useDeadlockDetector, lock);
  
  operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()), DocumentDescriptor());

  // delete from indexes
  int res;
  try {
    res = deleteSecondaryIndexes(trx, oldRevisionId, oldDoc, false);

    if (res != TRI_ERROR_NO_ERROR) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res);
    }

    res = deletePrimaryIndex(trx, oldRevisionId, oldDoc);

    if (res != TRI_ERROR_NO_ERROR) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res);
    }

    operation.indexed();

    TRI_IF_FAILURE("RemoveDocumentNoOperation") { THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG); }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = TRI_AddOperationTransaction(trx->getInternals(), revisionId, operation, marker, options.waitForSync);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    operation.revert(trx);
  } else {
    // store the tick that was used for removing the document        
    resultMarkerTick = operation.tick();
  }

  return res;
}

/// @brief rolls back a document operation
int LogicalCollection::rollbackOperation(arangodb::Transaction* trx,
                                         TRI_voc_document_operation_e type,
                                         TRI_voc_rid_t oldRevisionId,
                                         VPackSlice const& oldDoc,
                                         TRI_voc_rid_t newRevisionId,
                                         VPackSlice const& newDoc) {
  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(oldRevisionId == 0);
    TRI_ASSERT(oldDoc.isNone());
    TRI_ASSERT(newRevisionId != 0);
    TRI_ASSERT(!newDoc.isNone());

    // ignore any errors we're getting from this
    deletePrimaryIndex(trx, newRevisionId, newDoc);
    deleteSecondaryIndexes(trx, newRevisionId, newDoc, true);
    
    // remove new revision
    try {
      removeRevision(newRevisionId, false);
    } catch (...) {
      // TODO: decide whether we should rethrow here
    }
  
    return TRI_ERROR_NO_ERROR;
  } 
  
  if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
      type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    TRI_ASSERT(oldRevisionId != 0);
    TRI_ASSERT(!oldDoc.isNone());
    TRI_ASSERT(newRevisionId != 0);
    TRI_ASSERT(!newDoc.isNone());
    // remove the current values from the indexes
    deleteSecondaryIndexes(trx, newRevisionId, newDoc, true);
    // re-insert old state
    return insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
  } 
  
  if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    // re-insert old revision
    TRI_ASSERT(oldRevisionId != 0);
    TRI_ASSERT(!oldDoc.isNone());
    TRI_ASSERT(newRevisionId == 0);
    TRI_ASSERT(newDoc.isNone());

    int res = insertPrimaryIndex(trx, oldRevisionId, oldDoc);

    if (res == TRI_ERROR_NO_ERROR) {
      res = insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
    } else {
      LOG(ERR) << "error rolling back remove operation";
    }
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG(ERR) << "logic error. invalid operation type on rollback";
#endif
  return TRI_ERROR_INTERNAL;
}

void LogicalCollection::sizeHint(Transaction* trx, int64_t hint) {
  if (hint <= 0) {
    return;
  }

  int res = primaryIndex()->resize(trx, static_cast<size_t>(hint * 1.1));

  if (res != TRI_ERROR_NO_ERROR) {
    return;
  }

  _revisionsCache->sizeHint(hint);
}

/// @brief initializes an index with all existing documents
int LogicalCollection::fillIndex(arangodb::Transaction* trx,
                                 arangodb::Index* idx,
                                 bool skipPersistent) {
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (!useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  if (idx->isPersistent() && skipPersistent) {
    return TRI_ERROR_NO_ERROR;
  }

  try {
    size_t nrUsed = primaryIndex()->size();
    auto indexPool = application_features::ApplicationServer::getFeature<IndexThreadFeature>("IndexThread")->getThreadPool();

    int res;

    if (indexPool != nullptr && idx->hasBatchInsert() && nrUsed > 256 * 1024 &&
        _indexBuckets > 1) {
      // use batch insert if there is an index pool,
      // the collection has more than one index bucket
      // and it contains a significant amount of documents
      res = fillIndexBatch(trx, idx);
    } else {
      res = fillIndexSequential(trx, idx);
    }

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (std::bad_alloc const&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (std::exception const& ex) {
    LOG(WARN) << "caught exception while filling indexes: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(WARN) << "caught unknown exception while filling indexes";
    return TRI_ERROR_INTERNAL;
  }
}

/// @brief fill an index in batches
int LogicalCollection::fillIndexBatch(arangodb::Transaction* trx,
                                      arangodb::Index* idx) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto indexPool = application_features::ApplicationServer::getFeature<IndexThreadFeature>("IndexThread")->getThreadPool();

  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "fill-index-batch { collection: " << _vocbase->name() << "/"
      << name() << " }, " << idx->context()
      << ", threads: " << indexPool->numThreads()
      << ", buckets: " << indexBuckets();

  // give the index a size hint
  auto primaryIndex = this->primaryIndex();

  auto nrUsed = primaryIndex->size();

  idx->sizeHint(trx, nrUsed);

  // process documents a million at a time
  size_t blockSize = 1024 * 1024 * 1;

  if (nrUsed < blockSize) {
    blockSize = nrUsed;
  }
  if (blockSize == 0) {
    blockSize = 1;
  }

  int res = TRI_ERROR_NO_ERROR;
    
  ManagedDocumentResult mmdr(trx);

  std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> documents;
  documents.reserve(blockSize);
  
  if (nrUsed > 0) {
    arangodb::basics::BucketPosition position;
    uint64_t total = 0;

    while (true) {
      SimpleIndexElement element = primaryIndex->lookupSequential(trx, position, total);

      if (!element) {
        break;
      }
      
      TRI_voc_rid_t revisionId = element.revisionId();

      if (readRevision(trx, mmdr, revisionId)) {
        uint8_t const* vpack = mmdr.vpack();
        TRI_ASSERT(vpack != nullptr);
        documents.emplace_back(std::make_pair(revisionId, VPackSlice(vpack)));
      
        if (documents.size() == blockSize) {
          res = idx->batchInsert(trx, documents, indexPool->numThreads());
          documents.clear();

          // some error occurred
          if (res != TRI_ERROR_NO_ERROR) {
            break;
          }
        }
      }
    }
  }

  // process the remainder of the documents
  if (res == TRI_ERROR_NO_ERROR && !documents.empty()) {
    res = idx->batchInsert(trx, documents, indexPool->numThreads());
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, fill-index-batch { collection: " << _vocbase->name()
      << "/" << name() << " }, " << idx->context()
      << ", threads: " << indexPool->numThreads()
      << ", buckets: " << indexBuckets();

  return res;
}

/// @brief fill an index sequentially
int LogicalCollection::fillIndexSequential(arangodb::Transaction* trx,
                                           arangodb::Index* idx) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "fill-index-sequential { collection: " << _vocbase->name()
      << "/" << name() << " }, " << idx->context()
      << ", buckets: " << indexBuckets();

  // give the index a size hint
  auto primaryIndex = this->primaryIndex();
  size_t nrUsed = primaryIndex->size();

  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 
  idx->sizeHint(trx, nrUsed);

  if (nrUsed > 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    static int const LoopSize = 10000;
    int counter = 0;
    int loops = 0;
#endif

    arangodb::basics::BucketPosition position;
    uint64_t total = 0;
    ManagedDocumentResult result(trx);

    while (true) {
      SimpleIndexElement element = primaryIndex->lookupSequential(trx, position, total);

      if (!element) {
        break;
      }

      TRI_voc_rid_t revisionId = element.revisionId();
      if (readRevision(trx, result, revisionId)) {
        uint8_t const* vpack = result.vpack();
        TRI_ASSERT(vpack != nullptr);
        int res = idx->insert(trx, revisionId, VPackSlice(vpack), false);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      } else {
        return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND; // oops
      }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      if (++counter == LoopSize) {
        counter = 0;
        ++loops;
        LOG(TRACE) << "indexed " << (LoopSize * loops)
                   << " documents of collection " << cid();
      }
#endif
    }
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, fill-index-sequential { collection: " << _vocbase->name()
      << "/" << name() << " }, " << idx->context()
      << ", buckets: " << indexBuckets();

  return TRI_ERROR_NO_ERROR;
}

/// @brief read unlocks a collection
int LogicalCollection::endRead(bool useDeadlockDetector) {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    auto it = arangodb::Transaction::_makeNolockHeaders->find(name());
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndRead blocked: " << _name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  if (useDeadlockDetector) {
    // unregister reader
    try {
      _vocbase->_deadlockDetector.unsetReader(this);
    } catch (...) {
    }
  }

  // LOCKING-DEBUG
  // std::cout << "EndRead: " << _name << std::endl;
  _idxLock.unlockRead();

  return TRI_ERROR_NO_ERROR;
}

/// @brief write unlocks a collection
int LogicalCollection::endWrite(bool useDeadlockDetector) {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    auto it = arangodb::Transaction::_makeNolockHeaders->find(name());
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndWrite blocked: " << _name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  if (useDeadlockDetector) {
    // unregister writer
    try {
      _vocbase->_deadlockDetector.unsetWriter(this);
    } catch (...) {
      // must go on here to unlock the lock
    }
  }

  // LOCKING-DEBUG
  // std::cout << "EndWrite: " << _name << std::endl;
  _idxLock.unlockWrite();

  return TRI_ERROR_NO_ERROR;
}

/// @brief read locks a collection, with a timeout (in µseconds)
int LogicalCollection::beginReadTimed(bool useDeadlockDetector, uint64_t timeout, uint64_t sleepPeriod) {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    auto it = arangodb::Transaction::_makeNolockHeaders->find(name());
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginReadTimed blocked: " << _name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  uint64_t waited = 0;
  if (timeout == 0) {
    // we don't allow looping forever. limit waiting to 15 minutes max.
    timeout = 15 * 60 * 1000 * 1000;
  }

  // LOCKING-DEBUG
  // std::cout << "BeginReadTimed: " << _name << std::endl;
  int iterations = 0;
  bool wasBlocked = false;

  while (true) {
    TRY_READ_LOCKER(locker, _idxLock);

    if (locker.isLocked()) {
      // when we are here, we've got the read lock
      if (useDeadlockDetector) {
        _vocbase->_deadlockDetector.addReader(this, wasBlocked);
      }
      
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    if (useDeadlockDetector) {
      try {
        if (!wasBlocked) {
          // insert reader
          wasBlocked = true;
          if (_vocbase->_deadlockDetector.setReaderBlocked(this) ==
              TRI_ERROR_DEADLOCK) {
            // deadlock
            LOG(TRACE) << "deadlock detected while trying to acquire read-lock on collection '" << name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
          LOG(TRACE) << "waiting for read-lock on collection '" << name() << "'";
        } else if (++iterations >= 5) {
          // periodically check for deadlocks
          TRI_ASSERT(wasBlocked);
          iterations = 0;
          if (_vocbase->_deadlockDetector.detectDeadlock(this, false) ==
              TRI_ERROR_DEADLOCK) {
            // deadlock
            _vocbase->_deadlockDetector.unsetReaderBlocked(this);
            LOG(TRACE) << "deadlock detected while trying to acquire read-lock on collection '" << name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
        }
      } catch (...) {
        // clean up!
        if (wasBlocked) {
          _vocbase->_deadlockDetector.unsetReaderBlocked(this);
        }
        // always exit
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

#ifdef _WIN32
    usleep((unsigned long)sleepPeriod);
#else
    usleep((useconds_t)sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      if (useDeadlockDetector) {
        _vocbase->_deadlockDetector.unsetReaderBlocked(this);
      }
      LOG(TRACE) << "timed out waiting for read-lock on collection '" << name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }
}

/// @brief write locks a collection, with a timeout
int LogicalCollection::beginWriteTimed(bool useDeadlockDetector, uint64_t timeout, uint64_t sleepPeriod) {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    auto it = arangodb::Transaction::_makeNolockHeaders->find(name());
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginWriteTimed blocked: " << _name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  uint64_t waited = 0;
  if (timeout == 0) {
    // we don't allow looping forever. limit waiting to 15 minutes max.
    timeout = 15 * 60 * 1000 * 1000;
  }

  // LOCKING-DEBUG
  // std::cout << "BeginWriteTimed: " << document->_info._name << std::endl;
  int iterations = 0;
  bool wasBlocked = false;

  while (true) {
    TRY_WRITE_LOCKER(locker, _idxLock);

    if (locker.isLocked()) {
      // register writer
      if (useDeadlockDetector) {
        _vocbase->_deadlockDetector.addWriter(this, wasBlocked);
      }
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    if (useDeadlockDetector) {
      try {
        if (!wasBlocked) {
          // insert writer
          wasBlocked = true;
          if (_vocbase->_deadlockDetector.setWriterBlocked(this) ==
              TRI_ERROR_DEADLOCK) {
            // deadlock
            LOG(TRACE) << "deadlock detected while trying to acquire write-lock on collection '" << name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
          LOG(TRACE) << "waiting for write-lock on collection '" << name() << "'";
        } else if (++iterations >= 5) {
          // periodically check for deadlocks
          TRI_ASSERT(wasBlocked);
          iterations = 0;
          if (_vocbase->_deadlockDetector.detectDeadlock(this, true) ==
              TRI_ERROR_DEADLOCK) {
            // deadlock
            _vocbase->_deadlockDetector.unsetWriterBlocked(this);
            LOG(TRACE) << "deadlock detected while trying to acquire write-lock on collection '" << name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
        }
      } catch (...) {
        // clean up!
        if (wasBlocked) {
          _vocbase->_deadlockDetector.unsetWriterBlocked(this);
        }
        // always exit
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

#ifdef _WIN32
    usleep((unsigned long)sleepPeriod);
#else
    usleep((useconds_t)sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      if (useDeadlockDetector) {
        _vocbase->_deadlockDetector.unsetWriterBlocked(this);
      }
      LOG(TRACE) << "timed out waiting for write-lock on collection '" << name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }
}

/// @brief looks up a document by key, low level worker
/// the caller must make sure the read lock on the collection is held
/// the key must be a string slice, no revision check is performed
int LogicalCollection::lookupDocument(
    arangodb::Transaction* trx, VPackSlice const key,
    ManagedDocumentResult& result) {

  if (!key.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  SimpleIndexElement element = primaryIndex()->lookupKey(trx, key, result);
  if (element) {
    readRevision(trx, result, element.revisionId());
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

/// @brief checks the revision of a document
int LogicalCollection::checkRevision(Transaction* trx,
                                     TRI_voc_rid_t expected,
                                     TRI_voc_rid_t found) {
  if (expected != 0 && found != expected) {
    return TRI_ERROR_ARANGO_CONFLICT;
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief updates an existing document, low level worker
/// the caller must make sure the write lock on the collection is held
int LogicalCollection::updateDocument(
    arangodb::Transaction* trx, 
    TRI_voc_rid_t oldRevisionId, VPackSlice const& oldDoc, 
    TRI_voc_rid_t newRevisionId, VPackSlice const& newDoc, 
    arangodb::wal::DocumentOperation& operation, arangodb::wal::Marker const* marker,
    bool& waitForSync) {

  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)
  int res = deleteSecondaryIndexes(trx, oldRevisionId, oldDoc, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // re-enter the document in case of failure, ignore errors during rollback
    insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
    return res;
  }

  // insert new document into secondary indexes
  res = insertSecondaryIndexes(trx, newRevisionId, newDoc, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: move down
    removeRevision(newRevisionId, false);

    // rollback
    deleteSecondaryIndexes(trx, newRevisionId, newDoc, true);
    insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);

    return res;
  }
 
  // update the index element (primary index only - other index have been adjusted) 
  VPackSlice keySlice(Transaction::extractKeyFromDocument(newDoc));
  SimpleIndexElement* element = primaryIndex()->lookupKeyRef(trx, keySlice);
  if (element != nullptr && element->revisionId() != 0) {
    element->updateRevisionId(newRevisionId, static_cast<uint32_t>(keySlice.begin() - newDoc.begin()));
  }
  
  operation.indexed();

  TRI_IF_FAILURE("UpdateDocumentNoOperation") { return TRI_ERROR_DEBUG; }
  
  TRI_IF_FAILURE("UpdateDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return TRI_AddOperationTransaction(trx->getInternals(), newRevisionId, operation, marker, waitForSync);
}

/// @brief insert a document, low level worker
/// the caller must make sure the write lock on the collection is held
int LogicalCollection::insertDocument(
    arangodb::Transaction* trx, TRI_voc_rid_t revisionId, VPackSlice const& doc,
    arangodb::wal::DocumentOperation& operation, arangodb::wal::Marker const* marker,
    bool& waitForSync) {

  // insert into primary index first
  int res = insertPrimaryIndex(trx, revisionId, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = insertSecondaryIndexes(trx, revisionId, doc, false);

  if (res != TRI_ERROR_NO_ERROR) {
    deleteSecondaryIndexes(trx, revisionId, doc, true);
    deletePrimaryIndex(trx, revisionId, doc);
    return res;
  }
  
  operation.indexed();

  TRI_IF_FAILURE("InsertDocumentNoOperation") { return TRI_ERROR_DEBUG; }

  TRI_IF_FAILURE("InsertDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return TRI_AddOperationTransaction(trx->getInternals(), revisionId, operation, marker, waitForSync);
}

/// @brief creates a new entry in the primary index
int LogicalCollection::insertPrimaryIndex(arangodb::Transaction* trx,
                                          TRI_voc_rid_t revisionId,
                                          VPackSlice const& doc) {
  TRI_IF_FAILURE("InsertPrimaryIndex") { return TRI_ERROR_DEBUG; }

  // insert into primary index
  return primaryIndex()->insertKey(trx, revisionId, doc);
}

/// @brief deletes an entry from the primary index
int LogicalCollection::deletePrimaryIndex(
    arangodb::Transaction* trx, TRI_voc_rid_t revisionId, VPackSlice const& doc) {
  TRI_IF_FAILURE("DeletePrimaryIndex") { return TRI_ERROR_DEBUG; }

  return primaryIndex()->removeKey(trx, revisionId, doc);
}

/// @brief creates a new entry in the secondary indexes
int LogicalCollection::insertSecondaryIndexes(
    arangodb::Transaction* trx, TRI_voc_rid_t revisionId, VPackSlice const& doc, bool isRollback) {
  // Coordintor doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_IF_FAILURE("InsertSecondaryIndexes") { return TRI_ERROR_DEBUG; }

  bool const useSecondary = useSecondaryIndexes();
  if (!useSecondary && _persistentIndexes == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  int result = TRI_ERROR_NO_ERROR;

  size_t const n = _indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = _indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 

    if (!useSecondary && !idx->isPersistent()) {
      continue;
    }

    int res = idx->insert(trx, revisionId, doc, isRollback);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    } 
    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result == TRI_ERROR_NO_ERROR) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  return result;
}

/// @brief deletes an entry from the secondary indexes
int LogicalCollection::deleteSecondaryIndexes(
    arangodb::Transaction* trx, TRI_voc_rid_t revisionId, VPackSlice const& doc, bool isRollback) {
  // Coordintor doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  bool const useSecondary = useSecondaryIndexes();
  if (!useSecondary && _persistentIndexes == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("DeleteSecondaryIndexes") { return TRI_ERROR_DEBUG; }

  int result = TRI_ERROR_NO_ERROR;

  size_t const n = _indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = _indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX); 
    
    if (!useSecondary && !idx->isPersistent()) {
      continue;
    }

    int res = idx->remove(trx, revisionId, doc, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

/// @brief new object for insert, computes the hash of the key
int LogicalCollection::newObjectForInsert(
    Transaction* trx,
    VPackSlice const& value,
    VPackSlice const& fromSlice,
    VPackSlice const& toSlice,
    bool isEdgeCollection,
    VPackBuilder& builder,
    bool isRestore) {

  TRI_voc_tick_t newRev = 0;
  builder.openObject();
  
  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev 

  // _key
  VPackSlice s = value.get(StaticStrings::KeyString);
  if (s.isNone()) {
    TRI_ASSERT(!isRestore);   // need key in case of restore
    newRev = TRI_HybridLogicalClock();
    std::string keyString = _keyGenerator->generate(TRI_NewTickServer());
    if (keyString.empty()) {
      return TRI_ERROR_ARANGO_OUT_OF_KEYS;
    }
    uint8_t* where = builder.add(StaticStrings::KeyString,
                                 VPackValue(keyString));
    s = VPackSlice(where);  // point to newly built value, the string
  } else if (!s.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  } else {
    std::string keyString = s.copyString();
    int res = _keyGenerator->validate(keyString, isRestore);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    builder.add(StaticStrings::KeyString, s);
  }
  
  // _id
  uint8_t* p = builder.add(StaticStrings::IdString,
      VPackValuePair(9ULL, VPackValueType::Custom));
  *p++ = 0xf3;  // custom type for _id
  if (ServerState::isDBServer(trx->serverRole()) && !_isSystem) {
    // db server in cluster, note: the local collections _statistics,
    // _statisticsRaw and _statistics15 (which are the only system collections)
    // must not be treated as shards but as local collections
    DatafileHelper::StoreNumber<uint64_t>(p, _planId, sizeof(uint64_t));
  } else {
    // local server
    DatafileHelper::StoreNumber<uint64_t>(p, _cid, sizeof(uint64_t));
  }

  // _from and _to
  if (isEdgeCollection) {
    TRI_ASSERT(!fromSlice.isNone());
    TRI_ASSERT(!toSlice.isNone());
    builder.add(StaticStrings::FromString, fromSlice);
    builder.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  std::string newRevSt;
  if (isRestore) {
    VPackSlice oldRev = TRI_ExtractRevisionIdAsSlice(value);
    if (!oldRev.isString()) {
      return TRI_ERROR_ARANGO_DOCUMENT_REV_BAD;
    }
    bool isOld;
    VPackValueLength l;
    char const* p = oldRev.getString(l);
    TRI_voc_rid_t oldRevision = TRI_StringToRid(p, l, isOld, false);
    if (isOld) {
      oldRevision = TRI_HybridLogicalClock();
    }
    newRevSt = TRI_RidToString(oldRevision);
  } else {
    if (newRev == 0) {
      newRev = TRI_HybridLogicalClock();
    }
    newRevSt = TRI_RidToString(newRev);
  }
  builder.add(StaticStrings::RevString, VPackValue(newRevSt));
  
  // add other attributes after the system attributes
  TRI_SanitizeObjectWithEdges(value, builder);

  builder.close();
  return TRI_ERROR_NO_ERROR;
} 

/// @brief new object for replace, oldValue must have _key and _id correctly set
void LogicalCollection::newObjectForReplace(
    Transaction* trx,
    VPackSlice const& oldValue,
    VPackSlice const& newValue,
    VPackSlice const& fromSlice,
    VPackSlice const& toSlice,
    bool isEdgeCollection,
    std::string const& rev,
    VPackBuilder& builder) {

  builder.openObject();

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev
  
  // _key
  VPackSlice s = oldValue.get(StaticStrings::KeyString);
  TRI_ASSERT(!s.isNone());
  builder.add(StaticStrings::KeyString, s);

  // _id
  s = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!s.isNone());
  builder.add(StaticStrings::IdString, s);

  // _from and _to here
  if (isEdgeCollection) {
    TRI_ASSERT(!fromSlice.isNone());
    TRI_ASSERT(!toSlice.isNone());
    builder.add(StaticStrings::FromString, fromSlice);
    builder.add(StaticStrings::ToString, toSlice);
  }
  
  // _rev
  builder.add(StaticStrings::RevString, VPackValue(rev));
  
  // add other attributes after the system attributes
  TRI_SanitizeObjectWithEdges(newValue, builder);

  builder.close();
} 

/// @brief merge two objects for update, oldValue must have correctly set
/// _key and _id attributes
void LogicalCollection::mergeObjectsForUpdate(
      arangodb::Transaction* trx,
      VPackSlice const& oldValue,
      VPackSlice const& newValue,
      bool isEdgeCollection,
      std::string const& rev,
      bool mergeObjects, bool keepNull,
      VPackBuilder& b) {

  b.openObject();

  VPackSlice keySlice = oldValue.get(StaticStrings::KeyString);
  VPackSlice idSlice = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!keySlice.isNone());
  TRI_ASSERT(!idSlice.isNone());
  
  // Find the attributes in the newValue object:
  VPackSlice fromSlice;
  VPackSlice toSlice;

  std::unordered_map<std::string, VPackSlice> newValues;
  { 
    VPackObjectIterator it(newValue, false);
    while (it.valid()) {
      std::string key = it.key().copyString();
      if (!key.empty() && key[0] == '_' &&
          (key == StaticStrings::KeyString ||
           key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString ||
           key == StaticStrings::ToString)) {
        // note _from and _to and ignore _id, _key and _rev
        if (key == StaticStrings::FromString) {
          fromSlice = it.value();
        } else if (key == StaticStrings::ToString) {
          toSlice = it.value();
        } // else do nothing
      } else {
        // regular attribute
        newValues.emplace(std::move(key), it.value());
      }

      it.next();
    }
  }

  if (isEdgeCollection) {
    if (fromSlice.isNone()) {
      fromSlice = oldValue.get(StaticStrings::FromString);
    }
    if (toSlice.isNone()) {
      toSlice = oldValue.get(StaticStrings::ToString);
    }
  }

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  b.add(StaticStrings::KeyString, keySlice);

  // _id
  b.add(StaticStrings::IdString, idSlice);

  // _from, _to
  if (isEdgeCollection) {
    TRI_ASSERT(!fromSlice.isNone());
    TRI_ASSERT(!toSlice.isNone());
    b.add(StaticStrings::FromString, fromSlice);
    b.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  b.add(StaticStrings::RevString, VPackValue(rev));

  // add other attributes after the system attributes
  { 
    VPackObjectIterator it(oldValue, false);
    while (it.valid()) {
      std::string key = it.key().copyString();
      // exclude system attributes in old value now
      if (!key.empty() && key[0] == '_' &&
          (key == StaticStrings::KeyString ||
           key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString ||
           key == StaticStrings::ToString)) {
        it.next();
        continue;
      }
      
      auto found = newValues.find(key);

      if (found == newValues.end()) {
        // use old value
        b.add(key, it.value());
      } else if (mergeObjects && it.value().isObject() &&
                  (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          VPackBuilder sub = VPackCollection::merge(it.value(), value, 
                                                    true, !keepNull);
          b.add(key, sub.slice());
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      } else {
        // use new value
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          b.add(key, value);
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      }
      it.next();
    }
  }

  // add remaining values that were only in new object
  for (auto& it : newValues) {
    auto& s = it.second;
    if (s.isNone()) {
      continue;
    }
    if (!keepNull && s.isNull()) {
      continue;
    }
    b.add(std::move(it.first), s);
  }

  b.close();
}

/// @brief new object for remove, must have _key set
void LogicalCollection::newObjectForRemove(
    Transaction* trx,
    VPackSlice const& oldValue,
    std::string const& rev,
    VPackBuilder& builder) {

  // create an object consisting of _key and _rev (in this order)
  builder.openObject();
  if (oldValue.isString()) {
    builder.add(StaticStrings::KeyString, oldValue);
  } else {
    VPackSlice s = oldValue.get(StaticStrings::KeyString);
    TRI_ASSERT(s.isString());
    builder.add(StaticStrings::KeyString, s);
  }
  builder.add(StaticStrings::RevString, VPackValue(rev));
  builder.close();
}

bool LogicalCollection::readRevision(Transaction* trx, ManagedDocumentResult& result, TRI_voc_rid_t revisionId) {
  TRI_ASSERT(_revisionsCache != nullptr);
  return _revisionsCache->lookupRevision(trx, result, revisionId, !_isInitialIteration);
}

bool LogicalCollection::readRevisionConditional(Transaction* trx, ManagedDocumentResult& result, TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) {
  TRI_ASSERT(_revisionsCache != nullptr);
  return _revisionsCache->lookupRevisionConditional(trx, result, revisionId, maxTick, excludeWal, true);
}

void LogicalCollection::insertRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal) {
  // note: there is no need to insert into the cache here as the data points to temporary storage
  getPhysical()->insertRevision(revisionId, dataptr, fid, isInWal, true);
}

void LogicalCollection::updateRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal) {
  // note: there is no need to modify the cache entry here as insertRevision has not inserted the document into the cache
  getPhysical()->updateRevision(revisionId, dataptr, fid, isInWal);
}
  
bool LogicalCollection::updateRevisionConditional(TRI_voc_rid_t revisionId, TRI_df_marker_t const* oldPosition, TRI_df_marker_t const* newPosition, TRI_voc_fid_t newFid, bool isInWal) {
  return getPhysical()->updateRevisionConditional(revisionId, oldPosition, newPosition, newFid, isInWal);
}

void LogicalCollection::removeRevision(TRI_voc_rid_t revisionId, bool updateStats) {
  // clean up cache entry
  TRI_ASSERT(_revisionsCache);
  _revisionsCache->removeRevision(revisionId); 

  // and remove from storage engine
  getPhysical()->removeRevision(revisionId, updateStats);
}

/// @brief a method to skip certain documents in AQL write operations,
/// this is only used in the enterprise edition for smart graphs
#ifndef USE_ENTERPRISE
bool LogicalCollection::skipForAqlWrite(arangodb::velocypack::Slice document,
                                        std::string const& key) const {
  return false;
}
#endif

bool LogicalCollection::isSatellite() const {
  return _replicationFactor == 0;
}

