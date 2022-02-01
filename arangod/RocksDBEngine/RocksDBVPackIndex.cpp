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
/// @author Jan Steemann
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBVPackIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

// .............................................................................
// recall for all of the following comparison functions:
//
// left < right  return -1
// left > right  return  1
// left == right return  0
//
// furthermore:
//
// the following order is currently defined for placing an order on documents
// undef < null < boolean < number < strings < lists < hash arrays
// note: undefined will be treated as NULL pointer not NULL JSON OBJECT
// within each type class we have the following order
// boolean: false < true
// number: natural order
// strings: lexicographical
// lists: lexicographically and within each slot according to these rules.
// ...........................................................................

/// @brief Iterator structure for RocksDB unique index.
/// This iterator can be used only for equality lookups that use all
/// index attributes. It uses a point lookup and no seeks

namespace arangodb {

class RocksDBVPackUniqueIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackUniqueIndexIterator(LogicalCollection* collection,
                                  transaction::Methods* trx,
                                  arangodb::RocksDBVPackIndex const* index,
                                  VPackSlice const& indexValues,
                                  ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _cmp(index->comparator()),
        _key(trx),
        _done(false) {
    TRI_ASSERT(index->unique());
    TRI_ASSERT(index->columnFamily() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::VPackIndex));
    _key->constructUniqueVPackIndexValue(index->objectId(), indexValues);
  }

 public:
  char const* typeName() const override {
    return "rocksdb-unique-index-iterator";
  }

  /// @brief Get the next limit many element in the index
  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || _done) {
      // already looked up something
      return false;
    }

    _done = true;

    rocksdb::PinnableSlice ps;
    RocksDBMethods* mthds =
        RocksDBTransactionState::toMethods(_trx, _collection->id());
    rocksdb::Status s = mthds->Get(_index->columnFamily(), _key->string(), &ps,
                                   canReadOwnWrites());

    if (s.ok()) {
      cb(RocksDBValue::documentId(ps));
    }

    // there is at most one element, so we are done now
    return false;
  }

  bool nextCoveringImpl(CoveringCallback const& cb, size_t limit) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || _done) {
      // already looked up something
      return false;
    }

    _done = true;

    rocksdb::PinnableSlice ps;
    RocksDBMethods* mthds =
        RocksDBTransactionState::toMethods(_trx, _collection->id());
    rocksdb::Status s = mthds->Get(_index->columnFamily(), _key->string(), &ps,
                                   canReadOwnWrites());

    if (s.ok()) {
      if (_index->hasStoredValues()) {
        auto data = SliceCoveringDataWithStoredValues(
            RocksDBKey::indexedVPack(_key.ref()),
            RocksDBValue::uniqueIndexStoredValues(ps));
        cb(LocalDocumentId(RocksDBValue::documentId(ps)), data);
      } else {
        auto data = SliceCoveringData(RocksDBKey::indexedVPack(_key.ref()));
        cb(LocalDocumentId(RocksDBValue::documentId(ps)), data);
      }
    }

    // there is at most one element, so we are done now
    return false;
  }

  /// @brief Reset the cursor
  void resetImpl() override {
    TRI_ASSERT(_trx->state()->isRunning());

    _done = false;
  }

  /// we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override {
    return _index->type() != arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX;
  }

 private:
  arangodb::RocksDBVPackIndex const* _index;
  rocksdb::Comparator const* _cmp;
  RocksDBKeyLeaser _key;
  bool _done;
};

/// @brief Iterator structure for RocksDB. We require a start and stop node
template<bool unique, bool reverse>
class RocksDBVPackIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackIndexIterator(LogicalCollection* collection,
                            transaction::Methods* trx,
                            arangodb::RocksDBVPackIndex const* index,
                            RocksDBKeyBounds&& bounds,
                            ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _cmp(static_cast<RocksDBVPackComparator const*>(index->comparator())),
        _bounds(std::move(bounds)),
        _rangeBound(reverse ? _bounds.start() : _bounds.end()),
        _mustSeek(true),
        _mustCheckBounds(
            RocksDBTransactionState::toState(trx)->iteratorMustCheckBounds(
                collection->id(), readOwnWrites)) {
    TRI_ASSERT(index->columnFamily() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::VPackIndex));
  }

 public:
  char const* typeName() const override { return "rocksdb-index-iterator"; }

  /// @brief index does not support rearming
  bool canRearm() const override { return false; }

  /// @brief Get the next limit many elements in the index
  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    ensureIterator();
    TRI_ASSERT(_trx->state()->isRunning());
    TRI_ASSERT(_iterator != nullptr);

    if (limit == 0 || !_iterator->Valid() || outOfRange()) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iterator.get());
      return false;
    }

    TRI_ASSERT(limit > 0);

    do {
      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(_iterator->key()));

      if constexpr (unique) {
        cb(RocksDBValue::documentId(_iterator->value()));
      } else {
        cb(RocksDBKey::indexDocumentId(_iterator->key()));
      }

      if (!advance()) {
        // validate that Iterator is in a good shape and hasn't failed
        arangodb::rocksutils::checkIteratorStatus(_iterator.get());
        return false;
      }

      --limit;
      if (limit == 0) {
        return true;
      }
    } while (true);
  }

  bool nextCoveringImpl(CoveringCallback const& cb, size_t limit) override {
    ensureIterator();
    TRI_ASSERT(_trx->state()->isRunning());
    TRI_ASSERT(_iterator != nullptr);

    if (limit == 0 || !_iterator->Valid() || outOfRange()) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iterator.get());
      return false;
    }

    TRI_ASSERT(limit > 0);

    do {
      rocksdb::Slice key = _iterator->key();
      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(key));

      if constexpr (unique) {
        LocalDocumentId const documentId(
            RocksDBValue::documentId(_iterator->value()));
        if (_index->hasStoredValues()) {
          auto data = SliceCoveringDataWithStoredValues(
              RocksDBKey::indexedVPack(key),
              RocksDBValue::uniqueIndexStoredValues(_iterator->value()));
          cb(documentId, data);
        } else {
          auto data = SliceCoveringData(RocksDBKey::indexedVPack(key));
          cb(documentId, data);
        }
      } else {
        LocalDocumentId const documentId(RocksDBKey::indexDocumentId(key));
        if (_index->hasStoredValues()) {
          auto data = SliceCoveringDataWithStoredValues(
              RocksDBKey::indexedVPack(key),
              RocksDBValue::indexStoredValues(_iterator->value()));
          cb(documentId, data);
        } else {
          auto data = SliceCoveringData(RocksDBKey::indexedVPack(key));
          cb(documentId, data);
        }
      }

      if (!advance()) {
        // validate that Iterator is in a good shape and hasn't failed
        arangodb::rocksutils::checkIteratorStatus(_iterator.get());
        return false;
      }

      --limit;
      if (limit == 0) {
        return true;
      }
    } while (true);
  }

  void skipImpl(uint64_t count, uint64_t& skipped) override {
    ensureIterator();
    TRI_ASSERT(_trx->state()->isRunning());
    TRI_ASSERT(_iterator != nullptr);

    if (_iterator->Valid() && !outOfRange() && count > 0) {
      do {
        TRI_ASSERT(_index->objectId() ==
                   RocksDBKey::objectId(_iterator->key()));

        --count;
        ++skipped;
        if (!advance()) {
          break;
        }

        if (count == 0) {
          return;
        }
      } while (true);
    }

    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
  }

  /// @brief Reset the cursor
  void resetImpl() override {
    TRI_ASSERT(_trx->state()->isRunning());
    _mustSeek = true;
  }

  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override {
    return _index->type() != arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX;
  }

 private:
  inline bool outOfRange() const {
    // we can effectively disable the out-of-range checks for read-only
    // transactions, as our Iterator is a snapshot-based iterator with a
    // configured iterate_upper_bound/iterate_lower_bound value.
    // this makes RocksDB filter out non-matching keys automatically.
    // however, for a write transaction our Iterator is a rocksdb
    // BaseDeltaIterator, which will merge the values from a snapshot iterator
    // and the changes in the current transaction. here rocksdb will only apply
    // the bounds checks for the base iterator (from the snapshot), but not for
    // the delta iterator (from the current transaction), so we still have to
    // carry out the checks ourselves.

    if constexpr (reverse) {
      return _mustCheckBounds &&
             (_cmp->Compare(_iterator->key(), _rangeBound) < 0);
    } else {
      return _mustCheckBounds &&
             (_cmp->Compare(_iterator->key(), _rangeBound) > 0);
    }
  }

  void ensureIterator() {
    if (_iterator == nullptr) {
      auto state = RocksDBTransactionState::toState(_trx);
      RocksDBTransactionMethods* mthds =
          state->rocksdbMethods(_collection->id());
      _iterator =
          mthds->NewIterator(_index->columnFamily(), [&](ReadOptions& options) {
            TRI_ASSERT(options.prefix_same_as_start);
            // we need to have a pointer to a slice for the upper bound
            // so we need to assign the slice to an instance variable here
            if constexpr (reverse) {
              options.iterate_lower_bound = &_rangeBound;
            } else {
              options.iterate_upper_bound = &_rangeBound;
            }
            options.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes;
          });
    }

    TRI_ASSERT(_iterator != nullptr);
    if (_mustSeek) {
      if constexpr (reverse) {
        _iterator->SeekForPrev(_bounds.end());
      } else {
        _iterator->Seek(_bounds.start());
      }
      _mustSeek = false;
    }
    TRI_ASSERT(!_mustSeek);
  }

  inline bool advance() {
    if constexpr (reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }

    return _iterator->Valid() && !outOfRange();
  }

  arangodb::RocksDBVPackIndex const* _index;
  RocksDBVPackComparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  RocksDBKeyBounds const _bounds;
  // used for iterate_upper_bound iterate_lower_bound
  rocksdb::Slice _rangeBound;
  bool _mustSeek;
  bool const _mustCheckBounds;
};

}  // namespace arangodb

uint64_t RocksDBVPackIndex::HashForKey(const rocksdb::Slice& key) {
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  VPackSlice tmp = RocksDBKey::indexedVPack(key);
  return tmp.normalizedHash();
}

/// @brief create the index
RocksDBVPackIndex::RocksDBVPackIndex(IndexId iid,
                                     arangodb::LogicalCollection& collection,
                                     arangodb::velocypack::Slice info)
    : RocksDBIndex(iid, collection, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VPackIndex),
                   /*useCache*/ false),
      _deduplicate(arangodb::basics::VelocyPackHelper::getBooleanValue(
          info, "deduplicate", true)),
      _allowPartialIndex(true),
      _estimates(true),
      _estimator(nullptr),
      _storedValues(Index::parseFields(
          info.get(arangodb::StaticStrings::IndexStoredValues),
          /*allowEmpty*/ true, /*allowExpansion*/ false)),
      _coveredFields(Index::mergeFields(fields(), _storedValues)) {
  TRI_ASSERT(_cf == RocksDBColumnFamilyManager::get(
                        RocksDBColumnFamilyManager::Family::VPackIndex));

  if (_unique) {
    // unique indexes will always have a hard-coded estimate of 1
    _estimates = true;
  } else if (VPackSlice s = info.get(StaticStrings::IndexEstimates);
             s.isBoolean()) {
    // read "estimates" flag from velocypack if it is present.
    // if it's not present, we go with the default (estimates = true)
    _estimates = s.getBoolean();
  }

  if (_estimates && !_unique && !ServerState::instance()->isCoordinator() &&
      !collection.isAStub()) {
    // We activate the estimator for all non unique-indexes.
    // And only on single servers and DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimatorType>(
        RocksDBIndex::ESTIMATOR_SIZE);
  }

  TRI_ASSERT(!_fields.empty());
  TRI_ASSERT(iid.isSet());

  fillPaths(_fields, _paths, &_expanding);
  fillPaths(_storedValues, _storedValuesPaths, nullptr);
  TRI_ASSERT(_fields.size() == _paths.size());
  TRI_ASSERT(_storedValues.size() == _storedValuesPaths.size());
}

/// @brief destroy the index
RocksDBVPackIndex::~RocksDBVPackIndex() = default;

std::vector<std::vector<arangodb::basics::AttributeName>> const&
RocksDBVPackIndex::coveredFields() const {
  return _coveredFields;
}

bool RocksDBVPackIndex::hasSelectivityEstimate() const {
  return _unique || _estimates;
}

double RocksDBVPackIndex::selectivityEstimate(std::string_view) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  if (_estimator == nullptr || !_estimates) {
    // we turn off the estimates for some system collections to avoid updating
    // them too often. we also turn off estimates for stub collections on
    // coordinator and DB servers
    return 0.0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

/// @brief return a VelocyPack representation of the index
void RocksDBVPackIndex::toVelocyPack(
    VPackBuilder& builder, std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);

  // serialize storedValues, if they exist
  if (!_storedValues.empty()) {
    builder.add(arangodb::velocypack::Value(
        arangodb::StaticStrings::IndexStoredValues));
    builder.openArray();

    for (auto const& field : _storedValues) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }

  builder.add("deduplicate", VPackValue(_deduplicate));
  builder.add(StaticStrings::IndexEstimates, VPackValue(_estimates));
  builder.close();
}

/// @brief helper function to insert a document into any index type
/// Should result in an elements vector filled with the new index entries
/// uses the _unique field to determine the kind of key structure
ErrorCode RocksDBVPackIndex::fillElement(
    VPackBuilder& leased, LocalDocumentId const& documentId, VPackSlice doc,
    ::arangodb::containers::SmallVector<RocksDBKey>& elements,
    ::arangodb::containers::SmallVector<uint64_t>& hashes) {
  if (doc.isNone()) {
    LOG_TOPIC("51c6c", ERR, arangodb::Logger::ENGINES)
        << "encountered invalid marker with slice of type None";
    return TRI_ERROR_INTERNAL;
  }

  TRI_IF_FAILURE("FillElementIllegalSlice") { return TRI_ERROR_INTERNAL; }

  TRI_ASSERT(leased.isEmpty());
  if (!_useExpansion) {
    // fast path for inserts... no array elements used
    leased.openArray(true);

    size_t const n = _paths.size();
    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(!_paths[i].empty());

      VPackSlice slice = doc.get(_paths[i]);
      if (slice.isNone() || slice.isNull()) {
        // attribute not found
        if (_sparse) {
          // if sparse we do not have to index, this is indicated by result
          // being shorter than n
          return TRI_ERROR_NO_ERROR;
        }
        // null, note that this will be copied later!
        leased.add(VPackSlice::nullSlice());
      } else {
        leased.add(slice);
      }
    }
    leased.close();

    TRI_IF_FAILURE("FillElementOOM") { return TRI_ERROR_OUT_OF_MEMORY; }
    TRI_IF_FAILURE("FillElementOOM2") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    elements.emplace_back();
    RocksDBKey& key = elements.back();
    if (_unique) {
      // Unique VPack index values are stored as follows:
      // - Key: 7 + 8-byte object ID of index + VPack array with index
      // value(s) + separator (NUL) byte
      // - Value: primary key
      key.constructUniqueVPackIndexValue(objectId(), leased.slice());
    } else {
      // Non-unique VPack index values are stored as follows:
      // - Key: 6 + 8-byte object ID of index + VPack array with index
      // value(s) + revisionID
      // - Value: empty
      key.constructVPackIndexValue(objectId(), leased.slice(), documentId);
      hashes.push_back(leased.slice().normalizedHash());
    }
  } else {
    // other path for handling array elements, too

    ::arangodb::containers::SmallVector<VPackSlice>::allocator_type::arena_type
        sliceStackArena;
    ::arangodb::containers::SmallVector<VPackSlice> sliceStack{sliceStackArena};

    try {
      buildIndexValues(leased, documentId, doc, 0, elements, hashes,
                       sliceStack);
    } catch (arangodb::basics::Exception const& ex) {
      return ex.code();
    } catch (std::bad_alloc const&) {
      return TRI_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      // unknown error
      return TRI_ERROR_INTERNAL;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

void RocksDBVPackIndex::addIndexValue(
    VPackBuilder& leased, LocalDocumentId const& documentId,
    VPackSlice document,
    ::arangodb::containers::SmallVector<RocksDBKey>& elements,
    ::arangodb::containers::SmallVector<uint64_t>& hashes,
    ::arangodb::containers::SmallVector<VPackSlice>& sliceStack) {
  leased.clear();
  leased.openArray(true);  // unindexed
  for (VPackSlice const& s : sliceStack) {
    leased.add(s);
  }
  leased.close();

  if (_unique) {
    // Unique VPack index values are stored as follows:
    // - Key: 7 + 8-byte object ID of index + VPack array with index value(s)
    // - Value: primary key
    RocksDBKey key;
    key.constructUniqueVPackIndexValue(objectId(), leased.slice());
    elements.emplace_back(std::move(key));
  } else {
    // Non-unique VPack index values are stored as follows:
    // - Key: 6 + 8-byte object ID of index + VPack array with index value(s)
    // + primary key
    // - Value: empty
    RocksDBKey key;
    key.constructVPackIndexValue(objectId(), leased.slice(), documentId);
    elements.emplace_back(std::move(key));
    hashes.push_back(leased.slice().normalizedHash());
  }
}

/// @brief helper function to create a set of index combinations to insert
void RocksDBVPackIndex::buildIndexValues(
    VPackBuilder& leased, LocalDocumentId const& documentId,
    VPackSlice const doc, size_t level,
    ::arangodb::containers::SmallVector<RocksDBKey>& elements,
    ::arangodb::containers::SmallVector<uint64_t>& hashes,
    ::arangodb::containers::SmallVector<VPackSlice>& sliceStack) {
  // Invariant: level == sliceStack.size()

  // Stop the recursion:
  if (level == _paths.size()) {
    addIndexValue(leased, documentId, doc, elements, hashes, sliceStack);
    return;
  }

  if (_expanding[level] == -1) {  // the trivial, non-expanding case
    VPackSlice slice = doc.get(_paths[level]);
    if (slice.isNone() || slice.isNull()) {
      if (_sparse) {
        return;
      }
      sliceStack.emplace_back(arangodb::velocypack::Slice::nullSlice());
    } else {
      sliceStack.emplace_back(slice);
    }
    buildIndexValues(leased, documentId, doc, level + 1, elements, hashes,
                     sliceStack);
    sliceStack.pop_back();
    return;
  }

  // Finally, the complex case, where we have to expand one entry.
  // Note again that at most one step in the attribute path can be
  // an array step. Furthermore, if _allowPartialIndex is true and
  // anything goes wrong with this attribute path, we have to bottom out
  // with None values to be able to use the index for a prefix match.

  // Trivial case to bottom out with Illegal types.
  VPackSlice illegalSlice = arangodb::velocypack::Slice::illegalSlice();

  auto finishWithNones = [&]() -> void {
    if (!_allowPartialIndex || level == 0) {
      return;
    }
    for (size_t i = level; i < _paths.size(); i++) {
      sliceStack.emplace_back(illegalSlice);
    }
    addIndexValue(leased, documentId, doc, elements, hashes, sliceStack);
    for (size_t i = level; i < _paths.size(); i++) {
      sliceStack.pop_back();
    }
  };
  size_t const n = _paths[level].size();
  // We have 0 <= _expanding[level] < n.
  VPackSlice current(doc);
  for (size_t i = 0; i <= static_cast<size_t>(_expanding[level]); i++) {
    if (!current.isObject()) {
      finishWithNones();
      return;
    }
    current = current.get(_paths[level][i]);
    if (current.isNone()) {
      finishWithNones();
      return;
    }
  }
  // Now the expansion:
  if (!current.isArray() || current.length() == 0) {
    finishWithNones();
    return;
  }

  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      seen(2, arangodb::basics::VelocyPackHelper::VPackHash(),
           arangodb::basics::VelocyPackHelper::VPackEqual());

  auto moveOn = [&](VPackSlice something) -> void {
    auto it = seen.find(something);
    if (it == seen.end()) {
      seen.insert(something);
      sliceStack.emplace_back(something);
      buildIndexValues(leased, documentId, doc, level + 1, elements, hashes,
                       sliceStack);
      sliceStack.pop_back();
    } else if (_unique && !_deduplicate) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    }
  };
  for (VPackSlice member : VPackArrayIterator(current)) {
    VPackSlice current2(member);
    bool doneNull = false;
    for (size_t i = _expanding[level] + 1; i < n; i++) {
      if (!current2.isObject()) {
        if (!_sparse) {
          moveOn(arangodb::velocypack::Slice::nullSlice());
        }
        doneNull = true;
        break;
      }
      current2 = current2.get(_paths[level][i]);
      if (current2.isNone()) {
        if (!_sparse) {
          moveOn(arangodb::velocypack::Slice::nullSlice());
        }
        doneNull = true;
        break;
      }
    }
    if (!doneNull) {
      moveOn(current2);
    }
    // Finally, if, because of sparsity, we have not inserted anything by now,
    // we need to play the above trick with None because of the above
    // mentioned
    // reasons:
    if (seen.empty()) {
      finishWithNones();
    }
  }
}

/// @brief helper function to transform AttributeNames into strings.
void RocksDBVPackIndex::fillPaths(
    std::vector<std::vector<arangodb::basics::AttributeName>> const& source,
    std::vector<std::vector<std::string>>& paths, std::vector<int>* expanding) {
  paths.clear();
  if (expanding != nullptr) {
    expanding->clear();
  }
  for (std::vector<arangodb::basics::AttributeName> const& list : source) {
    paths.emplace_back();
    std::vector<std::string>& interior(paths.back());
    int expands = -1;
    int count = 0;
    for (auto const& att : list) {
      interior.emplace_back(att.name);
      if (att.shouldExpand) {
        expands = count;
      }
      ++count;
    }
    if (expanding != nullptr) {
      expanding->emplace_back(expands);
    }
  }
}

/// @brief returns whether the document can be inserted into the index
/// (or if there will be a conflict)
Result RocksDBVPackIndex::checkInsert(transaction::Methods& trx,
                                      RocksDBMethods* mthds,
                                      LocalDocumentId const& documentId,
                                      velocypack::Slice doc,
                                      OperationOptions const& options) {
  return checkOperation(trx, mthds, documentId, doc, options, false);
}

/// @brief returns whether the document can be replaced into the index
/// (or if there will be a conflict)
Result RocksDBVPackIndex::checkReplace(transaction::Methods& trx,
                                       RocksDBMethods* mthds,
                                       LocalDocumentId const& documentId,
                                       velocypack::Slice doc,
                                       OperationOptions const& options) {
  return checkOperation(trx, mthds, documentId, doc, options, true);
}

Result RocksDBVPackIndex::checkOperation(transaction::Methods& trx,
                                         RocksDBMethods* mthds,
                                         LocalDocumentId const& documentId,
                                         velocypack::Slice doc,
                                         OperationOptions const& options,
                                         bool ignoreExisting) {
  Result res;

  // non-unique indexes will not cause any constraint violation
  if (_unique) {
    // unique indexes...

    IndexOperationMode mode = options.indexOperationMode;
    rocksdb::Status s;
    ::arangodb::containers::SmallVector<RocksDBKey>::allocator_type::arena_type
        elementsArena;
    ::arangodb::containers::SmallVector<RocksDBKey> elements{elementsArena};
    ::arangodb::containers::SmallVector<uint64_t>::allocator_type::arena_type
        hashesArena;
    ::arangodb::containers::SmallVector<uint64_t> hashes{hashesArena};

    {
      // rethrow all types of exceptions from here...
      transaction::BuilderLeaser leased(&trx);
      auto r = fillElement(*leased, documentId, doc, elements, hashes);

      if (r != TRI_ERROR_NO_ERROR) {
        return addErrorMsg(res, r);
      }
    }

    transaction::StringLeaser leased(&trx);
    rocksdb::PinnableSlice existing(leased.get());

    bool const lock =
        !RocksDBTransactionState::toState(&trx)->isOnlyExclusiveTransaction();

    for (RocksDBKey const& key : elements) {
      if (lock) {
        s = mthds->GetForUpdate(_cf, key.string(), &existing);
      } else {
        // modifications always need to observe all changes in order to validate
        // uniqueness constraints
        s = mthds->Get(_cf, key.string(), &existing, ReadOwnWrites::yes);
      }

      if (s.ok()) {  // detected conflicting index entry
        LocalDocumentId docId = RocksDBValue::documentId(existing);
        if (docId == documentId && ignoreExisting) {
          // same document, this is ok!
          continue;
        }
        res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
        // find conflicting document's key
        auto readResult = _collection.getPhysical()->read(
            &trx, docId,
            [&](LocalDocumentId const&, VPackSlice doc) {
              VPackSlice key =
                  transaction::helpers::extractKeyFromDocument(doc);
              if (mode == IndexOperationMode::internal) {
                // in this error mode, we return the conflicting document's key
                // inside the error message string (and nothing else)!
                res = Result{res.errorNumber(), key.copyString()};
              } else {
                // normal mode: build a proper error message
                addErrorMsg(res, key.copyString());
              }
              return true;  // return value does not matter here
            },
            ReadOwnWrites::yes);  // modifications always need to observe all
                                  // changes in order to validate uniqueness
                                  // constraints
        if (readResult.fail()) {
          addErrorMsg(readResult);
          THROW_ARANGO_EXCEPTION(readResult);
        }
        TRI_ASSERT(res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED));
        break;
      } else if (!s.IsNotFound()) {
        res.reset(rocksutils::convertStatus(s));
        addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
      }
    }
  }

  return res;
}

/// @brief inserts a document into the index
Result RocksDBVPackIndex::insert(transaction::Methods& trx,
                                 RocksDBMethods* mthds,
                                 LocalDocumentId const& documentId,
                                 velocypack::Slice doc,
                                 OperationOptions const& options,
                                 bool performChecks) {
  Result res;
  ::arangodb::containers::SmallVector<RocksDBKey>::allocator_type::arena_type
      elementsArena;
  ::arangodb::containers::SmallVector<RocksDBKey> elements{elementsArena};
  ::arangodb::containers::SmallVector<uint64_t>::allocator_type::arena_type
      hashesArena;
  ::arangodb::containers::SmallVector<uint64_t> hashes{hashesArena};

  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    auto r = fillElement(*leased, documentId, doc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  // now we are going to construct the value to insert into rocksdb
  if (_unique) {
    // build index value (storedValues array will be stored in value if
    // storedValues are used)
    RocksDBValue value = RocksDBValue::UniqueVPackIndexValue(documentId);
    if (!_storedValuesPaths.empty()) {
      transaction::BuilderLeaser leased(&trx);
      leased->openArray(true);
      for (auto const& it : _storedValuesPaths) {
        VPackSlice s = doc.get(it);
        if (s.isNone()) {
          s = VPackSlice::nullSlice();
        }
        leased->add(s);
      }
      leased->close();
      value = RocksDBValue::UniqueVPackIndexValue(documentId, leased->slice());
    }

    transaction::StringLeaser leased(&trx);
    rocksdb::PinnableSlice existing(leased.get());

    rocksdb::Status s;
    // unique indexes have a different key structure
    for (RocksDBKey const& key : elements) {
      if (performChecks) {
        s = mthds->GetForUpdate(_cf, key.string(), &existing);
        if (s.ok()) {  // detected conflicting index entry
          res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
          break;
        } else if (!s.IsNotFound()) {
          res.reset(rocksutils::convertStatus(s));
          break;
        }
      }
      s = mthds->Put(_cf, key, value.string(), /*assume_tracked*/ true);
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::index);
        break;
      }
    }

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
        // find conflicting document's key
        LocalDocumentId docId = RocksDBValue::documentId(existing);
        auto readResult = _collection.getPhysical()->read(
            &trx, docId,
            [&](LocalDocumentId const&, VPackSlice doc) {
              IndexOperationMode mode = options.indexOperationMode;
              VPackSlice key =
                  transaction::helpers::extractKeyFromDocument(doc);
              if (mode == IndexOperationMode::internal) {
                // in this error mode, we return the conflicting document's key
                // inside the error message string (and nothing else)!
                res = Result{res.errorNumber(), key.copyString()};
              } else {
                // normal mode: build a proper error message
                addErrorMsg(res, key.copyString());
              }
              return true;  // return value does not matter here
            },
            ReadOwnWrites::yes);  // modifications always need to observe all
                                  // changes in order to validate uniqueness
                                  // constraints
        if (readResult.fail()) {
          addErrorMsg(readResult);
          THROW_ARANGO_EXCEPTION(readResult);
        }
      } else {
        addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
      }
    }

  } else {
    // non-unique index

    // AQL queries never read from the same collection, after writing into it
    IndexingDisabler guard(
        mthds,
        trx.state()->hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL) &&
            options.canDisableIndexing);

    // build index value (storedValues array will be stored in value if
    // storedValues are used)
    RocksDBValue value = RocksDBValue::VPackIndexValue();
    if (!_storedValuesPaths.empty()) {
      transaction::BuilderLeaser leased(&trx);
      leased->openArray(true);
      for (auto const& it : _storedValuesPaths) {
        VPackSlice s = doc.get(it);
        if (s.isNone()) {
          s = VPackSlice::nullSlice();
        }
        leased->add(s);
      }
      leased->close();
      value = RocksDBValue::VPackIndexValue(leased->slice());
    }

    rocksdb::Status s;
    for (RocksDBKey const& key : elements) {
      TRI_ASSERT(key.containsLocalDocumentId(documentId));
      s = mthds->PutUntracked(_cf, key, value.string());
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::index);
        break;
      }
    }

    if (res.fail()) {
      addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
    } else if (_estimates) {
      auto* state = RocksDBTransactionState::toState(&trx);
      auto* trxc = static_cast<RocksDBTransactionCollection*>(
          state->findCollection(_collection.id()));
      TRI_ASSERT(trxc != nullptr);
      for (uint64_t hash : hashes) {
        trxc->trackIndexInsert(id(), hash);
      }
    }
  }

  return res;
}

namespace {
bool attributesEqual(
    VPackSlice first, VPackSlice second,
    std::vector<arangodb::basics::AttributeName>::const_iterator begin,
    std::vector<arangodb::basics::AttributeName>::const_iterator end) {
  for (; begin != end; ++begin) {
    // check if, after fetching the subattribute, we are point to a non-object.
    // e.g. if the index is on field ["a.b"], the first iteration of this loop
    // will look for subattribute "a" in the original document. this will always
    // work. however, when looking for "b", we have to make sure that "a" was
    // an object. otherwise we must not call Slice::get() on it. In case one of
    // the subattributes we found so far is not an object, we fall back to the
    // regular comparison
    if (!first.isObject() || !second.isObject()) {
      break;
    }

    // fetch subattribute
    first = first.get(begin->name);
    first = first.resolveExternal();
    second = second.get(begin->name);
    second = second.resolveExternal();

    if (begin->shouldExpand && first.isArray() && second.isArray()) {
      if (first.length() != second.length()) {
        // Nonequal length, so there is a difference!
        // We have to play this carefully here. It is possible that the
        // set of values found is the same, but we must err on the side
        // of caution in this case and use the slow path. Note in
        // particular that the following code returns `true`, if one
        // of the arrays is empty, which is not correct!
        return false;
      }
      auto next = begin + 1;
      VPackArrayIterator it1(first), it2(second);
      while (it1.valid() && it2.valid()) {
        if (!attributesEqual(*it1, *it2, next, end)) {
          return false;
        }
        it1++;
        it2++;
      }
      return true;
    }

    auto dist = std::distance(begin, end);
    bool notF1 = first.isNone() || (dist == 1 && !first.isObject());
    bool notF2 = second.isNone() || (dist == 1 && !second.isObject());
    if (notF1 != notF2) {
      return false;
    }
    if (notF1 || notF2) {  // one of the paths was not found
      break;
    }
  }

  return basics::VelocyPackHelper::equal(first, second, true);
}
}  // namespace

Result RocksDBVPackIndex::update(
    transaction::Methods& trx, RocksDBMethods* mthds,
    LocalDocumentId const& oldDocumentId, velocypack::Slice oldDoc,
    LocalDocumentId const& newDocumentId, velocypack::Slice newDoc,
    OperationOptions const& options, bool performChecks) {
  if (!_unique) {
    // only unique index supports in-place updates
    // lets also not handle the complex case of expanded arrays
    return RocksDBIndex::update(trx, mthds, oldDocumentId, oldDoc,
                                newDocumentId, newDoc, options, performChecks);
  }

  if (!std::all_of(_fields.cbegin(), _fields.cend(), [&](auto const& path) {
        return ::attributesEqual(oldDoc, newDoc, path.begin(), path.end());
      })) {
    // change detected in some index attribute value.
    // we can only use in-place updates if no indexed attributes changed
    return RocksDBIndex::update(trx, mthds, oldDocumentId, oldDoc,
                                newDocumentId, newDoc, options, performChecks);
  }

  // update-in-place following...

  Result res;
  ::arangodb::containers::SmallVector<RocksDBKey>::allocator_type::arena_type
      elementsArena;
  ::arangodb::containers::SmallVector<RocksDBKey> elements{elementsArena};
  ::arangodb::containers::SmallVector<uint64_t>::allocator_type::arena_type
      hashesArena;
  ::arangodb::containers::SmallVector<uint64_t> hashes{hashesArena};
  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    auto r = fillElement(*leased, newDocumentId, newDoc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  RocksDBValue value = RocksDBValue::UniqueVPackIndexValue(newDocumentId);
  for (auto const& key : elements) {
    rocksdb::Status s =
        mthds->Put(_cf, key, value.string(), /*assume_tracked*/ false);
    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::index);
      addErrorMsg(res, newDoc.get(StaticStrings::KeyString).copyString());
      break;
    }
  }

  return res;
}

/// @brief removes a document from the index
Result RocksDBVPackIndex::remove(transaction::Methods& trx,
                                 RocksDBMethods* mthds,
                                 LocalDocumentId const& documentId,
                                 velocypack::Slice doc) {
  TRI_IF_FAILURE("BreakHashIndexRemove") {
    if (type() == arangodb::Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
      // intentionally  break index removal
      return Result(TRI_ERROR_INTERNAL,
                    "BreakHashIndexRemove failure point triggered");
    }
  }
  Result res;
  rocksdb::Status s;
  ::arangodb::containers::SmallVector<RocksDBKey>::allocator_type::arena_type
      elementsArena;
  ::arangodb::containers::SmallVector<RocksDBKey> elements{elementsArena};
  ::arangodb::containers::SmallVector<uint64_t>::allocator_type::arena_type
      hashesArena;
  ::arangodb::containers::SmallVector<uint64_t> hashes{hashesArena};

  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    auto r = fillElement(*leased, documentId, doc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  IndexingDisabler guard(
      mthds, !_unique && trx.state()->hasHint(
                             transaction::Hints::Hint::FROM_TOPLEVEL_AQL));

  size_t const count = elements.size();

  if (_unique) {
    for (size_t i = 0; i < count; ++i) {
      s = mthds->Delete(_cf, elements[i]);

      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s, rocksutils::index));
      }
    }
    if (res.fail()) {
      TRI_ASSERT(doc.get(StaticStrings::KeyString).isString());
      addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
    }
  } else {
    // non-unique index contain the unique objectID written exactly once
    for (size_t i = 0; i < count; ++i) {
      s = mthds->SingleDelete(_cf, elements[i]);

      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s, rocksutils::index));
      }
    }

    if (res.fail()) {
      TRI_ASSERT(doc.get(StaticStrings::KeyString).isString());
      addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
    } else if (_estimates) {
      auto* state = RocksDBTransactionState::toState(&trx);
      auto* trxc = static_cast<RocksDBTransactionCollection*>(
          state->findCollection(_collection.id()));
      TRI_ASSERT(trxc != nullptr);
      for (uint64_t hash : hashes) {
        // The estimator is only useful if we are in a non-unique indexes
        TRI_ASSERT(!_unique);
        trxc->trackIndexRemove(id(), hash);
      }
    }
  }

  return res;
}

/// @brief attempts to locate an entry in the index
/// Warning: who ever calls this function is responsible for destroying
/// the RocksDBVPackIndexIterator* results
std::unique_ptr<IndexIterator> RocksDBVPackIndex::lookup(
    transaction::Methods* trx, VPackSlice searchValues, bool reverse,
    ReadOwnWrites readOwnWrites) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(searchValues.length() <= _fields.size());

  if (ADB_UNLIKELY(searchValues.length() == 0)) {
    // full range search
    RocksDBKeyBounds bounds =
        _unique ? RocksDBKeyBounds::UniqueVPackIndex(objectId(), reverse)
                : RocksDBKeyBounds::VPackIndex(objectId(), reverse);

    return buildIterator(trx, std::move(bounds), reverse, readOwnWrites);
  }

  VPackBuilder leftSearch;

  VPackSlice lastNonEq;
  leftSearch.openArray();
  for (VPackSlice it : VPackArrayIterator(searchValues)) {
    TRI_ASSERT(it.isObject());
    VPackSlice eq = it.get(StaticStrings::IndexEq);
    if (eq.isNone()) {
      lastNonEq = it;
      break;
    }
    leftSearch.add(eq);
  }

  if (lastNonEq.isNone() && _unique &&
      searchValues.length() == _fields.size()) {
    leftSearch.close();

    return std::make_unique<RocksDBVPackUniqueIndexIterator>(
        &_collection, trx, this, leftSearch.slice(), readOwnWrites);
  }

  VPackSlice leftBorder;
  VPackSlice rightBorder;

  VPackBuilder rightSearch;

  if (lastNonEq.isNone()) {
    // We only have equality!
    rightSearch = leftSearch;

    leftSearch.add(VPackSlice::minKeySlice());
    leftSearch.close();

    rightSearch.add(VPackSlice::maxKeySlice());
    rightSearch.close();

    leftBorder = leftSearch.slice();
    rightBorder = rightSearch.slice();
  } else {
    // Copy rightSearch = leftSearch for right border
    rightSearch = leftSearch;

    // Define Lower-Bound
    VPackSlice lastLeft = lastNonEq.get(StaticStrings::IndexGe);
    if (!lastLeft.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(StaticStrings::IndexGt));
      leftSearch.add(lastLeft);
      leftSearch.add(VPackSlice::minKeySlice());
      leftSearch.close();
      VPackSlice search = leftSearch.slice();
      leftBorder = search;
    } else {
      lastLeft = lastNonEq.get(StaticStrings::IndexGt);
      if (!lastLeft.isNone()) {
        leftSearch.add(lastLeft);
        leftSearch.add(VPackSlice::maxKeySlice());
        leftSearch.close();
        VPackSlice search = leftSearch.slice();
        leftBorder = search;
      } else {
        // No lower bound set default to (null <= x)
        leftSearch.add(VPackSlice::minKeySlice());
        leftSearch.close();
        VPackSlice search = leftSearch.slice();
        leftBorder = search;
      }
    }

    // Define upper-bound
    VPackSlice lastRight = lastNonEq.get(StaticStrings::IndexLe);
    if (!lastRight.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(StaticStrings::IndexLt));
      rightSearch.add(lastRight);
      rightSearch.add(VPackSlice::maxKeySlice());
      rightSearch.close();
      VPackSlice search = rightSearch.slice();
      rightBorder = search;
    } else {
      lastRight = lastNonEq.get(StaticStrings::IndexLt);
      if (!lastRight.isNone()) {
        rightSearch.add(lastRight);
        rightSearch.add(VPackSlice::minKeySlice());
        rightSearch.close();
        VPackSlice search = rightSearch.slice();
        rightBorder = search;
      } else {
        // No upper bound set default to (x <= INFINITY)
        rightSearch.add(VPackSlice::maxKeySlice());
        rightSearch.close();
        VPackSlice search = rightSearch.slice();
        rightBorder = search;
      }
    }
  }

  RocksDBKeyBounds bounds = _unique ? RocksDBKeyBounds::UniqueVPackIndex(
                                          objectId(), leftBorder, rightBorder)
                                    : RocksDBKeyBounds::VPackIndex(
                                          objectId(), leftBorder, rightBorder);

  return buildIterator(trx, std::move(bounds), reverse, readOwnWrites);
}

Index::FilterCosts RocksDBVPackIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsFilterCondition(
      allIndexes, this, node, reference, itemsInIndex);
}

Index::SortCosts RocksDBVPackIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsSortCondition(
      this, sortCondition, reference, itemsInIndex);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBVPackIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  return SortedIndexAttributeMatcher::specializeCondition(this, node,
                                                          reference);
}

std::unique_ptr<IndexIterator> RocksDBVPackIndex::iteratorForCondition(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites, int) {
  TRI_ASSERT(!isSorted() || opts.sorted);

  VPackBuilder searchValues;
  searchValues.openArray();
  bool needNormalize = false;
  if (node == nullptr) {
    // We only use this index for sort. Empty searchValue
    VPackArrayBuilder guard(&searchValues);

    TRI_IF_FAILURE("PersistentIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_IF_FAILURE("SkiplistIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_IF_FAILURE("HashIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  } else {
    // Create the search values for the lookup
    VPackArrayBuilder guard(&searchValues);

    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>
        found;
    std::unordered_set<std::string> nonNullAttributes;
    [[maybe_unused]] size_t unused = 0;
    SortedIndexAttributeMatcher::matchAttributes(
        this, node, reference, found, unused, nonNullAttributes, true);

    // found contains all attributes that are relevant for this node.
    // It might be less than fields().
    //
    // Handle the first attributes. They can only be == or IN and only
    // one node per attribute

    auto getValueAccess = [&](arangodb::aql::AstNode const* comp,
                              arangodb::aql::AstNode const*& access,
                              arangodb::aql::AstNode const*& value) -> bool {
      access = comp->getMember(0);
      value = comp->getMember(1);
      std::pair<arangodb::aql::Variable const*,
                std::vector<arangodb::basics::AttributeName>>
          paramPair;
      if (!(access->isAttributeAccessForVariable(paramPair) &&
            paramPair.first == reference)) {
        access = comp->getMember(1);
        value = comp->getMember(0);
        if (!(access->isAttributeAccessForVariable(paramPair) &&
              paramPair.first == reference)) {
          // Both side do not have a correct AttributeAccess, this should not
          // happen and indicates
          // an error in the optimizer
          TRI_ASSERT(false);
        }
        return true;
      }
      return false;
    };

    size_t usedFields = 0;
    for (; usedFields < _fields.size(); ++usedFields) {
      auto it = found.find(usedFields);
      if (it == found.end()) {
        // We are either done
        // or this is a range.
        // Continue with more complicated loop
        break;
      }

      auto comp = it->second[0];
      TRI_ASSERT(comp->numMembers() == 2);
      arangodb::aql::AstNode const* access = nullptr;
      arangodb::aql::AstNode const* value = nullptr;
      getValueAccess(comp, access, value);
      // We found an access for this field

      if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
        searchValues.openObject();
        searchValues.add(VPackValue(StaticStrings::IndexEq));
        TRI_IF_FAILURE("PersistentIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("SkiplistIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("HashIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      } else if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        if (isAttributeExpanded(usedFields)) {
          searchValues.openObject();
          searchValues.add(VPackValue(StaticStrings::IndexEq));
          TRI_IF_FAILURE("PersistentIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          TRI_IF_FAILURE("SkiplistIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          TRI_IF_FAILURE("HashIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        } else {
          needNormalize = true;
          searchValues.openObject();
          searchValues.add(VPackValue(StaticStrings::IndexIn));
        }
      } else {
        // This is a one-sided range
        break;
      }
      // We have to add the value always, the key was added before
      value->toVelocyPackValue(searchValues);
      searchValues.close();
    }

    // Now handle the next element, which might be a range
    if (usedFields < _fields.size()) {
      auto it = found.find(usedFields);

      if (it != found.end()) {
        auto rangeConditions = it->second;
        TRI_ASSERT(rangeConditions.size() <= 2);

        VPackObjectBuilder searchElement(&searchValues);

        for (auto& comp : rangeConditions) {
          TRI_ASSERT(comp->numMembers() == 2);
          arangodb::aql::AstNode const* access = nullptr;
          arangodb::aql::AstNode const* value = nullptr;
          bool isReverseOrder = getValueAccess(comp, access, value);

          // Add the key
          switch (comp->type) {
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexGt));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexLt));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexGe));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexLe));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexLt));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexGt));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexLe));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexGe));
              }
              break;
            default:
              // unsupported right now. Should have been rejected by
              // supportsFilterCondition
              TRI_ASSERT(false);
              return std::make_unique<EmptyIndexIterator>(&_collection, trx);
          }

          // If the value does not have a vpack representation the index cannot
          // use it, and the results of the query are wrong.
          TRI_ASSERT(value->valueHasVelocyPackRepresentation());
          value->toVelocyPackValue(searchValues);
        }
      }
    }
  }
  searchValues.close();

  TRI_IF_FAILURE("PersistentIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SkiplistIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("HashIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (needNormalize) {
    transaction::BuilderLeaser expandedSearchValues(trx);
    expandInSearchValues(searchValues.slice(), *(expandedSearchValues.get()));
    VPackSlice expandedSlice = expandedSearchValues->slice();
    std::vector<std::unique_ptr<IndexIterator>> iterators;

    for (VPackSlice val : VPackArrayIterator(expandedSlice)) {
      iterators.push_back(lookup(trx, val, !opts.ascending, readOwnWrites));
    }

    if (!opts.ascending) {
      std::reverse(iterators.begin(), iterators.end());
    }

    return std::make_unique<MultiIndexIterator>(&_collection, trx, this,
                                                std::move(iterators));
  }

  VPackSlice searchSlice = searchValues.slice();
  TRI_ASSERT(searchSlice.length() == 1);
  searchSlice = searchSlice.at(0);
  return lookup(trx, searchSlice, !opts.ascending, readOwnWrites);
}

void RocksDBVPackIndex::afterTruncate(TRI_voc_tick_t tick,
                                      arangodb::transaction::Methods* trx) {
  if (unique() || _estimator == nullptr) {
    return;
  }
  TRI_ASSERT(_estimator != nullptr);
  _estimator->bufferTruncate(tick);
  RocksDBIndex::afterTruncate(tick, trx);
}

RocksDBCuckooIndexEstimatorType* RocksDBVPackIndex::estimator() {
  return _estimator.get();
}

void RocksDBVPackIndex::setEstimator(
    std::unique_ptr<RocksDBCuckooIndexEstimatorType> est) {
  TRI_ASSERT(!_unique);
  TRI_ASSERT(_estimator == nullptr ||
             _estimator->appliedSeq() <= est->appliedSeq());
  _estimator = std::move(est);
}

void RocksDBVPackIndex::recalculateEstimates() {
  if (unique() || _estimator == nullptr) {
    return;
  }
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  auto& selector =
      _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();

  auto bounds = getBounds();
  rocksdb::Slice const end = bounds.end();
  rocksdb::ReadOptions options;
  options.iterate_upper_bound = &end;   // safe to use on rocksb::DB directly
  options.prefix_same_as_start = true;  // key-prefix includes edge
  options.verify_checksums = false;
  options.fill_cache = false;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options, _cf));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    uint64_t hash = RocksDBVPackIndex::HashForKey(it->key());
    // cppcheck-suppress uninitvar ; doesn't understand above call
    _estimator->insert(hash);
  }
  _estimator->setAppliedSeq(seq);
}

std::unique_ptr<IndexIterator> RocksDBVPackIndex::buildIterator(
    transaction::Methods* trx, RocksDBKeyBounds bounds, bool reverse,
    ReadOwnWrites readOwnWrites) const {
  if (unique()) {
    // unique index
    if (reverse) {
      // reverse version
      return std::make_unique<RocksDBVPackIndexIterator<true, true>>(
          &_collection, trx, this, std::move(bounds), readOwnWrites);
    }
    // forward version
    return std::make_unique<RocksDBVPackIndexIterator<true, false>>(
        &_collection, trx, this, std::move(bounds), readOwnWrites);
  }

  // non-unique index
  if (reverse) {
    // reverse version
    return std::make_unique<RocksDBVPackIndexIterator<false, true>>(
        &_collection, trx, this, std::move(bounds), readOwnWrites);
  }
  // forward version
  return std::make_unique<RocksDBVPackIndexIterator<false, false>>(
      &_collection, trx, this, std::move(bounds), readOwnWrites);
}
