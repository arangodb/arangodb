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

#include "Aql/QueryCache.h"
#include "Basics/fasthash.h"
#include "Basics/Mutex.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "Sharding/ShardingInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Collection.h>
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

std::string readGloballyUniqueId(arangodb::velocypack::Slice info) {
  static const std::string empty;
  auto guid = arangodb::basics::VelocyPackHelper::getStringValue(
    info,
    arangodb::StaticStrings::DataSourceGuid,
    empty
  );

  if (!guid.empty()) {
    return guid;
  }

  auto version = arangodb::basics::VelocyPackHelper::readNumericValue<uint32_t>(
    info,
    "version",
    LogicalCollection::currentVersion()
  );

  // predictable UUID for legacy collections
  if (version < LogicalCollection::CollectionVersions::VERSION_33
      && info.isObject()) {
    return arangodb::basics::VelocyPackHelper::getStringValue(
      info, arangodb::StaticStrings::DataSourceName, empty
    );
  }

  return empty;
}

std::string readStringValue(
    arangodb::velocypack::Slice info,
    std::string const& name,
    std::string const& def
) {
  return info.isObject() ? Helper::getStringValue(info, name, def) : def;
}

arangodb::LogicalDataSource::Type const& readType(
    arangodb::velocypack::Slice info,
    std::string const& key,
    TRI_col_type_e def
) {
  static const auto& document = arangodb::LogicalDataSource::Type::emplace(
    arangodb::velocypack::StringRef("document")
  );
  static const auto& edge = arangodb::LogicalDataSource::Type::emplace(
    arangodb::velocypack::StringRef("edge")
  );

  // arbitrary system-global value for unknown
  static const auto& unknown = arangodb::LogicalDataSource::Type::emplace(
    arangodb::velocypack::StringRef("")
  );

  switch (Helper::readNumericValue<TRI_col_type_e, int>(info, key, def)) {
   case TRI_col_type_e::TRI_COL_TYPE_DOCUMENT:
    return document;
   case TRI_col_type_e::TRI_COL_TYPE_EDGE:
    return edge;
   default:
    return unknown;
  }
}

} // namespace

// The Slice contains the part of the plan that
// is relevant for this collection.
LogicalCollection::LogicalCollection(
    TRI_vocbase_t& vocbase,
    VPackSlice const& info,
    bool isAStub,
    uint64_t planVersion /*= 0*/
): LogicalDataSource(
     LogicalCollection::category(),
     ::readType(info, StaticStrings::DataSourceType, TRI_COL_TYPE_UNKNOWN),
     vocbase,
     arangodb::basics::VelocyPackHelper::extractIdValue(info),
     ::readGloballyUniqueId(info),
     arangodb::basics::VelocyPackHelper::stringUInt64(
       info.get(StaticStrings::DataSourcePlanId)
     ),
     ::readStringValue(info, StaticStrings::DataSourceName, ""),
     planVersion,
     TRI_vocbase_t::IsSystemName(::readStringValue(
       info, StaticStrings::DataSourceName, ""
     )) && Helper::readBooleanValue(
       info, StaticStrings::DataSourceSystem, false
     ),
     Helper::readBooleanValue(info, StaticStrings::DataSourceDeleted, false)
   ),
      _version(Helper::readNumericValue<uint32_t>(info, "version", currentVersion())),
      _internalVersion(0),
      _type(Helper::readNumericValue<TRI_col_type_e, int>(
        info, StaticStrings::DataSourceType, TRI_COL_TYPE_UNKNOWN)
      ),
      _status(Helper::readNumericValue<TRI_vocbase_col_status_e, int>(
          info, "status", TRI_VOC_COL_STATUS_CORRUPTED)),
      _isAStub(isAStub),
      _isSmart(Helper::readBooleanValue(info, StaticStrings::IsSmart, false)),
      _waitForSync(Helper::readBooleanValue(info, StaticStrings::WaitForSyncString, false)),
      _allowUserKeys(Helper::readBooleanValue(info, "allowUserKeys", true)),
      _keyOptions(nullptr),
      _keyGenerator(),
      _physical(
        EngineSelectorFeature::ENGINE->createPhysicalCollection(*this, info)
      ),
      _sharding() {
  TRI_ASSERT(info.isObject());

  if (!TRI_vocbase_t::IsAllowedName(info)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  if (_version < minimumVersion()) {
    // collection is too "old"
    std::string errorMsg(std::string("collection '") + name() +
                         "' has a too old version. Please start the server "
                         "with the --database.auto-upgrade option.");

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, errorMsg);
  }

  TRI_ASSERT(!guid().empty());

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));

  // add keyOptions from slice
  VPackSlice keyOpts = info.get("keyOptions");
  _keyGenerator.reset(KeyGenerator::factory(keyOpts));
  if (!keyOpts.isNone()) {
    _keyOptions = VPackBuilder::clone(keyOpts).steal();
  }

  _sharding = std::make_unique<ShardingInfo>(info, this);

  if (ServerState::instance()->isDBServer() ||
      !ServerState::instance()->isRunningInCluster()) {
    _followers.reset(new FollowerInfo(this));
  }

  TRI_ASSERT(_physical != nullptr);
  // This has to be called AFTER _phyiscal and _logical are properly linked
  // together.
  prepareIndexes(info.get("indexes"));
}

/*static*/ LogicalDataSource::Category const& LogicalCollection::category() noexcept {
  static const Category category;

  return category;
}

LogicalCollection::~LogicalCollection() {}

// SECTION: sharding
ShardingInfo* LogicalCollection::shardingInfo() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding.get();
}

size_t LogicalCollection::numberOfShards() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->numberOfShards();
}

size_t LogicalCollection::replicationFactor() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->replicationFactor();
}

std::string LogicalCollection::distributeShardsLike() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->distributeShardsLike();
}

void LogicalCollection::distributeShardsLike(std::string const& cid, ShardingInfo const* other) {
  TRI_ASSERT(_sharding != nullptr);
  _sharding->distributeShardsLike(cid, other);
}

std::vector<std::string> const& LogicalCollection::avoidServers() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->avoidServers();
}

bool LogicalCollection::isSatellite() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->isSatellite();
}

bool LogicalCollection::usesDefaultShardKeys() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->usesDefaultShardKeys();
}

std::vector<std::string> const& LogicalCollection::shardKeys() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->shardKeys();
}

std::shared_ptr<ShardMap> LogicalCollection::shardIds() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->shardIds();
}

void LogicalCollection::setShardMap(std::shared_ptr<ShardMap> const& map) {
  TRI_ASSERT(_sharding != nullptr);
  _sharding->setShardMap(map);
}

int LogicalCollection::getResponsibleShard(arangodb::velocypack::Slice slice,
                                           bool docComplete, std::string& shardID) {
  bool usesDefaultShardKeys;
  return getResponsibleShard(slice, docComplete, shardID, usesDefaultShardKeys);
}

int LogicalCollection::getResponsibleShard(arangodb::velocypack::Slice slice,
                                           bool docComplete, std::string& shardID,
                                           bool& usesDefaultShardKeys,
                                           std::string const& key) {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->getResponsibleShard(slice, docComplete, shardID, usesDefaultShardKeys, key);
}

/// @briefs creates a new document key, the input slice is ignored here
std::string LogicalCollection::createKey(VPackSlice) {
  return keyGenerator()->generate();
}

void LogicalCollection::prepareIndexes(VPackSlice indexesSlice) {
  TRI_ASSERT(_physical != nullptr);

  if (!indexesSlice.isArray()) {
    // always point to an array
    indexesSlice = arangodb::velocypack::Slice::emptyArraySlice();
  }

  _physical->prepareIndexes(indexesSlice);
}

std::unique_ptr<IndexIterator> LogicalCollection::getAllIterator(
    transaction::Methods* trx) {
  return _physical->getAllIterator(trx);
}

std::unique_ptr<IndexIterator> LogicalCollection::getAnyIterator(transaction::Methods* trx) {
  return _physical->getAnyIterator(trx);
}

void LogicalCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(LocalDocumentId const&)> callback) {
  _physical->invokeOnAllElements(trx, callback);
}

// @brief Return the number of documents in this collection
uint64_t LogicalCollection::numberDocuments(transaction::Methods* trx, transaction::CountType type) {
  // detailed results should have been handled in the levels above us
  TRI_ASSERT(type != transaction::CountType::Detailed);

  int64_t documents = transaction::CountCache::NotPopulated;
  if (type == transaction::CountType::ForceCache) {
    // always return from the cache, regardless what's in it
    documents = _countCache.get();
  } else if (type == transaction::CountType::TryCache) {
    documents = _countCache.get(transaction::CountCache::Ttl);
  }
  if (documents == transaction::CountCache::NotPopulated) {
    documents = static_cast<int64_t>(getPhysical()->numberDocuments(trx));
    _countCache.store(documents);
  }
  TRI_ASSERT(documents >= 0);
  return static_cast<uint64_t>(documents);
}

uint32_t LogicalCollection::internalVersion() const { return _internalVersion; }

TRI_col_type_e LogicalCollection::type() const { return _type; }

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
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  return _physical->revision(trx);
}

bool LogicalCollection::waitForSync() const { return _waitForSync; }

bool LogicalCollection::isSmart() const { return _isSmart; }

std::unique_ptr<FollowerInfo> const& LogicalCollection::followers() const {
  return _followers;
}

std::unordered_map<std::string, double> LogicalCollection::clusterIndexEstimates(bool allowUpdate) {
  return getPhysical()->clusterIndexEstimates(allowUpdate);
}

void LogicalCollection::clusterIndexEstimates(std::unordered_map<std::string, double>&& estimates) {
  getPhysical()->clusterIndexEstimates(std::move(estimates));
}

void LogicalCollection::flushClusterIndexEstimates() {
  getPhysical()->flushClusterIndexEstimates();
}

std::vector<std::shared_ptr<arangodb::Index>> LogicalCollection::getIndexes() const {
  return getPhysical()->getIndexes();
}

void LogicalCollection::getIndexesVPack(VPackBuilder& result,
                                        std::underlying_type<Index::Serialize>::type flags,
                                        std::function<bool(arangodb::Index const*)> const& filter) const {
  getPhysical()->getIndexesVPack(result, flags, filter);
}

bool LogicalCollection::allowUserKeys() const { return _allowUserKeys; }

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

Result LogicalCollection::rename(std::string&& newName) {
  // Should only be called from inside vocbase.
  // Otherwise caching is destroyed.
  TRI_ASSERT(!ServerState::instance()->isCoordinator());  // NOT YET IMPLEMENTED
  auto* databaseFeature = application_features::ApplicationServer::lookupFeature<
    DatabaseFeature
  >("Database");

  if (!databaseFeature) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find feature 'Database' while renaming collection"
    );
  }

  // Check for illegal states.
  switch (_status) {
    case TRI_VOC_COL_STATUS_CORRUPTED:
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    case TRI_VOC_COL_STATUS_DELETED:
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
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

  auto doSync = databaseFeature->forceSyncProperties();
  std::string oldName = name();

  // Okay we can finally rename safely
  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine != nullptr);

    name(std::move(newName));
    engine->changeCollection(vocbase(), id(), *this, doSync);
  } catch (basics::Exception const& ex) {
    // Engine Rename somehow failed. Reset to old name
    name(std::move(oldName));

    return ex.code();
  } catch (...) {
    // Engine Rename somehow failed. Reset to old name
    name(std::move(oldName));

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

arangodb::Result LogicalCollection::drop() {
  // make sure collection has been closed
  this->close();

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  engine->destroyCollection(vocbase(), *this);
  deleted(true);
  _physical->drop();

  return arangodb::Result();
}

void LogicalCollection::setStatus(TRI_vocbase_col_status_e status) {
  _status = status;

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    increaseInternalVersion();
  }
}

void LogicalCollection::toVelocyPackForClusterInventory(VPackBuilder& result,
                                                        bool useSystem,
                                                        bool isReady,
                                                        bool allInSync) const {
  if (system() && !useSystem) {
    return;
  }

  result.openObject();
  result.add(VPackValue("parameters"));

  std::unordered_set<std::string> ignoreKeys{"allowUserKeys", "cid", "count",
                                             "statusString", "version",
                                             "distributeShardsLike", "objectId",
                                             "indexes"};
  VPackBuilder params = toVelocyPackIgnore(ignoreKeys, false, false);
  { VPackObjectBuilder guard(&result);

    for (auto const& p : VPackObjectIterator(params.slice())) {
      result.add(p.key);
      result.add(p.value);
    }

    if (!_sharding->distributeShardsLike().empty()) {
      CollectionNameResolver resolver(vocbase());

      result.add("distributeShardsLike",
                 VPackValue(resolver.getCollectionNameCluster(
                     static_cast<TRI_voc_cid_t>(basics::StringUtils::uint64(
                         distributeShardsLike())))));
    }
  }

  result.add(VPackValue("indexes"));
  getIndexesVPack(result, Index::makeFlags(), [](arangodb::Index const* idx) {
    // we have to exclude the primary and the edge index here, because otherwise
    // at least the MMFiles engine will try to create it
    // AND exclude arangosearch indexes
    return (idx->type() != arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX &&
            idx->type() != arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX &&
            idx->type() != arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK);
  });
  result.add("planVersion", VPackValue(planVersion()));
  result.add("isReady", VPackValue(isReady));
  result.add("allInSync", VPackValue(allInSync));
  result.close();  // CollectionInfo
}

arangodb::Result LogicalCollection::appendVelocyPack(
    arangodb::velocypack::Builder& result,
    bool translateCids,
    bool forPersistence
) const {
  // We write into an open object
  TRI_ASSERT(result.isOpenObject());

  // Collection Meta Information
  result.add("cid", VPackValue(std::to_string(id())));
  result.add(StaticStrings::DataSourceType, VPackValue(static_cast<int>(_type)));
  result.add("status", VPackValue(_status));
  result.add("statusString", VPackValue(::translateStatus(_status)));
  result.add("version", VPackValue(_version));

  // Collection Flags
  result.add("waitForSync", VPackValue(_waitForSync));

  if (!forPersistence) {
    // with 'forPersistence' added by LogicalDataSource::toVelocyPack
    // FIXME TODO is this needed in !forPersistence???
    result.add(StaticStrings::DataSourceDeleted, VPackValue(deleted()));
    result.add(StaticStrings::DataSourceSystem, VPackValue(system()));
  }

  // TODO is this still releveant or redundant in keyGenerator?
  result.add("allowUserKeys", VPackValue(_allowUserKeys));

  // keyoptions
  result.add("keyOptions", VPackValue(VPackValueType::Object));
  if (_keyGenerator != nullptr) {
    _keyGenerator->toVelocyPack(result);
  }
  result.close();

  // Physical Information
  getPhysical()->getPropertiesVPack(result);

  // Indexes
  result.add(VPackValue("indexes"));
  auto flags = Index::makeFlags();
  if (forPersistence) {
    flags = Index::makeFlags(Index::Serialize::ObjectId);
  }
  getIndexesVPack(result, flags);

  // Cluster Specific
  result.add(StaticStrings::IsSmart, VPackValue(_isSmart));

  if (!forPersistence) {
    // with 'forPersistence' added by LogicalDataSource::toVelocyPack
    // FIXME TODO is this needed in !forPersistence???
    result.add(StaticStrings::DataSourcePlanId, VPackValue(std::to_string(planId())));
  }

  _sharding->toVelocyPack(result, translateCids);

  includeVelocyPackEnterprise(result);

  TRI_ASSERT(result.isOpenObject());
  // We leave the object open

  return arangodb::Result();
}

void LogicalCollection::toVelocyPackIgnore(VPackBuilder& result,
    std::unordered_set<std::string> const& ignoreKeys, bool translateCids,
    bool forPersistence) const {
  TRI_ASSERT(result.isOpenObject());
  VPackBuilder b = toVelocyPackIgnore(ignoreKeys, translateCids, forPersistence);
  result.add(VPackObjectIterator(b.slice()));
}

VPackBuilder LogicalCollection::toVelocyPackIgnore(
    std::unordered_set<std::string> const& ignoreKeys, bool translateCids,
    bool forPersistence) const {
  VPackBuilder full;
  full.openObject();
    properties(full, translateCids, forPersistence);
  full.close();
  return VPackCollection::remove(full.slice(), ignoreKeys);
}

void LogicalCollection::includeVelocyPackEnterprise(VPackBuilder&) const {
  // We ain't no enterprise
}

void LogicalCollection::increaseInternalVersion() { ++_internalVersion; }

arangodb::Result LogicalCollection::properties(
    velocypack::Slice const& slice,
    bool partialUpdate
) {
  // the following collection properties are intentionally not updated,
  // as updating them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...

  auto* databaseFeature = application_features::ApplicationServer::lookupFeature<
    DatabaseFeature
  >("Database");

  if (!databaseFeature) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find feature 'Database' while updating collection"
    );
   }

  auto* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find a storage engine while updating collection"
    );
  }

  MUTEX_LOCKER(guard, _infoLock); // prevent simultanious updates

  size_t rf = _sharding->replicationFactor();
  VPackSlice rfSl = slice.get("replicationFactor");
  if (!rfSl.isNone()) {
    if (rfSl.isInteger()) {
      int64_t rfTest = rfSl.getNumber<int64_t>();
      if (rfTest < 0) {
        // negative value for replication factor... not good
        return Result(TRI_ERROR_BAD_PARAMETER, "bad value for replicationFactor");
      }

      rf = rfSl.getNumber<size_t>();
      if ((!isSatellite() && rf == 0) || rf > 10) {
        return Result(TRI_ERROR_BAD_PARAMETER, "bad value for replicationFactor");
      }

      if (ServerState::instance()->isCoordinator()
          && rf != _sharding->replicationFactor()) { // sanity checks
        if (!_sharding->distributeShardsLike().empty()) {
          return Result(TRI_ERROR_FORBIDDEN, "Cannot change replicationFactor, "
                        "please change " + _sharding->distributeShardsLike());
        } else if (_type == TRI_COL_TYPE_EDGE && _isSmart) {
          return Result(TRI_ERROR_NOT_IMPLEMENTED, "Changing replicationFactor "
                        "not supported for smart edge collections");
        } else if (isSatellite()) {
          return Result(TRI_ERROR_FORBIDDEN, "Satellite collection, "
                        "cannot change replicationFactor");
        }
      }
    }
    else if (rfSl.isString()) {
      if (rfSl.compareString("satellite") != 0) {
        // only the string "satellite" is allowed here
        return Result(TRI_ERROR_BAD_PARAMETER, "bad value for satellite");
      }
      // we got the string "satellite"...
#ifdef USE_ENTERPRISE
      if (!isSatellite()) {
        // but the collection is not a satellite collection!
        return Result(TRI_ERROR_FORBIDDEN, "cannot change satellite collection status");
      }
#else
      return Result(TRI_ERROR_FORBIDDEN, "cannot use satellite collection status");
#endif
      // fallthrough here if we set the string "satellite" for a satellite collection
      TRI_ASSERT(isSatellite() && _sharding->replicationFactor() == 0 && rf == 0);
    } else {
      return Result(TRI_ERROR_BAD_PARAMETER, "bad value for replicationFactor");
    }
  }

  auto doSync = !engine->inRecovery() && databaseFeature->forceSyncProperties();

  // The physical may first reject illegal properties.
  // After this call it either has thrown or the properties are stored
  Result res = getPhysical()->updateProperties(slice, doSync);
  if (!res.ok()) {
    return res;
  }

  TRI_ASSERT(!isSatellite() || rf == 0);
  _waitForSync = Helper::getBooleanValue(slice, "waitForSync", _waitForSync);
  _sharding->replicationFactor(rf);

  if (ServerState::instance()->isCoordinator()) {
    // We need to inform the cluster as well
    return ClusterInfo::instance()->setCollectionPropertiesCoordinator(
      vocbase().name(), std::to_string(id()), this
    );
  }

  engine->changeCollection(vocbase(), id(), *this, doSync);

  if (DatabaseFeature::DATABASE != nullptr &&
      DatabaseFeature::DATABASE->versionTracker() != nullptr) {
    DatabaseFeature::DATABASE->versionTracker()->track("change collection");
  }

  return {};
}

/// @brief return the figures for a collection
std::shared_ptr<arangodb::velocypack::Builder> LogicalCollection::figures() const {
  return getPhysical()->figures();
}

/// @brief opens an existing collection
void LogicalCollection::open(bool ignoreErrors) {
  getPhysical()->open(ignoreErrors);
  TRI_UpdateTickServer(id());
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

std::shared_ptr<Index> LogicalCollection::createIndex(VPackSlice const& info,
                                                      bool& created) {
  auto idx = _physical->createIndex(info, /*restore*/ false, created);
  if (idx) {
    if (DatabaseFeature::DATABASE != nullptr &&
        DatabaseFeature::DATABASE->versionTracker() != nullptr) {
      DatabaseFeature::DATABASE->versionTracker()->track("create index");
    }
  }
  return idx;
}

/// @brief drops an index, including index file removal and replication
bool LogicalCollection::dropIndex(TRI_idx_iid_t iid) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(_vocbase);
#endif
  arangodb::aql::QueryCache::instance()->invalidate(&vocbase(), name());

  bool result = _physical->dropIndex(iid);

  if (result) {
    if (DatabaseFeature::DATABASE != nullptr &&
        DatabaseFeature::DATABASE->versionTracker() != nullptr) {
      DatabaseFeature::DATABASE->versionTracker()->track("drop index");
    }
  }
  return result;
}

/// @brief Persist the connected physical collection.
///        This should be called AFTER the collection is successfully
///        created and only on Single/DBServer
void LogicalCollection::persistPhysicalCollection() {
  // Coordinators are not allowed to have local collections!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto path = engine->createCollection(vocbase(), id(), *this);

  getPhysical()->setPath(path);
}

/// @brief Defer a callback to be executed when the collection
///        can be dropped. The callback is supposed to drop
///        the collection and it is guaranteed that no one is using
///        it at that moment.
void LogicalCollection::deferDropCollection(
    std::function<bool(LogicalCollection&)> const& callback) {
  _physical->deferDropCollection(callback);
}

/// @brief reads an element from the document collection
Result LogicalCollection::read(transaction::Methods* trx, StringRef const& key,
                               ManagedDocumentResult& result, bool lock) {
  TRI_IF_FAILURE("LogicalCollection::read") {
    return Result(TRI_ERROR_DEBUG);
  }
  return getPhysical()->read(trx, key, result, lock);
}

Result LogicalCollection::read(transaction::Methods* trx, arangodb::velocypack::Slice const& key,
            ManagedDocumentResult& result, bool lock) {
  TRI_IF_FAILURE("LogicalCollection::read") {
    return Result(TRI_ERROR_DEBUG);
  }
  return getPhysical()->read(trx, key, result, lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a truncate operation (note: currently this only clears
/// the read-cache
////////////////////////////////////////////////////////////////////////////////

Result LogicalCollection::truncate(
    transaction::Methods& trx,
    OperationOptions& options
) {
  TRI_IF_FAILURE("LogicalCollection::truncate") {
    return Result(TRI_ERROR_DEBUG);
  }

  return getPhysical()->truncate(trx, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document or edge into the collection
////////////////////////////////////////////////////////////////////////////////

Result LogicalCollection::insert(
    transaction::Methods* trx, VPackSlice const slice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_tick_t& revisionId,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock) {
  TRI_IF_FAILURE("LogicalCollection::insert") {
    return Result(TRI_ERROR_DEBUG);
  }
  resultMarkerTick = 0;
  return getPhysical()->insert(trx, slice, result, options, resultMarkerTick,
                               lock, revisionId, keyLockInfo, std::move(callbackDuringLock));
}

/// @brief updates a document or edge in a collection
Result LogicalCollection::update(
    transaction::Methods* trx, VPackSlice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous,
    std::function<Result(void)> callbackDuringLock) {
  TRI_IF_FAILURE("LogicalCollection::update") {
    return Result(TRI_ERROR_DEBUG);
  }

  resultMarkerTick = 0;
  if (!newSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  prevRev = 0;
  VPackSlice key = newSlice.get(StaticStrings::KeyString);
  if (key.isNone()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  return getPhysical()->update(trx, newSlice, result, options, resultMarkerTick,
                               lock, prevRev, previous, key,
                               std::move(callbackDuringLock));
}

/// @brief replaces a document or edge in a collection
Result LogicalCollection::replace(
    transaction::Methods* trx, VPackSlice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous,
    std::function<Result(void)> callbackDuringLock) {
  TRI_IF_FAILURE("LogicalCollection::replace") {
    return Result(TRI_ERROR_DEBUG);
  }
  resultMarkerTick = 0;
  if (!newSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  prevRev = 0;
  return getPhysical()->replace(trx, newSlice, result, options,
                                resultMarkerTick, lock, prevRev, previous,
                                std::move(callbackDuringLock));
}

/// @brief removes a document or edge
Result LogicalCollection::remove(
    transaction::Methods& trx,
    velocypack::Slice const slice,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock
) {
  TRI_IF_FAILURE("LogicalCollection::remove") {
    return Result(TRI_ERROR_DEBUG);
  }
  resultMarkerTick = 0;
  TRI_voc_rid_t revisionId = 0;
  return getPhysical()->remove(trx, slice, previous, options, resultMarkerTick,
                               lock, prevRev, revisionId, keyLockInfo,
                               std::move(callbackDuringLock));
}

bool LogicalCollection::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& token,
                                     ManagedDocumentResult& result) const {
  return getPhysical()->readDocument(trx, token, result);
}

bool LogicalCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 LocalDocumentId const& token,
                                                 IndexIterator::DocumentCallback const& cb) const {
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

// SECTION: Key Options
VPackSlice LogicalCollection::keyOptions() const {
  if (_keyOptions == nullptr) {
    return arangodb::velocypack::Slice::nullSlice();
  }
  return VPackSlice(_keyOptions->data());
}

ChecksumResult LogicalCollection::checksum(bool withRevisions, bool withData) const {
  auto ctx = transaction::StandaloneContext::Create(vocbase());
  SingleCollectionTransaction trx(ctx, *this, AccessMode::Type::READ);
  Result res = trx.begin();

  if (!res.ok()) {
    return ChecksumResult(std::move(res));
  }

  trx.pinData(id()); // will throw when it fails

  // get last tick
  LogicalCollection* collection = trx.documentCollection();
  auto physical = collection->getPhysical();
  TRI_ASSERT(physical != nullptr);
  std::string const revisionId = TRI_RidToString(physical->revision(&trx));
  uint64_t hash = 0;

  trx.invokeOnAllElements(name(), [&hash, &withData, &withRevisions, &trx, &collection](LocalDocumentId const& token) {
    collection->readDocumentWithCallback(&trx, token, [&](LocalDocumentId const&, VPackSlice slice) {
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
    });
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
