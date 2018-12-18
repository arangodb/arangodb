////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterCollection.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterMethods.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ClusterEngine/ClusterIndex.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesCollection.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using Helper = arangodb::basics::VelocyPackHelper;

namespace arangodb {

ClusterCollection::ClusterCollection(
  LogicalCollection& collection,
  ClusterEngineType engineType,
  arangodb::velocypack::Slice const& info
)
    : PhysicalCollection(collection, info),
      _engineType(engineType),
      _info(info),
      _selectivityEstimates(collection) {
  // duplicate all the error handling
  if (_engineType == ClusterEngineType::MMFilesEngine) {
    bool isVolatile =
        Helper::readBooleanValue(_info.slice(), "isVolatile", false);

    if (isVolatile && _logicalCollection.waitForSync()) {
      // Illegal collection configuration
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "volatile collections do not support the waitForSync option");
    }

    VPackSlice journalSlice = _info.slice().get("journalSize");

    if (journalSlice.isNone()) {
      // In some APIs maximalSize is allowed instead
      journalSlice = _info.slice().get("maximalSize");
    }

    if (journalSlice.isNumber()) {
      if (journalSlice.getNumericValue<uint32_t>() < TRI_JOURNAL_MINIMAL_SIZE) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "<properties>.journalSize too small");
      }
    }
  } else if (_engineType == ClusterEngineType::RocksDBEngine) {
    VPackSlice s = info.get("isVolatile");
    if (s.isBoolean() && s.getBoolean()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "volatile collections are unsupported in the RocksDB engine");
    }
  } else if (_engineType != ClusterEngineType::MockEngine) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
}

ClusterCollection::ClusterCollection(
    LogicalCollection& collection,
    PhysicalCollection const* physical
)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _engineType(static_cast<ClusterCollection const*>(physical)->_engineType),
      _info(static_cast<ClusterCollection const*>(physical)->_info),
      _selectivityEstimates(collection) {}

ClusterCollection::~ClusterCollection() {}
  
/// @brief fetches current index selectivity estimates
/// if allowUpdate is true, will potentially make a cluster-internal roundtrip to
/// fetch current values!
std::unordered_map<std::string, double> ClusterCollection::clusterIndexEstimates(bool allowUpdate) const {
  return _selectivityEstimates.get(allowUpdate);
}

/// @brief sets the current index selectivity estimates
void ClusterCollection::clusterIndexEstimates(std::unordered_map<std::string, double>&& estimates) {
  _selectivityEstimates.set(std::move(estimates));
}

/// @brief flushes the current index selectivity estimates
void ClusterCollection::flushClusterIndexEstimates() {
  _selectivityEstimates.flush();
}

std::string const& ClusterCollection::path() const {
  return StaticStrings::Empty;  // we do not have any path
}

void ClusterCollection::setPath(std::string const&) {
  // we do not have any path
}

Result ClusterCollection::updateProperties(VPackSlice const& slice,
                                           bool doSync) {
  VPackBuilder merge;
  merge.openObject();

  // duplicate all the error handling of the storage engines
  if (_engineType == ClusterEngineType::MMFilesEngine) {  // duplicate the error validation
    // validation
    uint32_t tmp = Helper::getNumericValue<uint32_t>(
        slice, "indexBuckets",
        2 /*Just for validation, this default Value passes*/);

    if (tmp == 0 || tmp > 1024) {
      return {TRI_ERROR_BAD_PARAMETER,
              "indexBuckets must be a two-power between 1 and 1024"};
    }

    bool isVolatile =
        Helper::readBooleanValue(_info.slice(), "isVolatile", false);
    if (isVolatile &&
        arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", _logicalCollection.waitForSync()
        )) {
      // the combination of waitForSync and isVolatile makes no sense
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "volatile collections do not support the waitForSync option");
    }

    if (isVolatile !=
        Helper::getBooleanValue(slice, "isVolatile", isVolatile)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
          "isVolatile option cannot be changed at runtime");
    }
    VPackSlice journalSlice = slice.get("journalSize");
    if (journalSlice.isNone()) {
      // In some APIs maximalSize is allowed instead
      journalSlice = slice.get("maximalSize");
    }
    uint32_t journalSize = Helper::getNumericValue<uint32_t>(_info.slice(), "journalSize", TRI_JOURNAL_DEFAULT_SIZE);
    if (journalSlice.isNumber()) {
      journalSize = journalSlice.getNumericValue<uint32_t>();
      if (journalSize < TRI_JOURNAL_MINIMAL_SIZE) {
        return {TRI_ERROR_BAD_PARAMETER, "<properties>.journalSize too small"};
      }
    }

    merge.add("doCompact", VPackValue(Helper::readBooleanValue(slice, "doCompact", true)));
    merge.add("indexBuckets", VPackValue(Helper::readNumericValue(slice, "indexBuckets",
                                  MMFilesCollection::defaultIndexBuckets)));
    merge.add("journalSize", VPackValue(journalSize));

  } else if (_engineType == ClusterEngineType::RocksDBEngine) {
    bool def = Helper::readBooleanValue(_info.slice(), "cacheEnabled", false);
    merge.add("cacheEnabled",
              VPackValue(Helper::readBooleanValue(slice, "cacheEnabled", def)));

  } else if (_engineType != ClusterEngineType::MockEngine) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  merge.close();
  TRI_ASSERT(merge.slice().isObject());
  TRI_ASSERT(merge.isClosed());
 
  TRI_ASSERT(_info.slice().isObject()); 
  TRI_ASSERT(_info.isClosed());
   
  VPackBuilder tmp = VPackCollection::merge(_info.slice(), merge.slice(), true);
  _info = std::move(tmp);
  
  TRI_ASSERT(_info.slice().isObject()); 
  TRI_ASSERT(_info.isClosed());

  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index>& idx : _indexes) {
    static_cast<ClusterIndex*>(idx.get())->updateProperties(_info.slice());
  }

  // nothing else to do
  return TRI_ERROR_NO_ERROR;
}

arangodb::Result ClusterCollection::persistProperties() {
  // only code path calling this causes these properties to be
  // already written in RocksDBEngine::changeCollection()
  return Result();
}

PhysicalCollection* ClusterCollection::clone(LogicalCollection& logical) const {
  return new ClusterCollection(logical, this);
}

/// @brief used for updating properties
void ClusterCollection::getPropertiesVPack(velocypack::Builder& result) const {
  // objectId might be undefined on the coordinator
  TRI_ASSERT(result.isOpenObject());

  if (_engineType == ClusterEngineType::MMFilesEngine) {
    result.add("doCompact", VPackValue(Helper::readBooleanValue(
                                _info.slice(), "doCompact", true)));
    result.add("indexBuckets", VPackValue(Helper::readNumericValue(
                                   _info.slice(), "indexBuckets",
                                   MMFilesCollection::defaultIndexBuckets)));
    result.add("isVolatile", VPackValue(Helper::readBooleanValue(
                                 _info.slice(), "isVolatile", false)));
    result.add("journalSize",
               VPackValue(Helper::readNumericValue(_info.slice(), "journalSize",
                                                   TRI_JOURNAL_DEFAULT_SIZE)));

  } else if (_engineType == ClusterEngineType::RocksDBEngine) {
    result.add("cacheEnabled", VPackValue(Helper::readBooleanValue(
                                   _info.slice(), "cacheEnabled", false)));

  } else if (_engineType != ClusterEngineType::MockEngine) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
}

/// @brief return the figures for a collection
std::shared_ptr<VPackBuilder> ClusterCollection::figures() {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  builder->close();

  auto res = figuresOnCoordinator(
    _logicalCollection.vocbase().name(),
    std::to_string(_logicalCollection.id()),
    builder
  );

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return builder;
}

void ClusterCollection::figuresSpecific(
    std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);  // not used here
}

/// @brief closes an open collection
int ClusterCollection::close() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->unload();
  }
  return TRI_ERROR_NO_ERROR;
}

void ClusterCollection::load() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->load();
  }
}

void ClusterCollection::unload() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->unload();
  }
}

TRI_voc_rid_t ClusterCollection::revision(transaction::Methods* trx) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

uint64_t ClusterCollection::numberDocuments(transaction::Methods* trx) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief report extra memory used by indexes etc.
size_t ClusterCollection::memory() const { return 0; }

void ClusterCollection::open(bool ignoreErrors) {}

void ClusterCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  WRITE_LOCKER(guard, _indexesLock);
  TRI_ASSERT(indexesSlice.isArray());

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  std::vector<std::shared_ptr<Index>> indexes;

  if (indexesSlice.length() == 0 && _indexes.empty()) {
    engine->indexFactory().fillSystemIndexes(_logicalCollection, indexes);
  } else {
    engine->indexFactory().prepareIndexes(
      _logicalCollection, indexesSlice, indexes
    );
  }

  for (std::shared_ptr<Index>& idx : indexes) {
    addIndex(std::move(idx));
  }

  if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (_logicalCollection.type() == TRI_COL_TYPE_EDGE &&
       (_indexes[1]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
        (_indexes.size() >= 3 && _engineType == ClusterEngineType::RocksDBEngine &&
         _indexes[2]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX)))) {
    std::string msg =
      "got invalid indexes for collection '" + _logicalCollection.name() + "'";

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << msg;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }

  TRI_ASSERT(!_indexes.empty());
}

static std::shared_ptr<Index> findIndex(
    velocypack::Slice const& info,
    std::vector<std::shared_ptr<Index>> const& indexes) {
  TRI_ASSERT(info.isObject());

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid index type definition");
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  for (auto const& idx : indexes) {
    if (idx->type() == type) {
      // Only check relevant indexes
      if (idx->matchesDefinition(info)) {
        // We found an index for this definition.
        return idx;
      }
    }
  }
  return nullptr;
}

/// @brief Find index by definition
std::shared_ptr<Index> ClusterCollection::lookupIndex(
    velocypack::Slice const& info) const {
  READ_LOCKER(guard, _indexesLock);
  return findIndex(info, _indexes);
}

std::shared_ptr<Index> ClusterCollection::createIndex(
    arangodb::velocypack::Slice const& info, bool restore,
    bool& created) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  // prevent concurrent dropping
  WRITE_LOCKER(guard, _exclusiveLock);
  std::shared_ptr<Index> idx;

  {
    WRITE_LOCKER(guard, _indexesLock);
    idx = findIndex(info, _indexes);
    if (idx) {
      created = false;
      // We already have this index.
      return idx;
    }
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);

  // We are sure that we do not have an index of this type.
  // We also hold the lock.
  // Create it

  idx = engine->indexFactory().prepareIndexFromSlice(
    info, true, _logicalCollection, false
  );
  TRI_ASSERT(idx != nullptr);

  // In the coordinator case we do not fill the index
  // We only inform the others.
  addIndex(idx);
  created = true;
  return idx;
}

/// @brief Drop an index with the given iid.
bool ClusterCollection::dropIndex(TRI_idx_iid_t iid) {
  // usually always called when _exclusiveLock is held
  if (iid == 0) {
    // invalid index id or primary index
    return true;
  }

  size_t i = 0;
  WRITE_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> index : _indexes) {
    if (iid == index->id()) {
      _indexes.erase(_indexes.begin() + i);
      events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
      return true;
    }
    ++i;
  }

  // We tried to remove an index that does not exist
  events::DropIndex("", std::to_string(iid), TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  return false;
}

std::unique_ptr<IndexIterator> ClusterCollection::getAllIterator(
    transaction::Methods* trx) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<IndexIterator> ClusterCollection::getAnyIterator(
    transaction::Methods* trx) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(LocalDocumentId const&)> /*callback*/) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

Result ClusterCollection::truncate(
    transaction::Methods& trx,
    OperationOptions& options
) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

LocalDocumentId ClusterCollection::lookupKey(transaction::Methods* trx,
                                             VPackSlice const& key) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::read(transaction::Methods* trx,
                               arangodb::StringRef const& key,
                               ManagedDocumentResult& result, bool) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

// read using a token!
bool ClusterCollection::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     ManagedDocumentResult& result) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// read using a token!
bool ClusterCollection::readDocumentWithCallback(
    transaction::Methods* trx, LocalDocumentId const& documentId,
    IndexIterator::DocumentCallback const& cb) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::insert(arangodb::transaction::Methods*,
                                 arangodb::velocypack::Slice const,
                                 arangodb::ManagedDocumentResult&,
                                 OperationOptions&, TRI_voc_tick_t&, bool,
                                 TRI_voc_tick_t&, 
                                 KeyLockInfo* /*keyLock*/,
                                 std::function<Result(void)>) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::update(
    arangodb::transaction::Methods* trx,
    arangodb::velocypack::Slice const newSlice, ManagedDocumentResult& mdr,
    OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool,
    TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
    arangodb::velocypack::Slice const key,
    std::function<Result(void)> /*callbackDuringLock*/) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::replace(
    transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    ManagedDocumentResult& mdr, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous,
    std::function<Result(void)> /*callbackDuringLock*/) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::remove(
    transaction::Methods& trx,
    velocypack::Slice slice,
    ManagedDocumentResult& previous,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    KeyLockInfo* /*keyLock*/,
    std::function<Result(void)> /*callbackDuringLock*/
) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterCollection::deferDropCollection(
    std::function<bool(LogicalCollection&)> const& /*callback*/
) {
  // nothing to do here
}

void ClusterCollection::addIndex(std::shared_ptr<arangodb::Index> idx) {
  // LOCKED from the outside
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return;
    }
  }
  _indexes.emplace_back(idx);
}

} // arangodb
