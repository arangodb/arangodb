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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PhysicalCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/WriteLocker.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/IndexesSnapshot.h"
#include "Utils/Events.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {

PhysicalCollection::PhysicalCollection(LogicalCollection& collection)
    : _logicalCollection(collection) {}

/// @brief fetches current index selectivity estimates
/// if allowUpdate is true, will potentially make a cluster-internal roundtrip
/// to fetch current values!
IndexEstMap PhysicalCollection::clusterIndexEstimates(bool allowUpdating,
                                                      TransactionId tid) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "cluster index estimates called for non-cluster collection");
}

/// @brief flushes the current index selectivity estimates
void PhysicalCollection::flushClusterIndexEstimates() {
  // default-implementation is a no-op. the operation is only useful for cluster
  // collections
}

void PhysicalCollection::prepareIndexes(velocypack::Slice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());

  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine();

  std::vector<std::shared_ptr<Index>> indexes;
  {
    // link creation needs read-lock too
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
    if (indexesSlice.length() == 0 && _indexes.empty()) {
      engine.indexFactory().fillSystemIndexes(_logicalCollection, indexes);
    } else {
      engine.indexFactory().prepareIndexes(_logicalCollection, indexesSlice,
                                           indexes);
    }
  }

  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
  TRI_ASSERT(_indexes.empty());

  for (auto& idx : indexes) {
    TRI_ASSERT(idx != nullptr);
    auto const id = idx->id();
    for (auto const& it : _indexes) {
      TRI_ASSERT(it != nullptr);
      if (it->id() == id) {  // index is there twice
        idx.reset();
        break;
      }
    }

    if (idx) {
      _indexes.emplace(idx);
      duringAddIndex(idx);
    }
  }

  TRI_ASSERT(!_indexes.empty());

  auto it = _indexes.cbegin();
  if ((*it)->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (TRI_COL_TYPE_EDGE == _logicalCollection.type() &&
       (_indexes.size() < 3 ||
        ((*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
         (*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX)))) {
    std::string msg = "got invalid indexes for collection '" +
                      _logicalCollection.name() + "'";
    LOG_TOPIC("0ef34", ERR, arangodb::Logger::ENGINES) << msg;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto const& it : _indexes) {
      LOG_TOPIC("19e0b", ERR, arangodb::Logger::ENGINES)
          << "- " << it->context();
    }
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(msg));
  }
}

void PhysicalCollection::close() {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto& it : _indexes) {
    it->unload();
  }
}

void PhysicalCollection::drop() {
  try {
    // close collection. this will also invalidate the revisions cache
    close();
  } catch (...) {
    // don't throw from here... dropping should succeed
  }
  {
    RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
    _indexes.clear();
  }
}

uint64_t PhysicalCollection::recalculateCounts() {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "recalculateCounts not implemented for this engine");
}

bool PhysicalCollection::hasDocuments() {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "hasDocuments not implemented for this engine");
}

/// @brief Find index by definition
/*static*/ std::shared_ptr<Index> PhysicalCollection::findIndex(
    velocypack::Slice info, IndexContainerType const& indexes) {
  TRI_ASSERT(info.isObject());

  auto value = info.get(StaticStrings::IndexType);  // extract type

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid index type definition");
  }

  auto const type = Index::type(value.stringView());
  for (auto const& idx : indexes) {
    if (idx->type() == type) {
      // Only check relevant indexes
      if (type == arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX) {
        // directly return here, as we allow at most one ttl index per
        // collection
        return idx;
      }

      if (idx->matchesDefinition(info)) {
        // We found an index for this definition.
        return idx;
      }
    }
  }
  return nullptr;
}

/// @brief Find index by definition
std::shared_ptr<Index> PhysicalCollection::lookupIndex(
    velocypack::Slice info) const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  return findIndex(info, _indexes);
}

std::shared_ptr<Index> PhysicalCollection::lookupIndex(IndexId idxId) const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto const& idx : _indexes) {
    if (idx->id() == idxId) {
      return idx;
    }
  }
  return nullptr;
}

std::shared_ptr<Index> PhysicalCollection::lookupIndex(
    std::string_view idxName) const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto const& idx : _indexes) {
    if (idx->name() == idxName) {
      return idx;
    }
  }
  return nullptr;
}

std::unique_ptr<containers::RevisionTree> PhysicalCollection::revisionTree(
    transaction::Methods& /*trx*/) {
  return nullptr;
}

std::unique_ptr<containers::RevisionTree> PhysicalCollection::revisionTree(
    rocksdb::SequenceNumber) {
  return nullptr;
}

std::unique_ptr<containers::RevisionTree>
PhysicalCollection::computeRevisionTree(uint64_t batchId) {
  return nullptr;
}

Result PhysicalCollection::rebuildRevisionTree() {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

uint64_t PhysicalCollection::placeRevisionTreeBlocker(TransactionId) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void PhysicalCollection::removeRevisionTreeBlocker(TransactionId) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief hands out a list of indexes
std::vector<std::shared_ptr<Index>> PhysicalCollection::getIndexes() const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  return {_indexes.begin(), _indexes.end()};
}

/// @brief get a snapshot of all indexes of the collection, with the read
/// lock on the list of indexes being held while the snapshot is active
IndexesSnapshot PhysicalCollection::getIndexesSnapshot() {
  std::vector<std::shared_ptr<Index>> indexes;

  // lock list of indexes in collection
  RecursiveReadLocker<basics::ReadWriteLock> locker(
      _indexesLock, _indexesLockWriteOwner, __FILE__, __LINE__);

  // copy indexes from std::set into flat std::vector
  indexes.reserve(_indexes.size());
  for (auto const& it : _indexes) {
    indexes.emplace_back(it);
  }

  // pass ownership for lock and list of indexes to IndexesSnapshot
  return IndexesSnapshot(std::move(locker), std::move(indexes));
}

Index* PhysicalCollection::primaryIndex() const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto& idx : _indexes) {
    if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
      TRI_ASSERT(idx->id().isPrimary());
      return idx.get();
    }
  }
  return nullptr;
}

void PhysicalCollection::getIndexesVPack(
    VPackBuilder& result,
    std::function<bool(Index const*,
                       std::underlying_type<Index::Serialize>::type&)> const&
        filter) const {
  result.openArray();
  {
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
    for (std::shared_ptr<Index> const& idx : _indexes) {
      std::underlying_type<Index::Serialize>::type flags = Index::makeFlags();

      if (!filter(idx.get(), flags)) {
        continue;
      }

      idx->toVelocyPack(result, flags);
    }
  }
  result.close();
}

/// @brief return the figures for a collection
futures::Future<OperationResult> PhysicalCollection::figures(
    bool details, OperationOptions const& options) {
  auto buffer = std::make_shared<VPackBufferUInt8>();
  VPackBuilder builder(buffer);

  builder.openObject();

  // add index information
  size_t sizeIndexes = 0;
  size_t numIndexes = 0;

  {
    bool seenEdgeIndex = false;
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
    for (auto const& idx : _indexes) {
      // only count an edge index instance
      if (idx->type() != Index::TRI_IDX_TYPE_EDGE_INDEX || !seenEdgeIndex) {
        ++numIndexes;
      }
      if (idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
        seenEdgeIndex = true;
      }
      sizeIndexes += static_cast<size_t>(idx->memory());
    }
  }

  builder.add("indexes", VPackValue(VPackValueType::Object));
  builder.add("count", VPackValue(numIndexes));
  builder.add("size", VPackValue(sizeIndexes));
  builder.close();  // indexes

  // add engine-specific figures
  figuresSpecific(details, builder);
  builder.close();
  return OperationResult(Result(), std::move(buffer), options);
}

std::unique_ptr<ReplicationIterator> PhysicalCollection::getReplicationIterator(
    ReplicationIterator::Ordering, uint64_t batchId) {
  return nullptr;
}

std::unique_ptr<ReplicationIterator> PhysicalCollection::getReplicationIterator(
    ReplicationIterator::Ordering, transaction::Methods&) {
  return nullptr;
}

void PhysicalCollection::adjustNumberDocuments(transaction::Methods&, int64_t) {
}

bool PhysicalCollection::IndexOrder::operator()(
    std::shared_ptr<Index> const& left,
    std::shared_ptr<Index> const& right) const {
  // Primary index always first (but two primary indexes render comparison
  // invalid but that`s a bug itself)
  TRI_ASSERT(
      !((left->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) &&
        (right->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX)));
  if (left->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return true;
  } else if (right->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return false;
  }

  // edge indexes should go right after primary
  if (left->type() != right->type()) {
    if (left->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      return true;
    } else if (right->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      return false;
    }
  }

  // This failpoint allows CRUD tests to trigger reversal
  // of index operations. Hash index placed always AFTER reversable indexes
  // could be broken by unique constraint violation or by intentional failpoint.
  // And this will make possible to deterministically trigger index reversals
  TRI_IF_FAILURE("HashIndexAlwaysLast") {
    if (left->type() != right->type()) {
      if (left->type() == Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
        return false;
      } else if (right->type() == Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
        return true;
      }
    }
  }

  // indexes which needs no reverse should be done first to minimize
  // need for reversal procedures
  if (left->needsReversal() != right->needsReversal()) {
    return right->needsReversal();
  }
  // use id to make  order of equally-sorted indexes deterministic
  return left->id() < right->id();
}

Result PhysicalCollection::dropIndex(IndexId iid) {
  if (iid.empty() || iid.isPrimary()) {
    return {};
  }

  Result res = basics::catchToResult([&]() -> Result {
    auto& selector = _logicalCollection.vocbase()
                         .server()
                         .getFeature<EngineSelectorFeature>();
    auto& engine = selector.engine();
    bool const inRecovery = engine.inRecovery();

    std::shared_ptr<arangodb::Index> toRemove;
    {
      RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);

      // create a copy of _indexes, in case we need to roll back changes.
      auto copy = _indexes;

      for (auto it = _indexes.begin(); it != _indexes.end(); ++it) {
        if (iid != (*it)->id()) {
          continue;
        }

        toRemove = *it;
        // we have to remove from _indexes already here, because the
        // duringDropIndex may serialize the collection's indexes
        // and look at the _indexes member... and there we need the
        // index to be deleted already.
        _indexes.erase(it);

        if (!inRecovery) {
          Result res = duringDropIndex(toRemove);
          if (res.fail()) {
            // callback failed - revert back to copy
            _indexes = std::move(copy);
            return res;
          }
        }

        break;
      }

      if (toRemove == nullptr) {
        // index not found
        return {TRI_ERROR_ARANGO_INDEX_NOT_FOUND};
      }
    }

    TRI_ASSERT(toRemove != nullptr);

    return afterDropIndex(toRemove);
  });

  events::DropIndex(_logicalCollection.vocbase().name(),
                    _logicalCollection.name(), std::to_string(iid.id()),
                    res.errorNumber());

  return res;
}

// callback that is called directly before the index is dropped.
// the write-lock on all indexes is still held
Result PhysicalCollection::duringDropIndex(std::shared_ptr<Index> /*idx*/) {
  return {};
}

// callback that is called directly after the index has been dropped.
// no locks are held anymore.
Result PhysicalCollection::afterDropIndex(std::shared_ptr<Index> /*idx*/) {
  return {};
}

// callback that is called while adding a new index
void PhysicalCollection::duringAddIndex(std::shared_ptr<Index> /*idx*/) {}

}  // namespace arangodb
