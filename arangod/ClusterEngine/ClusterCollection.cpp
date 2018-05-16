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
#include "Aql/PlanCache.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/CollectionLockState.h"
#include "ClusterEngine/ClusterIndex.h"
#include "ClusterEngine/ClusterEngine.h"
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
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

ClusterCollection::ClusterCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info), _info(info)

      /*_cacheEnabled(!collection->system() &&
                    basics::Helper::readBooleanValue(
                        info, "cacheEnabled", false))*/ {
  /*VPackSlice s = info.get("isVolatile");
  if (s.isBoolean() && s.getBoolean()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections are unsupported in the RocksDB engine");
  }*/

}

ClusterCollection::ClusterCollection(LogicalCollection* collection,
                                     PhysicalCollection const* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()), _info(VPackSlice::emptyObjectSlice())  {
}

ClusterCollection::~ClusterCollection() {
 
}

std::string const& ClusterCollection::path() const {
  return StaticStrings::Empty;  // we do not have any path
}

void ClusterCollection::setPath(std::string const&) {
  // we do not have any path
}


Result ClusterCollection::updateProperties(VPackSlice const& slice, bool doSync) {
  
  VPackBuilder merge;
  merge.openObject();

  ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  if (ce->isMMFiles()) {
    
    merge.add("doCompact", VPackValue(Helper::readBooleanValue(slice, "doCompact", true)));
    merge.add("indexBuckets", VPackValue(Helper::readNumericValue(slice, "indexBuckets", MMFilesCollection::defaultIndexBuckets)));
    merge.add("journalSize", VPackValue(Helper::readNumericValue(slice, "journalSize", TRI_JOURNAL_DEFAULT_SIZE)));
    
  } else if (ce->isRocksDB()) {
    
    merge.add("cacheEnabled", VPackValue(Helper::readBooleanValue(slice, "cacheEnabled", false)));
    
  } else {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  merge.close();
  _info = VPackCollection::merge(_info.slice(), merge.slice(), true);

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

PhysicalCollection* ClusterCollection::clone(LogicalCollection* logical) const {
  return new ClusterCollection(logical, this);
}

/// @brief used for updating properties
void ClusterCollection::getPropertiesVPack(
    velocypack::Builder& result) const {
  // objectId might be undefined on the coordinator
  TRI_ASSERT(result.isOpenObject());

  ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  if (ce->isMMFiles()) {
    
    result.add("doCompact", VPackValue(Helper::readBooleanValue(_info.slice(), "doCompact", true)));
    result.add("indexBuckets", VPackValue(Helper::readNumericValue(_info.slice(), "indexBuckets", MMFilesCollection::defaultIndexBuckets)));
    result.add("journalSize", VPackValue(Helper::readNumericValue(_info.slice(), "journalSize", TRI_JOURNAL_DEFAULT_SIZE)));
    
  } else if (ce->isRocksDB()) {
    
    result.add("cacheEnabled", VPackValue(Helper::readBooleanValue(_info.slice(), "cacheEnabled", false)));
    
  } else {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
}

/// @brief return the figures for a collection
std::shared_ptr<VPackBuilder> ClusterCollection::figures() {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  builder->close();
  
  int res = figuresOnCoordinator(_logicalCollection->vocbase().name(),
                                 std::to_string(_logicalCollection->id()), builder);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return builder;
}

void ClusterCollection::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED); // not used here
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

void ClusterCollection::open(bool ignoreErrors) {

}

void ClusterCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  WRITE_LOCKER(guard, _indexesLock);
  TRI_ASSERT(indexesSlice.isArray());
  if (indexesSlice.length() == 0) {
    createInitialIndexes();
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto& idxFactory = engine->indexFactory();
  bool splitEdgeIndex = false;
  TRI_idx_iid_t last = 0;

  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (Helper::getBooleanValue(v, "error", false)) {
      // We have an error here.
      // Do not add index.
      // TODO Handle Properly
      continue;
    }

    bool alreadyHandled = false;
    // check for combined edge index from MMFiles; must split!
    auto value = v.get("type");
    if (value.isString()) {
      std::string tmp = value.copyString();
      arangodb::Index::IndexType const type =
          arangodb::Index::type(tmp.c_str());
      if (type == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        VPackSlice fields = v.get("fields");
        if (fields.isArray() && fields.length() == 2) {
          VPackBuilder from;
          from.openObject();
          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::StringRef(f.key) == "fields") {
              from.add(VPackValue("fields"));
              from.openArray();
              from.add(VPackValue(StaticStrings::FromString));
              from.close();
            } else {
              from.add(f.key);
              from.add(f.value);
            }
          }
          from.close();

          VPackBuilder to;
          to.openObject();
          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::StringRef(f.key) == "fields") {
              to.add(VPackValue("fields"));
              to.openArray();
              to.add(VPackValue(StaticStrings::ToString));
              to.close();
            } else if (arangodb::StringRef(f.key) == "id") {
              auto iid = basics::StringUtils::uint64(f.value.copyString()) + 1;
              last = iid;
              to.add("id", VPackValue(std::to_string(iid)));
            } else {
              to.add(f.key);
              to.add(f.value);
            }
          }
          to.close();

          auto idxFrom = idxFactory.prepareIndexFromSlice(
            from.slice(), false, _logicalCollection, true
          );

          addIndex(idxFrom);

          auto idxTo = idxFactory.prepareIndexFromSlice(
            to.slice(), false, _logicalCollection, true
          );

          addIndex(idxTo);

          alreadyHandled = true;
          splitEdgeIndex = true;
        }
      } else if (splitEdgeIndex) {
        VPackBuilder b;
        b.openObject();
        for (auto const& f : VPackObjectIterator(v)) {
          if (arangodb::StringRef(f.key) == "id") {
            last++;
            b.add("id", VPackValue(std::to_string(last)));
          } else {
            b.add(f.key);
            b.add(f.value);
          }
        }
        b.close();

        auto idx = idxFactory.prepareIndexFromSlice(
          b.slice(), false, _logicalCollection, true
        );

        addIndex(idx);

        alreadyHandled = true;
      }
    }

    if (!alreadyHandled) {
      auto idx =
        idxFactory.prepareIndexFromSlice(v, false, _logicalCollection, true);

      addIndex(idx);
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE &&
       (_indexes[1]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
        _indexes[2]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX))) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "got invalid indexes for collection '" << _logicalCollection->name()
        << "'";
    for (auto it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif

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
    transaction::Methods* trx, arangodb::velocypack::Slice const& info,
    bool& created) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  // prevent concurrent dropping
  bool isLocked = trx->isLocked(_logicalCollection, AccessMode::Type::EXCLUSIVE);
  CONDITIONAL_WRITE_LOCKER(guard, _exclusiveLock, !isLocked);
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

/// @brief Restores an index from VelocyPack.
int ClusterCollection::restoreIndex(transaction::Methods* trx,
                                    velocypack::Slice const& info,
                                    std::shared_ptr<Index>& idx) {
  // The coordinator can never get into this state!
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
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

std::unique_ptr<IndexIterator> ClusterCollection::getAllIterator(transaction::Methods* trx) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<IndexIterator> ClusterCollection::getAnyIterator(
    transaction::Methods* trx) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(LocalDocumentId const&)> callback) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

void ClusterCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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

Result ClusterCollection::insert(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const slice,
                                 arangodb::ManagedDocumentResult& mdr,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick,
                                 bool /*lock*/, TRI_voc_rid_t& revisionId) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::update(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const newSlice,
                                 arangodb::ManagedDocumentResult& mdr,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick,
                                 bool /*lock*/, TRI_voc_rid_t& prevRev,
                                 ManagedDocumentResult& previous,
                                 arangodb::velocypack::Slice const key) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::replace(transaction::Methods* trx,
                                  arangodb::velocypack::Slice const newSlice,
                                  ManagedDocumentResult& mdr,
                                  OperationOptions& options,
                                  TRI_voc_tick_t& resultMarkerTick,
                                  bool /*lock*/, TRI_voc_rid_t& prevRev,
                                  ManagedDocumentResult& previous,
                                  arangodb::velocypack::Slice const fromSlice,
                                  arangodb::velocypack::Slice const toSlice) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterCollection::remove(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const slice,
                                 arangodb::ManagedDocumentResult& previous,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick,
                                 bool /*lock*/, TRI_voc_rid_t& prevRev,
                                 TRI_voc_rid_t& revisionId) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> /*callback*/) {
  // nothing to do here
}

/// @brief creates the initial indexes for the collection
void ClusterCollection::createInitialIndexes() {
  // LOCKED from the outside
  if (!_indexes.empty()) {
    return;
  }

  std::vector<std::shared_ptr<arangodb::Index>> systemIndexes;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  engine->indexFactory().fillSystemIndexes(_logicalCollection, systemIndexes);

  for (auto const& it : systemIndexes) {
    addIndex(it);
  }
}

void ClusterCollection::addIndex(
    std::shared_ptr<arangodb::Index> idx) {
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
