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
/// @author Jan-Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
  
static std::string const Empty;

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info, "objectId")) {
  TRI_ASSERT(_objectId != 0);
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     PhysicalCollection* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _objectId(static_cast<RocksDBCollection*>(physical)->_objectId) {
  TRI_ASSERT(_objectId != 0);
}

RocksDBCollection::~RocksDBCollection() {}

std::string const& RocksDBCollection::path() const {
  return Empty; // we do not have any path
}

void RocksDBCollection::setPath(std::string const&) {
  // we do not have any path
}

arangodb::Result RocksDBCollection::updateProperties(VPackSlice const& slice,
                                                     bool doSync) {
  // nothing to do
  return arangodb::Result{};
}

arangodb::Result RocksDBCollection::persistProperties() {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return arangodb::Result{};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection* logical,
                                             PhysicalCollection* physical) {
  return new RocksDBCollection(logical, physical);
}

TRI_voc_rid_t RocksDBCollection::revision() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int64_t RocksDBCollection::initialCount() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::updateCount(int64_t) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBCollection::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("objectId", VPackValue(std::to_string(_objectId)));
}

void RocksDBCollection::getPropertiesVPackCoordinator(velocypack::Builder& result) const {
  getPropertiesVPack(result);
}

/// @brief closes an open collection
int RocksDBCollection::close() {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

uint64_t RocksDBCollection::numberDocuments() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::sizeHint(transaction::Methods* trx, int64_t hint) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief report extra memory used by indexes etc.
size_t RocksDBCollection::memory() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::open(bool ignoreErrors) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief iterate all markers of a collection on load
int RocksDBCollection::iterateMarkersOnLoad(
    arangodb::transaction::Methods* trx) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

bool RocksDBCollection::isFullyCollected() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

void RocksDBCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  createInitialIndexes();
  if (indexesSlice.isArray()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    IndexFactory const* idxFactory = engine->indexFactory();
    TRI_ASSERT(idxFactory != nullptr);
    for (auto const& v : VPackArrayIterator(indexesSlice)) {
      if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                              false)) {
        // We have an error here.
        // Do not add index.
        // TODO Handle Properly
        continue;
      }

      auto idx =
          idxFactory->prepareIndexFromSlice(v, false, _logicalCollection, true);

      if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
          idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
        continue;
      }

      if (ServerState::instance()->isRunningInCluster()) {
        addIndexCoordinator(idx);
      } else {
        addIndex(idx);
      }
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "got invalid indexes for collection '" << _logicalCollection->name() << "'";
    for (auto const& it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif
}

/// @brief Find index by definition
std::shared_ptr<Index> RocksDBCollection::lookupIndex(
    velocypack::Slice const&) const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return nullptr;
}

std::shared_ptr<Index> RocksDBCollection::createIndex(
    transaction::Methods* trx, arangodb::velocypack::Slice const& info,
    bool& created) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return nullptr;
}
/// @brief Restores an index from VelocyPack.
int RocksDBCollection::restoreIndex(transaction::Methods*,
                                    velocypack::Slice const&,
                                    std::shared_ptr<Index>&) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}
/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(TRI_idx_iid_t iid) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}
std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return nullptr;
}
std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return nullptr;
}
void RocksDBCollection::invokeOnAllElements(
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

void RocksDBCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

int RocksDBCollection::read(transaction::Methods*,
                            arangodb::velocypack::Slice const key,
                            ManagedDocumentResult& result, bool) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

bool RocksDBCollection::readDocumentConditional(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

int RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& result,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int RocksDBCollection::update(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& result,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t& prevRev,
                              ManagedDocumentResult& previous,
                              TRI_voc_rid_t const& revisionId,
                              arangodb::velocypack::Slice const key) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int RocksDBCollection::replace(
    transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId,
    arangodb::velocypack::Slice const fromSlice,
    arangodb::velocypack::Slice const toSlice) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int RocksDBCollection::remove(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const slice,
                              arangodb::ManagedDocumentResult& previous,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t const& revisionId,
                              TRI_voc_rid_t& prevRev) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> callback) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}
  
/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief creates the initial indexes for the collection
void RocksDBCollection::createInitialIndexes() {
  if (!_indexes.empty()) {
    return;
  }

  std::vector<std::shared_ptr<arangodb::Index>> systemIndexes;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory(); 
  TRI_ASSERT(idxFactory != nullptr);

  idxFactory->fillSystemIndexes(_logicalCollection, systemIndexes);
  for (auto const& it : systemIndexes) {
    addIndex(it);
  }
}

void RocksDBCollection::addIndex(std::shared_ptr<arangodb::Index> idx) {
  // primary index must be added at position 0
  TRI_ASSERT(idx->type() != arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
             _indexes.empty());

  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  _indexes.emplace_back(idx);
}

void RocksDBCollection::addIndexCoordinator(
    std::shared_ptr<arangodb::Index> idx) {
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return;
    }
  }

  _indexes.emplace_back(idx);
}
