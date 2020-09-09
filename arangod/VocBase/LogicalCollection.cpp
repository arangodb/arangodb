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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/Mutex.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "Replication/ReplicationFeature.h"
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
#include "VocBase/Validators.h"

#include <velocypack/Collection.h>
#include <velocypack/StringRef.h>
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
  auto guid = arangodb::basics::VelocyPackHelper::getStringValue(info, arangodb::StaticStrings::DataSourceGuid,
                                                                 empty);

  if (!guid.empty()) {
    return guid;
  }

  auto version = arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
      info, StaticStrings::Version,
      static_cast<uint32_t>(LogicalCollection::currentVersion()));

  // predictable UUID for legacy collections
  if (static_cast<LogicalCollection::Version>(version) < LogicalCollection::Version::v33 && info.isObject()) {
    return arangodb::basics::VelocyPackHelper::getStringValue(info, arangodb::StaticStrings::DataSourceName,
                                                              empty);
  }

  return empty;
}

arangodb::LogicalDataSource::Type const& readType(arangodb::velocypack::Slice info,
                                                  std::string const& key,
                                                  TRI_col_type_e def) {
  static const auto& document = arangodb::LogicalDataSource::Type::emplace(
      arangodb::velocypack::StringRef("document"));
  static const auto& edge =
      arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("edge"));

  // arbitrary system-global value for unknown
  static const auto& unknown =
      arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef());

  switch (Helper::getNumericValue<TRI_col_type_e, int>(info, key, def)) {
    case TRI_col_type_e::TRI_COL_TYPE_DOCUMENT:
      return document;
    case TRI_col_type_e::TRI_COL_TYPE_EDGE:
      return edge;
    default:
      return unknown;
  }
}

}  // namespace

// The Slice contains the part of the plan that
// is relevant for this collection.
LogicalCollection::LogicalCollection(TRI_vocbase_t& vocbase, VPackSlice const& info,
                                     bool isAStub, uint64_t planVersion /*= 0*/
                                     )
    : LogicalDataSource(
          LogicalCollection::category(),
          ::readType(info, StaticStrings::DataSourceType, TRI_COL_TYPE_UNKNOWN),
          vocbase, Helper::extractIdValue(info), ::readGloballyUniqueId(info),
          Helper::stringUInt64(info.get(StaticStrings::DataSourcePlanId)),
          Helper::getStringValue(info, StaticStrings::DataSourceName, ""), planVersion,
          TRI_vocbase_t::IsSystemName(
              Helper::getStringValue(info, StaticStrings::DataSourceName, "")) &&
              Helper::getBooleanValue(info, StaticStrings::DataSourceSystem, false),
          Helper::getBooleanValue(info, StaticStrings::DataSourceDeleted, false)),
      _version(static_cast<Version>(Helper::getNumericValue<uint32_t>(
          info, StaticStrings::Version, static_cast<uint32_t>(currentVersion())))),
      _v8CacheVersion(0),
      _type(Helper::getNumericValue<TRI_col_type_e, int>(info, StaticStrings::DataSourceType,
                                                         TRI_COL_TYPE_UNKNOWN)),
      _status(Helper::getNumericValue<TRI_vocbase_col_status_e, int>(
          info, "status", TRI_VOC_COL_STATUS_CORRUPTED)),
      _isAStub(isAStub),
#ifdef USE_ENTERPRISE
      _isDisjoint(Helper::getBooleanValue(info, StaticStrings::IsDisjoint, false)),
      _isSmart(Helper::getBooleanValue(info, StaticStrings::IsSmart, false)),
      _isSmartChild(Helper::getBooleanValue(info, StaticStrings::IsSmartChild, false)),
#endif
      _waitForSync(Helper::getBooleanValue(info, StaticStrings::WaitForSyncString, false)),
      _allowUserKeys(Helper::getBooleanValue(info, "allowUserKeys", true)),
      _usesRevisionsAsDocumentIds(
          Helper::getBooleanValue(info, StaticStrings::UsesRevisionsAsDocumentIds, false)),
      _syncByRevision(determineSyncByRevision()),
      _minRevision((system() || isSmartChild())
                       ? 0
                       : Helper::getNumericValue<TRI_voc_rid_t>(info, StaticStrings::MinRevision,
                                                                0)),
#ifdef USE_ENTERPRISE
      _smartJoinAttribute(
          Helper::getStringValue(info, StaticStrings::SmartJoinAttribute, "")),
#endif
      _physical(EngineSelectorFeature::ENGINE->createPhysicalCollection(*this, info)) {

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

  auto res = updateValidators(info.get(StaticStrings::Schema));
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(!guid().empty());

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));

  // add keyOptions from slice
  VPackSlice keyOpts = info.get("keyOptions");
  _keyGenerator.reset(KeyGenerator::factory(vocbase.server(), keyOpts));
  if (!keyOpts.isNone()) {
    _keyOptions = VPackBuilder::clone(keyOpts).steal();
  }

  _sharding = std::make_unique<ShardingInfo>(info, this);

#ifdef USE_ENTERPRISE
  if (ServerState::instance()->isCoordinator() || ServerState::instance()->isDBServer()) {
    if (!info.get(StaticStrings::SmartJoinAttribute).isNone() && !hasSmartJoinAttribute()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE,
          "smartJoinAttribute must contain a string attribute name");
    }

    if (hasSmartJoinAttribute()) {
      auto const& sk = _sharding->shardKeys();
      TRI_ASSERT(!sk.empty());

      if (sk.size() != 1) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE,
            "smartJoinAttribute can only be used for collections with a single "
            "shardKey value");
      }
      TRI_ASSERT(!sk.front().empty());
      if (sk.front().back() != ':') {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE, std::string("smartJoinAttribute can only be used for shardKeys ending on ':', got '") +
                                                                                   sk.front() +
                                                                                   "'");
      }

      if (isSmart()) {
        if (_type == TRI_COL_TYPE_EDGE) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE,
              "cannot use smartJoinAttribute on a smart edge collection");
        } else if (_type == TRI_COL_TYPE_DOCUMENT) {
          VPackSlice sga = info.get(StaticStrings::GraphSmartGraphAttribute);
          if (sga.isString() &&
              sga.copyString() != info.get(StaticStrings::SmartJoinAttribute).copyString()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE,
                "smartJoinAttribute must be equal to smartGraphAttribute");
          }
        }
      }
    }
  }
#else
  // whatever we got passed in, in a non-Enterprise Edition build, we just
  // ignore any specification for the smartJoinAttribute
  _smartJoinAttribute.clear();
#endif

  if (ServerState::instance()->isDBServer() ||
      !ServerState::instance()->isRunningInCluster()) {
    _followers.reset(new FollowerInfo(this));
  }

  TRI_ASSERT(_physical != nullptr);
  // This has to be called AFTER _phyiscal and _logical are properly linked
  // together.

  if (_physical->didPartialUpgrade()) {
    _physical->cleanupAfterUpgrade();
  }

  prepareIndexes(info.get("indexes"));
}

/*static*/ LogicalDataSource::Category const& LogicalCollection::category() noexcept {
  static const Category category;

  return category;
}

Result LogicalCollection::updateValidators(VPackSlice validatorSlice) {
  using namespace std::literals::string_literals;
  if (validatorSlice.isNone()) {
    return { TRI_ERROR_NO_ERROR };
  } 
  if (validatorSlice.isNull()) {
    validatorSlice = VPackSlice::emptyObjectSlice();
  }
  if (!validatorSlice.isObject()) {
    return {TRI_ERROR_VALIDATION_BAD_PARAMETER, "Schema description is not an object."};
  }

  TRI_ASSERT(validatorSlice.isObject());

  auto newVec = std::make_shared<ValidatorVec>();

  // delete validators if empty object is given
  if (!validatorSlice.isEmptyObject()) {
    try {
      auto validator = std::make_unique<ValidatorJsonSchema>(validatorSlice);
      newVec->push_back(std::move(validator));
    } catch (std::exception const& ex) {
      return { TRI_ERROR_VALIDATION_BAD_PARAMETER, "Error when building schema: "s + ex.what() };
    }
  }

  std::atomic_store_explicit(&_validators, newVec, std::memory_order_relaxed);

  return { TRI_ERROR_NO_ERROR };
}

LogicalCollection::~LogicalCollection() = default;

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

size_t LogicalCollection::writeConcern() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->writeConcern();
}

std::string LogicalCollection::distributeShardsLike() const {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->distributeShardsLike();
}

void LogicalCollection::distributeShardsLike(std::string const& cid,
                                             ShardingInfo const* other) {
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
                                           VPackStringRef const& key) {
  TRI_ASSERT(_sharding != nullptr);
  return _sharding->getResponsibleShard(slice, docComplete, shardID,
                                        usesDefaultShardKeys, key);
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

std::unique_ptr<IndexIterator> LogicalCollection::getAllIterator(transaction::Methods* trx) {
  return _physical->getAllIterator(trx);
}

std::unique_ptr<IndexIterator> LogicalCollection::getAnyIterator(transaction::Methods* trx) {
  return _physical->getAnyIterator(trx);
}
// @brief Return the number of documents in this collection
uint64_t LogicalCollection::numberDocuments(transaction::Methods* trx,
                                            transaction::CountType type) {
  // detailed results should have been handled in the levels above us
  TRI_ASSERT(type != transaction::CountType::Detailed);

  uint64_t documents = transaction::CountCache::NotPopulated;
  if (type == transaction::CountType::ForceCache) {
    // always return from the cache, regardless what's in it
    documents = _countCache.get();
  } else if (type == transaction::CountType::TryCache) {
    documents = _countCache.get(transaction::CountCache::Ttl);
  }
  if (documents == transaction::CountCache::NotPopulated) {
    documents = getPhysical()->numberDocuments(trx);
    _countCache.store(documents);
  }
  TRI_ASSERT(documents != transaction::CountCache::NotPopulated);
  return documents;
}

bool LogicalCollection::hasClusterWideUniqueRevs() const {
  return version() >= Version::v37 && isSmartChild();
}

uint32_t LogicalCollection::v8CacheVersion() const { return _v8CacheVersion; }

TRI_col_type_e LogicalCollection::type() const { return _type; }

TRI_vocbase_col_status_e LogicalCollection::status() const { return _status; }

TRI_vocbase_col_status_e LogicalCollection::getStatusLocked() {
  READ_LOCKER(readLocker, _statusLock);
  return _status;
}

void LogicalCollection::executeWhileStatusWriteLocked(std::function<void()> const& callback) {
  WRITE_LOCKER_EVENTUAL(locker, _statusLock);
  callback();
}

TRI_vocbase_col_status_e LogicalCollection::tryFetchStatus(bool& didFetch) {
  TRY_READ_LOCKER(locker, _statusLock);
  if (locker.isLocked()) {
    didFetch = true;
    return _status;
  }
  didFetch = false;
  return TRI_VOC_COL_STATUS_CORRUPTED;
}

// SECTION: Properties
TRI_voc_rid_t LogicalCollection::revision(transaction::Methods* trx) const {
  // TODO CoordinatorCase
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  return _physical->revision(trx);
}

bool LogicalCollection::usesRevisionsAsDocumentIds() const {
  return _usesRevisionsAsDocumentIds.load();
}

void LogicalCollection::setUsesRevisionsAsDocumentIds(bool usesRevisions) {
  if (!_usesRevisionsAsDocumentIds.load() && usesRevisions && _version >= Version::v37) {
    _usesRevisionsAsDocumentIds.store(true);
  }
}

TRI_voc_rid_t LogicalCollection::minRevision() const {
  return _minRevision;
}

std::unique_ptr<FollowerInfo> const& LogicalCollection::followers() const {
  return _followers;
}

bool LogicalCollection::syncByRevision() const {
  return _syncByRevision.load();
}

void LogicalCollection::setSyncByRevision(bool usesRevisions) {
  if (!_syncByRevision.load() && _usesRevisionsAsDocumentIds.load() && usesRevisions) {
    _syncByRevision.store(true);
  }
}

bool LogicalCollection::useSyncByRevision() const {
  return !_isAStub && _syncByRevision.load();
}

bool LogicalCollection::determineSyncByRevision() const {
  if (version() >= LogicalCollection::Version::v37) {
    auto& server = vocbase().server();
    if (server.hasFeature<EngineSelectorFeature>() &&
        server.hasFeature<ReplicationFeature>()) {
      auto& engine = server.getFeature<EngineSelectorFeature>();
      auto& replication = server.getFeature<ReplicationFeature>();
      return engine.isRocksDB() && replication.syncByRevision() &&
             usesRevisionsAsDocumentIds();
    }
  }
  return false;
}

IndexEstMap LogicalCollection::clusterIndexEstimates(bool allowUpdating, TRI_voc_tid_t tid) {
  return getPhysical()->clusterIndexEstimates(allowUpdating, tid);
}

void LogicalCollection::flushClusterIndexEstimates() {
  getPhysical()->flushClusterIndexEstimates();
}

std::vector<std::shared_ptr<arangodb::Index>> LogicalCollection::getIndexes() const {
  return getPhysical()->getIndexes();
}

void LogicalCollection::getIndexesVPack(
    VPackBuilder& result,
    std::function<bool(arangodb::Index const*, std::underlying_type<Index::Serialize>::type&)> const& filter) const {
  getPhysical()->getIndexesVPack(result, filter);
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

  if (!vocbase().server().hasFeature<DatabaseFeature>()) {
    return Result(
        TRI_ERROR_INTERNAL,
        "failed to find feature 'Database' while renaming collection");
  }
  auto& databaseFeature = vocbase().server().getFeature<DatabaseFeature>();

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

  auto doSync = databaseFeature.forceSyncProperties();
  std::string oldName = name();

  // Okay we can finally rename safely
  try {
    StorageEngine& engine =
        vocbase().server().getFeature<EngineSelectorFeature>().engine();
    name(std::move(newName));
    engine.changeCollection(vocbase(), *this, doSync);
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
  increaseV8Version();
  return TRI_ERROR_NO_ERROR;
}

int LogicalCollection::close() {
  // This was unload() in 3.0
  return getPhysical()->close();
}

void LogicalCollection::load() { _physical->load(); }

void LogicalCollection::unload() { _physical->unload(); }

arangodb::Result LogicalCollection::drop() {
  // make sure collection has been closed
  this->close();

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  deleted(true);
  _physical->drop();

  return arangodb::Result();
}

void LogicalCollection::setStatus(TRI_vocbase_col_status_e status) {
  _status = status;

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    increaseV8Version();
  }
}

void LogicalCollection::toVelocyPackForClusterInventory(VPackBuilder& result,
                                                        bool useSystem, bool isReady,
                                                        bool allInSync) const {
  if (system() && !useSystem) {
    return;
  }

  result.openObject();
  result.add(VPackValue("parameters"));

  std::unordered_set<std::string> ignoreKeys{"allowUserKeys",
                                             "cid",
                                             "count",
                                             "statusString",
                                             StaticStrings::Version,
                                             "distributeShardsLike",
                                             StaticStrings::ObjectId,
                                             StaticStrings::Indexes};
  VPackBuilder params = toVelocyPackIgnore(ignoreKeys, Serialization::List);
  {
    VPackObjectBuilder guard(&result);

    for (auto const& p : VPackObjectIterator(params.slice())) {
      result.add(p.key);
      result.add(p.value);
    }

    if (!_sharding->distributeShardsLike().empty()) {
      CollectionNameResolver resolver(vocbase());

      result.add(StaticStrings::DistributeShardsLike,
                 VPackValue(resolver.getCollectionNameCluster(static_cast<TRI_voc_cid_t>(
                     basics::StringUtils::uint64(distributeShardsLike())))));
    }
  }

  result.add(VPackValue("indexes"));
  getIndexesVPack(result, [](Index const* idx, uint8_t& flags) {
    // we have to exclude the primary and the edge index here, because otherwise
    // at least the MMFiles engine will try to create it
    // AND exclude hidden indexes
    switch (idx->type()) {
      case Index::TRI_IDX_TYPE_PRIMARY_INDEX:
      case Index::TRI_IDX_TYPE_EDGE_INDEX:
        return false;
      default:
        flags = Index::makeFlags();
        return !idx->isHidden() && !idx->inProgress();
    }
  });
  result.add("planVersion", VPackValue(planVersion()));
  result.add("isReady", VPackValue(isReady));
  result.add("allInSync", VPackValue(allInSync));
  result.close();  // CollectionInfo
}

arangodb::Result LogicalCollection::appendVelocyPack(arangodb::velocypack::Builder& result,
                                                     Serialization context) const {
  bool const forPersistence = (context == Serialization::Persistence || context == Serialization::PersistenceWithInProgress);
  bool const showInProgress = (context == Serialization::PersistenceWithInProgress);

  // We write into an open object
  TRI_ASSERT(result.isOpenObject());

  // Collection Meta Information
  result.add("cid", VPackValue(std::to_string(id())));
  result.add(StaticStrings::DataSourceType, VPackValue(static_cast<int>(_type)));
  result.add("status", VPackValue(_status));
  result.add("statusString", VPackValue(::translateStatus(_status)));
  result.add(StaticStrings::Version, VPackValue(static_cast<uint32_t>(_version)));

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
  auto indexFlags = Index::makeFlags();
  // hide hidden indexes. In effect hides unfinished indexes,
  // and iResearch links (only on a single-server and coordinator)
  if (forPersistence) {
    indexFlags = Index::makeFlags(Index::Serialize::Internals);
  }
  auto filter = [indexFlags, forPersistence, showInProgress](arangodb::Index const* idx, decltype(Index::makeFlags())& flags) {
    if ((forPersistence || !idx->isHidden()) && (showInProgress || !idx->inProgress())) {
      flags = indexFlags;
      return true;
    }

    return false;
  };
  getIndexesVPack(result, filter);

  // Validators
  {
    result.add(VPackValue(StaticStrings::Schema));
    validatorsToVelocyPack(result);
  }

  // Cluster Specific
  result.add(StaticStrings::IsDisjoint, VPackValue(isDisjoint()));
  result.add(StaticStrings::IsSmart, VPackValue(isSmart()));
  result.add(StaticStrings::IsSmartChild, VPackValue(isSmartChild()));
  /*result.add(StaticStrings::UsesRevisionsAsDocumentIds,
             VPackValue(usesRevisionsAsDocumentIds()));
  result.add(StaticStrings::MinRevision, VPackValue(minRevision()));
  result.add(StaticStrings::SyncByRevision, VPackValue(syncByRevision()));*/

  if (hasSmartJoinAttribute()) {
    result.add(StaticStrings::SmartJoinAttribute, VPackValue(_smartJoinAttribute));
  }

  if (!forPersistence) {
    // with 'forPersistence' added by LogicalDataSource::toVelocyPack
    // FIXME TODO is this needed in !forPersistence???
    result.add(StaticStrings::DataSourcePlanId, VPackValue(std::to_string(planId())));
  }

  _sharding->toVelocyPack(result, Serialization::List != context);

  includeVelocyPackEnterprise(result);

  TRI_ASSERT(result.isOpenObject());
  // We leave the object open

  return arangodb::Result();
}

void LogicalCollection::toVelocyPackIgnore(VPackBuilder& result,
                                           std::unordered_set<std::string> const& ignoreKeys,
                                           Serialization context) const {
  TRI_ASSERT(result.isOpenObject());
  VPackBuilder b = toVelocyPackIgnore(ignoreKeys, context);
  result.add(VPackObjectIterator(b.slice()));
}

VPackBuilder LogicalCollection::toVelocyPackIgnore(std::unordered_set<std::string> const& ignoreKeys,
                                                   Serialization context) const {
  VPackBuilder full;
  full.openObject();
  properties(full, context);
  full.close();
  if (ignoreKeys.empty()) {
    return full;
  }
  return VPackCollection::remove(full.slice(), ignoreKeys);
}

void LogicalCollection::includeVelocyPackEnterprise(VPackBuilder&) const {
  // We ain't no Enterprise Edition
}

void LogicalCollection::increaseV8Version() { ++_v8CacheVersion; }

arangodb::Result LogicalCollection::properties(velocypack::Slice const& slice, bool) {
  // the following collection properties are intentionally not updated,
  // as updating them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...
  if (!vocbase().server().hasFeature<DatabaseFeature>()) {
    return Result(
        TRI_ERROR_INTERNAL,
        "failed to find feature 'Database' while updating collection");
  }
  auto& databaseFeature = vocbase().server().getFeature<DatabaseFeature>();

  if (!vocbase().server().hasFeature<EngineSelectorFeature>() ||
      !vocbase().server().getFeature<EngineSelectorFeature>().selected()) {
    return Result(TRI_ERROR_INTERNAL,
                  "failed to find a storage engine while updating collection");
  }
  auto& engine = vocbase().server().getFeature<EngineSelectorFeature>().engine();

  MUTEX_LOCKER(guard, _infoLock);  // prevent simultaneous updates

  auto res = updateValidators(slice.get(StaticStrings::Schema));
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  size_t replicationFactor = _sharding->replicationFactor();
  size_t writeConcern = _sharding->writeConcern();
  VPackSlice replicationFactorSlice = slice.get(StaticStrings::ReplicationFactor);

  VPackSlice writeConcernSlice = slice.get(StaticStrings::WriteConcern);
  if (writeConcernSlice.isNone()) { // deprecated in 3.6
    writeConcernSlice = slice.get(StaticStrings::MinReplicationFactor);
  }

  if (!replicationFactorSlice.isNone()) {
    if (replicationFactorSlice.isInteger()) {
      int64_t replicationFactorTest = replicationFactorSlice.getNumber<int64_t>();
      if (replicationFactorTest < 0) {
        // negative value for replication factor... not good
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "bad value for replicationFactor");
      }

      replicationFactor = replicationFactorSlice.getNumber<size_t>();
      if ((!isSatellite() && replicationFactor == 0) || replicationFactor > 10) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "bad value for replicationFactor");
      }

      if (ServerState::instance()->isCoordinator() &&
          replicationFactor != _sharding->replicationFactor()) {  // sanity checks
        if (!_sharding->distributeShardsLike().empty()) {
          return Result(TRI_ERROR_FORBIDDEN,
                        "cannot change replicationFactor for a collection using 'distributeShardsLike'");
        } else if (_type == TRI_COL_TYPE_EDGE && isSmart()) {
          return Result(TRI_ERROR_NOT_IMPLEMENTED,
                        "changing replicationFactor is "
                        "not supported for smart edge collections");
        } else if (isSatellite()) {
          return Result(TRI_ERROR_FORBIDDEN,
                        "cannot change replicationFactor of a SatelliteCollection");
        }
      }
    } else if (replicationFactorSlice.isString()) {
      if (replicationFactorSlice.compareString(StaticStrings::Satellite) != 0) {
        // only the string "satellite" is allowed here
        return Result(TRI_ERROR_BAD_PARAMETER, "bad value for satellite");
      }
      // we got the string "satellite"...
#ifdef USE_ENTERPRISE
      if (!isSatellite()) {
        // but the collection is not a SatelliteCollection!
        return Result(TRI_ERROR_FORBIDDEN,
                      "cannot change SatelliteCollection status");
      }
#else
      return Result(TRI_ERROR_FORBIDDEN,
                    "cannot use SatelliteCollection status");
#endif
      // fallthrough here if we set the string "satellite" for a satellite
      // collection
      TRI_ASSERT(isSatellite() && _sharding->replicationFactor() == 0 && replicationFactor == 0);
    } else {
      return Result(TRI_ERROR_BAD_PARAMETER, "bad value for replicationFactor");
    }
  }

  if (!writeConcernSlice.isNone()) {
    if (writeConcernSlice.isInteger()) {
      int64_t writeConcernTest = writeConcernSlice.getNumber<int64_t>();
      if (writeConcernTest < 0) {
        // negative value for writeConcern... not good
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "bad value for writeConcern");
      }

      writeConcern = writeConcernSlice.getNumber<size_t>();
      if (writeConcern > replicationFactor) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "bad value for writeConcern");
      }

      if (ServerState::instance()->isCoordinator() &&
          replicationFactor != _sharding->writeConcern()) {  // sanity checks
        if (!_sharding->distributeShardsLike().empty()) {
          return Result(TRI_ERROR_FORBIDDEN,
                        "Cannot change writeConcern, please change " +
                            _sharding->distributeShardsLike());
        } else if (_type == TRI_COL_TYPE_EDGE && isSmart()) {
          return Result(TRI_ERROR_NOT_IMPLEMENTED,
                        "Changing writeConcern "
                        "not supported for smart edge collections");
        } else if (isSatellite()) {
          return Result(TRI_ERROR_FORBIDDEN,
                        "SatelliteCollection, "
                        "cannot change writeConcern");
        }
      }
    } else {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "bad value for writeConcern");
    }
    TRI_ASSERT((writeConcern <= replicationFactor && !isSatellite()) || (writeConcern == 0 && isSatellite()));
  }

  auto doSync = !engine.inRecovery() && databaseFeature.forceSyncProperties();

  // The physical may first reject illegal properties.
  // After this call it either has thrown or the properties are stored
  res = getPhysical()->updateProperties(slice, doSync);
  if (!res.ok()) {
    return res;
  }

  TRI_ASSERT(!isSatellite() || replicationFactor == 0);
  _waitForSync = Helper::getBooleanValue(slice, "waitForSync", _waitForSync);
  _sharding->setWriteConcernAndReplicationFactor(writeConcern, replicationFactor);

  if (ServerState::instance()->isCoordinator()) {
    // We need to inform the cluster as well
    auto& ci = vocbase().server().getFeature<ClusterFeature>().clusterInfo();
    return ci.setCollectionPropertiesCoordinator(vocbase().name(),
                                                 std::to_string(id()), this);
  }

  engine.changeCollection(vocbase(), *this, doSync);

  if (DatabaseFeature::DATABASE != nullptr &&
      DatabaseFeature::DATABASE->versionTracker() != nullptr) {
    DatabaseFeature::DATABASE->versionTracker()->track("change collection");
  }

  return {};
}

/// @brief return the figures for a collection
futures::Future<OperationResult> LogicalCollection::figures(bool details) const {
  return getPhysical()->figures(details);
}

/// SECTION Indexes

std::shared_ptr<Index> LogicalCollection::lookupIndex(IndexId idxId) const {
  return getPhysical()->lookupIndex(idxId);
}

std::shared_ptr<Index> LogicalCollection::lookupIndex(std::string const& idxName) const {
  return getPhysical()->lookupIndex(idxName);
}

std::shared_ptr<Index> LogicalCollection::lookupIndex(VPackSlice const& info) const {
  if (!info.isObject()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return getPhysical()->lookupIndex(info);
}

std::shared_ptr<Index> LogicalCollection::createIndex(VPackSlice const& info, bool& created) {
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
bool LogicalCollection::dropIndex(IndexId iid) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(_vocbase);
#endif

  arangodb::aql::QueryCache::instance()->invalidate(&vocbase(), guid());

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
  engine->createCollection(vocbase(), *this);
}

basics::ReadWriteLock& LogicalCollection::statusLock() {
  return _statusLock;
}

/// @brief Defer a callback to be executed when the collection
///        can be dropped. The callback is supposed to drop
///        the collection and it is guaranteed that no one is using
///        it at that moment.
void LogicalCollection::deferDropCollection(std::function<bool(LogicalCollection&)> const& callback) {
  _syncByRevision = false;  // safety to make sure we can do physical cleanup
  _physical->deferDropCollection(callback);
}

/// @brief reads an element from the document collection
Result LogicalCollection::read(transaction::Methods* trx,
                               arangodb::velocypack::StringRef const& key,
                               ManagedDocumentResult& result) {
  TRI_IF_FAILURE("LogicalCollection::read") { return Result(TRI_ERROR_DEBUG); }
  return getPhysical()->read(trx, key, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a truncate operation (note: currently this only clears
/// the read-cache
////////////////////////////////////////////////////////////////////////////////

Result LogicalCollection::truncate(transaction::Methods& trx, OperationOptions& options) {
  TRI_IF_FAILURE("LogicalCollection::truncate") {
    return Result(TRI_ERROR_DEBUG);
  }

  return getPhysical()->truncate(trx, options);
}

/// @brief compact-data operation
Result LogicalCollection::compact() { return getPhysical()->compact(); }

Result LogicalCollection::lookupKey(transaction::Methods* trx, VPackStringRef key,
                                    std::pair<LocalDocumentId, TRI_voc_rid_t>& result) const { 
  return getPhysical()->lookupKey(trx, key, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document or edge into the collection
////////////////////////////////////////////////////////////////////////////////

Result LogicalCollection::insert(transaction::Methods* trx, VPackSlice const slice,
                                 ManagedDocumentResult& result, OperationOptions& options) {
  TRI_IF_FAILURE("LogicalCollection::insert") {
    return Result(TRI_ERROR_DEBUG);
  }
  return getPhysical()->insert(trx, slice, result, options);
}

/// @brief updates a document or edge in a collection
Result LogicalCollection::update(transaction::Methods* trx, VPackSlice newSlice,
                                 ManagedDocumentResult& result, OperationOptions& options,
                                 ManagedDocumentResult& previous) {
  TRI_IF_FAILURE("LogicalCollection::update") {
    return Result(TRI_ERROR_DEBUG);
  }

  if (!newSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  return getPhysical()->update(trx, newSlice, result, options, previous);
}

/// @brief replaces a document or edge in a collection
Result LogicalCollection::replace(transaction::Methods* trx, VPackSlice newSlice,
                                  ManagedDocumentResult& result, OperationOptions& options,
                                  ManagedDocumentResult& previous) {
  TRI_IF_FAILURE("LogicalCollection::replace") {
    return Result(TRI_ERROR_DEBUG);
  }
  if (!newSlice.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  return getPhysical()->replace(trx, newSlice, result, options, previous);
}

/// @brief removes a document or edge
Result LogicalCollection::remove(transaction::Methods& trx, velocypack::Slice const slice,
                                 OperationOptions& options,
                                 ManagedDocumentResult& previous) {
  TRI_IF_FAILURE("LogicalCollection::remove") {
    return Result(TRI_ERROR_DEBUG);
  }
  return getPhysical()->remove(trx, slice, previous, options);
}

bool LogicalCollection::readDocument(transaction::Methods* trx, LocalDocumentId const& token,
                                     ManagedDocumentResult& result) const {
  return getPhysical()->readDocument(trx, token, result);
}

bool LogicalCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 LocalDocumentId const& token,
                                                 IndexIterator::DocumentCallback const& cb) const {
  return getPhysical()->readDocumentWithCallback(trx, token, cb);
}

/// @brief a method to skip certain documents in AQL write operations,
/// this is only used in the Enterprise Edition for SmartGraphs
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

void LogicalCollection::validatorsToVelocyPack(VPackBuilder& b) const {
  auto vals = std::atomic_load_explicit(&_validators, std::memory_order_relaxed);
  if (vals == nullptr || vals->empty()) {
    b.add(VPackSlice::nullSlice());
    return;
  }
  vals->front()->toVelocyPack(b);
}

Result LogicalCollection::validate(VPackSlice s, VPackOptions const* options) const {
  auto vals = std::atomic_load_explicit(&_validators, std::memory_order_relaxed);
  if (vals == nullptr) { return {}; }
  for(auto const& validator : *vals) {
    auto rv = validator->validate(s, VPackSlice::noneSlice(), true, options);
    if (rv.fail()) {
      return rv;
    }
  }
  return {};
}

Result LogicalCollection::validate(VPackSlice modifiedDoc, VPackSlice oldDoc, VPackOptions const* options) const {
  auto vals = std::atomic_load_explicit(&_validators, std::memory_order_relaxed);
  if (vals == nullptr) { return {}; }
  for(auto const& validator : *vals) {
    auto rv = validator->validate(modifiedDoc, oldDoc, false, options);
    if (rv.fail()) {
      return rv;
    }
  }
  return {};
}
