////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/ComputedValues.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {

PhysicalCollection::PhysicalCollection(LogicalCollection& collection,
                                       velocypack::Slice info)
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

void PhysicalCollection::drop() {
  {
    RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
    _indexes.clear();
  }
  try {
    // close collection. this will also invalidate the revisions cache
    close();
  } catch (...) {
    // don't throw from here... dropping should succeed
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

bool PhysicalCollection::hasIndexOfType(arangodb::Index::IndexType type) const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto const& idx : _indexes) {
    if (idx->type() == type) {
      return true;
    }
  }
  return false;
}

/// @brief Find index by definition
/*static*/ std::shared_ptr<Index> PhysicalCollection::findIndex(
    VPackSlice info, IndexContainerType const& indexes) {
  TRI_ASSERT(info.isObject());

  auto value = info.get(arangodb::StaticStrings::IndexType);  // extract type

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid index type definition");
  }

  arangodb::Index::IndexType const type =
      arangodb::Index::type(value.stringView());
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
std::shared_ptr<Index> PhysicalCollection::lookupIndex(VPackSlice info) const {
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
    transaction::Methods& trx) {
  return nullptr;
}

std::unique_ptr<containers::RevisionTree> PhysicalCollection::revisionTree(
    uint64_t batchId) {
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

}  // namespace arangodb
