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

/// @brief Find index by definition
std::shared_ptr<Index> PhysicalCollection::lookupIndex(
    velocypack::Slice info) const {
  return _collectionIndexes.lookup(info);
}

/// @brief Find index by iid
std::shared_ptr<Index> PhysicalCollection::lookupIndex(IndexId idxId) const {
  return _collectionIndexes.lookup(idxId);
}

/// @brief Find index by name
std::shared_ptr<Index> PhysicalCollection::lookupIndex(
    std::string_view idxName) const {
  return _collectionIndexes.lookup(idxName);
}

/// @brief get list of all indexes
std::vector<std::shared_ptr<Index>> PhysicalCollection::getIndexes() const {
  return _collectionIndexes.getAll();
}

void PhysicalCollection::getIndexesVPack(
    velocypack::Builder& builder,
    std::function<bool(Index const*,
                       std::underlying_type<Index::Serialize>::type&)> const&
        filter) const {
  return _collectionIndexes.toVelocyPack(builder, filter);
}

CollectionIndexes::ReadLocked PhysicalCollection::readLockedIndexes() const {
  return _collectionIndexes.readLocked();
}

void PhysicalCollection::close() { _collectionIndexes.unload(); }

void PhysicalCollection::drop() {
  _collectionIndexes.drop();
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

std::unique_ptr<containers::RevisionTree> PhysicalCollection::revisionTree(
    transaction::Methods& trx) {
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

/// @brief return the figures for a collection
futures::Future<OperationResult> PhysicalCollection::figures(
    bool details, OperationOptions const& options) {
  auto buffer = std::make_shared<VPackBufferUInt8>();
  VPackBuilder builder(buffer);

  builder.openObject();

  auto [numIndexes, sizeIndexes] = _collectionIndexes.stats();

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

}  // namespace arangodb
