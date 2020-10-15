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

#include "PhysicalCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

PhysicalCollection::PhysicalCollection(LogicalCollection& collection,
                                       arangodb::velocypack::Slice const& info)
    : _logicalCollection(collection),
      _isDBServer(ServerState::instance()->isDBServer()),
      _indexes() {}

/// @brief fetches current index selectivity estimates
/// if allowUpdate is true, will potentially make a cluster-internal roundtrip
/// to fetch current values!
IndexEstMap PhysicalCollection::clusterIndexEstimates(bool allowUpdating,
                                                      TRI_voc_tick_t tid) {
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
    WRITE_LOCKER(guard, _indexesLock);
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
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "recalculateCounts not implemented for this engine");
}

bool PhysicalCollection::hasDocuments() {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "hasDocuments not implemented for this engine");
}

bool PhysicalCollection::isValidEdgeAttribute(VPackSlice const& slice) const {
  if (!slice.isString()) {
    return false;
  }

  // validate id string
  VPackValueLength len;
  char const* docId = slice.getStringUnchecked(len);
  return KeyGenerator::validateId(docId, static_cast<size_t>(len));
}

bool PhysicalCollection::hasIndexOfType(arangodb::Index::IndexType type) const {
  READ_LOCKER(guard, _indexesLock);
  for (auto const& idx : _indexes) {
    if (idx->type() == type) {
      return true;
    }
  }
  return false;
}

/// @brief Find index by definition
/*static*/ std::shared_ptr<Index> PhysicalCollection::findIndex(
    VPackSlice const& info, IndexContainerType const& indexes) {
  TRI_ASSERT(info.isObject());

  auto value = info.get(arangodb::StaticStrings::IndexType);  // extract type

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid index type definition");
  }

  VPackValueLength len;
  char const* str = value.getStringUnchecked(len);
  arangodb::Index::IndexType const type = arangodb::Index::type(str, len);
  for (auto const& idx : indexes) {
    if (idx->type() == type) {
      // Only check relevant indexes
      if (type == arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX) {
        // directly return here, as we allow at most one ttl index per collection
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
std::shared_ptr<Index> PhysicalCollection::lookupIndex(VPackSlice const& info) const {
  READ_LOCKER(guard, _indexesLock);
  return findIndex(info, _indexes);
}

std::shared_ptr<Index> PhysicalCollection::lookupIndex(IndexId idxId) const {
  READ_LOCKER(guard, _indexesLock);
  for (auto const& idx : _indexes) {
    if (idx->id() == idxId) {
      return idx;
    }
  }
  return nullptr;
}

std::shared_ptr<Index> PhysicalCollection::lookupIndex(std::string const& idxName) const {
  READ_LOCKER(guard, _indexesLock);
  for (auto const& idx : _indexes) {
    if (idx->name() == idxName) {
      return idx;
    }
  }
  return nullptr;
}

TRI_voc_rid_t PhysicalCollection::newRevisionId() const {
  if (_logicalCollection.hasClusterWideUniqueRevs()) {
    application_features::ApplicationServer& server =
        _logicalCollection.vocbase().server();
    ClusterFeature& cf = server.getFeature<ClusterFeature>();
    ClusterInfo& ci = cf.clusterInfo();
    return static_cast<TRI_voc_rid_t>(ci.uniqid());
  }
  return TRI_HybridLogicalClock();
}

Result PhysicalCollection::upgrade() {
  return Result{TRI_ERROR_NOT_IMPLEMENTED,
                "collection upgrade not supported on this type of collection"};
}

bool PhysicalCollection::didPartialUpgrade() { return false; }

Result PhysicalCollection::cleanupAfterUpgrade() {
  return Result{TRI_ERROR_NOT_IMPLEMENTED,
                "collection upgrade not supported on this type of collection"};
}

/// @brief merge two objects for update, oldValue must have correctly set
/// _key and _id attributes
Result PhysicalCollection::mergeObjectsForUpdate(
    transaction::Methods*, VPackSlice const& oldValue,
    VPackSlice const& newValue, bool isEdgeCollection, bool mergeObjects,
    bool keepNull, VPackBuilder& b, bool isRestore, TRI_voc_rid_t& revisionId) const {
  b.openObject();

  VPackSlice keySlice = oldValue.get(StaticStrings::KeyString);
  VPackSlice idSlice = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!keySlice.isNone());
  TRI_ASSERT(!idSlice.isNone());

  // Find the attributes in the newValue object:
  VPackSlice fromSlice;
  VPackSlice toSlice;

  std::unordered_map<arangodb::velocypack::StringRef, VPackSlice> newValues;
  {
    VPackObjectIterator it(newValue, true);
    while (it.valid()) {
      auto current = *it;
      arangodb::velocypack::StringRef key(current.key);
      if (key.size() >= 3 && key[0] == '_' &&
          (key == StaticStrings::KeyString || key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString || key == StaticStrings::ToString)) {
        // note _from and _to and ignore _id, _key and _rev
        if (isEdgeCollection) {
          if (key == StaticStrings::FromString) {
            fromSlice = current.value;
          } else if (key == StaticStrings::ToString) {
            toSlice = current.value;
          }
        }  // else do nothing
      } else {
        // regular attribute
        newValues.try_emplace(key, current.value);
      }

      it.next();
    }
  }

  if (isEdgeCollection) {
    if (fromSlice.isNone()) {
      fromSlice = oldValue.get(StaticStrings::FromString);
    } else if (!isValidEdgeAttribute(fromSlice)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
    if (toSlice.isNone()) {
      toSlice = oldValue.get(StaticStrings::ToString);
    } else if (!isValidEdgeAttribute(toSlice)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
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
    TRI_ASSERT(fromSlice.isString());
    TRI_ASSERT(toSlice.isString());
    b.add(StaticStrings::FromString, fromSlice);
    b.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  bool handled = false;
  if (isRestore) {
    // copy revision id verbatim
    VPackSlice s = newValue.get(StaticStrings::RevString);
    if (s.isString()) {
      b.add(StaticStrings::RevString, s);
      VPackValueLength l;
      char const* p = s.getStringUnchecked(l);
      revisionId = TRI_StringToRid(p, l, false);
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[21];
    revisionId = newRevisionId();
    b.add(StaticStrings::RevString, TRI_RidToValuePair(revisionId, &ridBuffer[0]));
  }

  // add other attributes after the system attributes
  {
    VPackObjectIterator it(oldValue, true);
    while (it.valid()) {
      auto current = (*it);
      arangodb::velocypack::StringRef key(current.key);
      // exclude system attributes in old value now
      if (key.size() >= 3 && key[0] == '_' &&
          (key == StaticStrings::KeyString || key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString || key == StaticStrings::ToString)) {
        it.next();
        continue;
      }

      auto found = newValues.find(key);

      if (found == newValues.end()) {
        // use old value
        b.addUnchecked(key.data(), key.size(), current.value);
      } else if (mergeObjects && current.value.isObject() && (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          VPackBuilder sub = VPackCollection::merge(current.value, value, true, !keepNull);
          b.addUnchecked(key.data(), key.size(), sub.slice());
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      } else {
        // use new value
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          b.addUnchecked(key.data(), key.size(), value);
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      }
      it.next();
    }
  }

  // add remaining values that were only in new object
  for (auto const& it : newValues) {
    VPackSlice const& s = it.second;
    if (s.isNone()) {
      continue;
    }
    if (!keepNull && s.isNull()) {
      continue;
    }
    b.addUnchecked(it.first.data(), it.first.size(), s);
  }

  b.close();
  return Result();
}

/// @brief new object for insert, computes the hash of the key
Result PhysicalCollection::newObjectForInsert(transaction::Methods*,
                                              VPackSlice const& value, bool isEdgeCollection,
                                              VPackBuilder& builder, bool isRestore,
                                              TRI_voc_rid_t& revisionId) const {
  builder.openObject();

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  VPackSlice s = value.get(StaticStrings::KeyString);
  if (s.isNone()) {
    TRI_ASSERT(!isRestore);  // need key in case of restore
    auto keyString = _logicalCollection.keyGenerator()->generate();

    if (keyString.empty()) {
      return Result(TRI_ERROR_ARANGO_OUT_OF_KEYS);
    }

    builder.add(StaticStrings::KeyString, VPackValue(keyString));
  } else if (!s.isString()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  } else {
    TRI_ASSERT(s.isString());

    VPackValueLength l;
    char const* p = s.getStringUnchecked(l);

    // validate and track the key just used
    auto res = _logicalCollection.keyGenerator()->validate(p, l, isRestore);

    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res);
    }

    builder.add(StaticStrings::KeyString, s);
  }

  // _id
  uint8_t* p = builder.add(StaticStrings::IdString,
                           VPackValuePair(9ULL, VPackValueType::Custom));

  *p++ = 0xf3;  // custom type for _id

  if (_isDBServer && !_logicalCollection.system()) {
    // db server in cluster, note: the local collections _statistics,
    // _statisticsRaw and _statistics15 (which are the only system
    // collections)
    // must not be treated as shards but as local collections
    encoding::storeNumber<uint64_t>(p, _logicalCollection.planId(), sizeof(uint64_t));
  } else {
    // local server
    encoding::storeNumber<uint64_t>(p, _logicalCollection.id(), sizeof(uint64_t));
  }

  // _from and _to
  if (isEdgeCollection) {
    VPackSlice fromSlice = value.get(StaticStrings::FromString);

    if (!isValidEdgeAttribute(fromSlice)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    VPackSlice toSlice = value.get(StaticStrings::ToString);

    if (!isValidEdgeAttribute(toSlice)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    TRI_ASSERT(fromSlice.isString());
    TRI_ASSERT(toSlice.isString());
    builder.add(StaticStrings::FromString, fromSlice);
    builder.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  bool handled = false;
  if (isRestore) {
    // copy revision id verbatim
    s = value.get(StaticStrings::RevString);
    if (s.isString()) {
      builder.add(StaticStrings::RevString, s);
      VPackValueLength l;
      char const* str = s.getStringUnchecked(l);
      revisionId = TRI_StringToRid(str, l, false);
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[21];
    revisionId = newRevisionId();
    builder.add(StaticStrings::RevString, TRI_RidToValuePair(revisionId, &ridBuffer[0]));
  }

  // add other attributes after the system attributes
  TRI_SanitizeObjectWithEdges(value, builder);

  builder.close();
  return Result();
}

/// @brief new object for remove, must have _key set
void PhysicalCollection::newObjectForRemove(transaction::Methods*,
                                            VPackSlice const& oldValue,
                                            VPackBuilder& builder, bool isRestore,
                                            TRI_voc_rid_t& revisionId) const {
  // create an object consisting of _key and _rev (in this order)
  builder.openObject();
  if (oldValue.isString()) {
    builder.add(StaticStrings::KeyString, oldValue);
  } else {
    VPackSlice s = oldValue.get(StaticStrings::KeyString);
    TRI_ASSERT(s.isString());
    builder.add(StaticStrings::KeyString, s);
  }

  // temporary buffer for stringifying revision ids
  char ridBuffer[21];
  revisionId = newRevisionId();
  builder.add(StaticStrings::RevString, TRI_RidToValuePair(revisionId, &ridBuffer[0]));
  builder.close();
}

/// @brief new object for replace, oldValue must have _key and _id correctly
/// set
Result PhysicalCollection::newObjectForReplace(transaction::Methods*,
                                               VPackSlice const& oldValue,
                                               VPackSlice const& newValue, bool isEdgeCollection,
                                               VPackBuilder& builder, bool isRestore,
                                               TRI_voc_rid_t& revisionId) const {
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

  // _from and _to
  if (isEdgeCollection) {
    VPackSlice fromSlice = newValue.get(StaticStrings::FromString);
    if (!isValidEdgeAttribute(fromSlice)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    VPackSlice toSlice = newValue.get(StaticStrings::ToString);
    if (!isValidEdgeAttribute(toSlice)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    TRI_ASSERT(fromSlice.isString());
    TRI_ASSERT(toSlice.isString());
    builder.add(StaticStrings::FromString, fromSlice);
    builder.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  bool handled = false;
  if (isRestore) {
    // copy revision id verbatim
    s = newValue.get(StaticStrings::RevString);
    if (s.isString()) {
      builder.add(StaticStrings::RevString, s);
      VPackValueLength l;
      char const* p = s.getStringUnchecked(l);
      revisionId = TRI_StringToRid(p, l, false);
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[21];
    revisionId = newRevisionId();
    builder.add(StaticStrings::RevString, TRI_RidToValuePair(revisionId, &ridBuffer[0]));
  }

  // add other attributes after the system attributes
  TRI_SanitizeObjectWithEdges(newValue, builder);

  builder.close();
  return Result();
}

std::unique_ptr<containers::RevisionTree> PhysicalCollection::revisionTree(
    transaction::Methods& trx) {
  return nullptr;
}

std::unique_ptr<containers::RevisionTree> PhysicalCollection::revisionTree(uint64_t batchId) {
  return nullptr;
}

Result PhysicalCollection::rebuildRevisionTree() {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

void PhysicalCollection::placeRevisionTreeBlocker(TRI_voc_tid_t) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void PhysicalCollection::removeRevisionTreeBlocker(TRI_voc_tid_t) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief checks the revision of a document
int PhysicalCollection::checkRevision(transaction::Methods*, TRI_voc_rid_t expected,
                                      TRI_voc_rid_t found) const {
  if (expected != 0 && found != expected) {
    return TRI_ERROR_ARANGO_CONFLICT;
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief hands out a list of indexes
std::vector<std::shared_ptr<Index>> PhysicalCollection::getIndexes() const {
  READ_LOCKER(guard, _indexesLock);
  return { _indexes.begin(), _indexes.end() };
}

void PhysicalCollection::getIndexesVPack(VPackBuilder& result,
                                         std::function<bool(Index const*, std::underlying_type<Index::Serialize>::type&)> const& filter) const {
  READ_LOCKER(guard, _indexesLock);
  result.openArray();
  for (std::shared_ptr<Index> const& idx : _indexes) {
    std::underlying_type<Index::Serialize>::type flags = Index::makeFlags();

    if (!filter(idx.get(), flags)) {
      continue;
    }

    idx->toVelocyPack(result, flags);
  }
  result.close();
}

/// @brief return the figures for a collection
futures::Future<OperationResult> PhysicalCollection::figures(bool details) {
  auto buffer = std::make_shared<VPackBufferUInt8>();
  VPackBuilder builder(buffer);
  
  builder.openObject();

  // add index information
  size_t sizeIndexes = memory();
  size_t numIndexes = 0;

  {
    bool seenEdgeIndex = false;
    READ_LOCKER(guard, _indexesLock);
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
  return OperationResult(Result(), std::move(buffer));
}

std::unique_ptr<ReplicationIterator> PhysicalCollection::getReplicationIterator(
    ReplicationIterator::Ordering, uint64_t batchId) {
  return nullptr;
}

std::unique_ptr<ReplicationIterator> PhysicalCollection::getReplicationIterator(
    ReplicationIterator::Ordering, transaction::Methods&) {
  return nullptr;
}

void PhysicalCollection::adjustNumberDocuments(transaction::Methods&, int64_t) {}

Result PhysicalCollection::remove(transaction::Methods& trx, LocalDocumentId documentId,
                                  ManagedDocumentResult& previous, OperationOptions& options) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

bool PhysicalCollection::IndexOrder::operator()(const std::shared_ptr<Index>& left,
                                                const std::shared_ptr<Index>& right) const {
  // Primary index always first (but two primary indexes render comparison
  // invalid but that`s a bug itself)
  TRI_ASSERT(!((left->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) &&
               (right->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX)));
  if (left->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return true;
  }
  if (right->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return false;
  }

  // edge indexes should go right after primary
  if (left->type() != right->type()) {
    if (right->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      return false;
    } else if (left->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      return true;
    }
  }

  // This failpoint allows CRUD tests to trigger reversal
  // of index operations. Hash index placed always AFTER reversable indexes
  // could be broken by unique constraint violation or by intentional failpoint.
  // And this will make possible to deterministically trigger index reversals
  TRI_IF_FAILURE("HashIndexAlwaysLast") {
    if (left->type() != right->type()) {
      if (right->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
        return true;
      } else if (left->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
        return false;
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
