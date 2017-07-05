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

#include "Basics/StaticStrings.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/encoding.h"
#include "Indexes/Index.h"
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
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

PhysicalCollection::PhysicalCollection(LogicalCollection* collection,
                                       VPackSlice const& info)
    : _logicalCollection(collection), _indexes() {}

void PhysicalCollection::figures(
    std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  this->figuresSpecific(builder);
};

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

bool PhysicalCollection::hasIndexOfType(arangodb::Index::IndexType type) const {
  READ_LOCKER(guard, _indexesLock);
  for (auto const& idx : _indexes) {
    if (idx->type() == type) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<Index> PhysicalCollection::lookupIndex(
    TRI_idx_iid_t idxId) const {
  READ_LOCKER(guard, _indexesLock);
  for (auto const& idx : _indexes) {
    if (idx->id() == idxId) {
      return idx;
    }
  }
  return nullptr;
}

/// @brief merge two objects for update, oldValue must have correctly set
/// _key and _id attributes

void PhysicalCollection::mergeObjectsForUpdate(
    transaction::Methods* trx, VPackSlice const& oldValue,
    VPackSlice const& newValue, bool isEdgeCollection, std::string const& rev,
    bool mergeObjects, bool keepNull, VPackBuilder& b) const {
  b.openObject();

  VPackSlice keySlice = oldValue.get(StaticStrings::KeyString);
  VPackSlice idSlice = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!keySlice.isNone());
  TRI_ASSERT(!idSlice.isNone());

  // Find the attributes in the newValue object:
  VPackSlice fromSlice;
  VPackSlice toSlice;

  std::unordered_map<StringRef, VPackSlice> newValues;
  {
    VPackObjectIterator it(newValue, true);
    while (it.valid()) {
      StringRef key(it.key());
      if (!key.empty() && key[0] == '_' &&
          (key == StaticStrings::KeyString || key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString ||
           key == StaticStrings::ToString)) {
        // note _from and _to and ignore _id, _key and _rev
        if (key == StaticStrings::FromString) {
          fromSlice = it.value();
        } else if (key == StaticStrings::ToString) {
          toSlice = it.value();
        }  // else do nothing
      } else {
        // regular attribute
        newValues.emplace(key, it.value());
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
    VPackObjectIterator it(oldValue, true);
    while (it.valid()) {
      StringRef key(it.key());
      // exclude system attributes in old value now
      if (!key.empty() && key[0] == '_' &&
          (key == StaticStrings::KeyString || key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString ||
           key == StaticStrings::ToString)) {
        it.next();
        continue;
      }

      auto found = newValues.find(key);

      if (found == newValues.end()) {
        // use old value
        b.add(key.data(), key.size(), it.value());
      } else if (mergeObjects && it.value().isObject() &&
                 (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          VPackBuilder sub =
              VPackCollection::merge(it.value(), value, true, !keepNull);
          b.add(key.data(), key.size(), sub.slice());
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      } else {
        // use new value
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          b.add(key.data(), key.size(), value);
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
    b.add(it.first.data(), it.first.size(), s);
  }

  b.close();
}

/// @brief new object for insert, computes the hash of the key
int PhysicalCollection::newObjectForInsert(
    transaction::Methods* trx, VPackSlice const& value,
    VPackSlice const& fromSlice, VPackSlice const& toSlice,
    bool isEdgeCollection, VPackBuilder& builder, bool isRestore) const {
  TRI_voc_tick_t newRev = 0;
  builder.openObject();

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  VPackSlice s = value.get(StaticStrings::KeyString);
  if (s.isNone()) {
    TRI_ASSERT(!isRestore);  // need key in case of restore
    newRev = TRI_HybridLogicalClock();
    std::string keyString =
        _logicalCollection->keyGenerator()->generate();
    if (keyString.empty()) {
      return TRI_ERROR_ARANGO_OUT_OF_KEYS;
    }
    builder.add(StaticStrings::KeyString, VPackValue(keyString));
  } else if (!s.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  } else {
    VPackValueLength l;
    char const* p = s.getString(l);
    int res =
        _logicalCollection->keyGenerator()->validate(p, l, isRestore);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    builder.add(StaticStrings::KeyString, s);
        
    // track the key just used
    _logicalCollection->keyGenerator()->track(p, l);
  }

  // _id
  uint8_t* p = builder.add(StaticStrings::IdString,
                           VPackValuePair(9ULL, VPackValueType::Custom));
  *p++ = 0xf3;  // custom type for _id
  if (trx->state()->isDBServer() && !_logicalCollection->isSystem()) {
    // db server in cluster, note: the local collections _statistics,
    // _statisticsRaw and _statistics15 (which are the only system
    // collections)
    // must not be treated as shards but as local collections
    encoding::storeNumber<uint64_t>(p, _logicalCollection->planId(),
                                    sizeof(uint64_t));
  } else {
    // local server
    encoding::storeNumber<uint64_t>(p, _logicalCollection->cid(),
                                    sizeof(uint64_t));
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
    if (isOld || oldRevision == UINT64_MAX) {
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

/// @brief new object for remove, must have _key set
void PhysicalCollection::newObjectForRemove(transaction::Methods* trx,
                                            VPackSlice const& oldValue,
                                            std::string const& rev,
                                            VPackBuilder& builder) const {
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

/// @brief new object for replace, oldValue must have _key and _id correctly
/// set
void PhysicalCollection::newObjectForReplace(
    transaction::Methods* trx, VPackSlice const& oldValue,
    VPackSlice const& newValue, VPackSlice const& fromSlice,
    VPackSlice const& toSlice, bool isEdgeCollection, std::string const& rev,
    VPackBuilder& builder) const {
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

/// @brief checks the revision of a document
int PhysicalCollection::checkRevision(transaction::Methods* trx,
                                      TRI_voc_rid_t expected,
                                      TRI_voc_rid_t found) const {
  if (expected != 0 && found != expected) {
    return TRI_ERROR_ARANGO_CONFLICT;
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief hands out a list of indexes
std::vector<std::shared_ptr<arangodb::Index>>
PhysicalCollection::getIndexes() const {
  READ_LOCKER(guard, _indexesLock);
  return _indexes;
}

void PhysicalCollection::getIndexesVPack(VPackBuilder& result, bool withFigures,
                                         bool forPersistence) const {
  READ_LOCKER(guard, _indexesLock);
  result.openArray();
  for (auto const& idx : _indexes) {
    idx->toVelocyPack(result, withFigures, forPersistence);
  }
  result.close();
}

/// @brief return the figures for a collection
std::shared_ptr<arangodb::velocypack::Builder> PhysicalCollection::figures() {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();

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

  builder->add("indexes", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(numIndexes));
  builder->add("size", VPackValue(sizeIndexes));
  builder->close();  // indexes

  // add engine-specific figures
  figures(builder);
  builder->close();
  return builder;
}
