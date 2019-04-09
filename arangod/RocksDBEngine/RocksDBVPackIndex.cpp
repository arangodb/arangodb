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
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
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
  RocksDBVPackUniqueIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                                  arangodb::RocksDBVPackIndex const* index,
                                  VPackSlice const& indexValues)
      : IndexIterator(collection, trx),
        _index(index),
        _cmp(index->comparator()),
        _key(trx),
        _done(false) {
    TRI_ASSERT(index->columnFamily() == RocksDBColumnFamily::vpack());
    _key->constructUniqueVPackIndexValue(index->objectId(), indexValues);
  }

 public:
  char const* typeName() const override {
    return "rocksdb-unique-index-iterator";
  }

  /// @brief Get the next limit many element in the index
  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || _done) {
      // already looked up something
      return false;
    }

    _done = true;

    rocksdb::PinnableSlice ps;
    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(_trx);
    rocksdb::Status s = mthds->Get(_index->columnFamily(), _key->string(), &ps);

    if (s.ok()) {
      cb(RocksDBValue::documentId(ps));
    }

    // there is at most one element, so we are done now
    return false;
  }

  bool nextCovering(DocumentCallback const& cb, size_t limit) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || _done) {
      // already looked up something
      return false;
    }

    _done = true;

    rocksdb::PinnableSlice ps;
    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(_trx);
    rocksdb::Status s = mthds->Get(_index->columnFamily(), _key->string(), &ps);

    if (s.ok()) {
      cb(LocalDocumentId(RocksDBValue::documentId(ps)),
        RocksDBKey::indexedVPack(_key.ref()));
    }

    // there is at most one element, so we are done now
    return false;
  }

  /// @brief Reset the cursor
  void reset() override {
    TRI_ASSERT(_trx->state()->isRunning());

    _done = false;
  }

  /// @brief we provide a method to provide the index attribute values
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
class RocksDBVPackIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                            arangodb::RocksDBVPackIndex const* index,
                            bool reverse, RocksDBKeyBounds&& bounds)
      : IndexIterator(collection, trx),
        _index(index),
        _cmp(index->comparator()),
        _fullEnumerationObjectId(0),
        _reverse(reverse),
        _bounds(std::move(bounds)) {
    TRI_ASSERT(index->columnFamily() == RocksDBColumnFamily::vpack());

    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
    rocksdb::ReadOptions options = mthds->iteratorReadOptions();
    // we need to have a pointer to a slice for the upper bound
    // so we need to assign the slice to an instance variable here
    if (reverse) {
      _rangeBound = _bounds.start();
      options.iterate_lower_bound = &_rangeBound;
      VPackSlice s = VPackSlice(_rangeBound.data() + sizeof(uint64_t));
      if (s.isArray() && s.length() == 1 && s.at(0).isMinKey()) {
        // lower bound is the min key. that means we can get away with a
        // cheap outOfBounds comparator
        _fullEnumerationObjectId = _index->objectId();
      }
    } else {
      _rangeBound = _bounds.end();
      options.iterate_upper_bound = &_rangeBound;
      VPackSlice s = VPackSlice(_rangeBound.data() + sizeof(uint64_t));
      if (s.isArray() && s.length() == 1 && s.at(0).isMaxKey()) {
        // upper bound is the max key. that means we can get away with a
        // cheap outOfBounds comparator
        _fullEnumerationObjectId = _index->objectId();
      }
    }

    TRI_ASSERT(options.prefix_same_as_start);
    _iterator = mthds->NewIterator(options, index->columnFamily());
    if (reverse) {
      _iterator->SeekForPrev(_bounds.end());
    } else {
      _iterator->Seek(_bounds.start());
    }
  }

 public:
  char const* typeName() const override { return "rocksdb-index-iterator"; }

  /// @brief Get the next limit many elements in the index
  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || !_iterator->Valid() || outOfRange()) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    while (limit > 0) {
      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(_iterator->key()));

      cb(_index->_unique ? RocksDBValue::documentId(_iterator->value())
                        : RocksDBKey::indexDocumentId(_bounds.type(), _iterator->key()));

      --limit;
      if (!advance()) {
        return false;
      }
    }

    return true;
  }

  bool nextCovering(DocumentCallback const& cb, size_t limit) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || !_iterator->Valid() || outOfRange()) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    while (limit > 0) {
      rocksdb::Slice const& key = _iterator->key();
      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(key));

      LocalDocumentId const documentId(
          _index->_unique ? RocksDBValue::documentId(_iterator->value())
                          : RocksDBKey::indexDocumentId(_bounds.type(), key));
      cb(documentId, RocksDBKey::indexedVPack(key));

      --limit;
      if (!advance()) {
        return false;
      }
    }

    return true;
  }

  void skip(uint64_t count, uint64_t& skipped) override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (!_iterator->Valid() || outOfRange()) {
      return;
    }

    while (count > 0) {
      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(_iterator->key()));

      --count;
      ++skipped;
      if (!advance()) {
        return;
      }
    }
  }

  /// @brief Reset the cursor
  void reset() override {
    TRI_ASSERT(_trx->state()->isRunning());

    if (_reverse) {
      _iterator->SeekForPrev(_bounds.end());
    } else {
      _iterator->Seek(_bounds.start());
    }
  }

  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override {
    return _index->type() != arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX; 
  }

 private:
  inline bool outOfRange() const {
    if (_fullEnumerationObjectId) {
      // we are enumerating the entire index
      // so we can use a cheap comparator that only checks the objectId
      return (arangodb::rocksutils::uint64FromPersistent(_iterator->key().data()) != _fullEnumerationObjectId);
    }

    // we are enumerating a subset of the index
    // so we really need to run the full-featured (read: expensive)
    // comparator

    if (_reverse) {
      return (_cmp->Compare(_iterator->key(), _rangeBound) < 0);
    } else {
      return (_cmp->Compare(_iterator->key(), _rangeBound) > 0);
    }
  }

  inline bool advance() {
    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }

    return _iterator->Valid() && !outOfRange();
  }

  arangodb::RocksDBVPackIndex const* _index;
  rocksdb::Comparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  uint64_t _fullEnumerationObjectId;
  bool const _reverse;
  RocksDBKeyBounds _bounds;
  // used for iterate_upper_bound iterate_lower_bound
  rocksdb::Slice _rangeBound;
};

} // namespace

uint64_t RocksDBVPackIndex::HashForKey(const rocksdb::Slice& key) {
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  VPackSlice tmp = RocksDBKey::indexedVPack(key);
  return tmp.normalizedHash();
}

/// @brief create the index
RocksDBVPackIndex::RocksDBVPackIndex(TRI_idx_iid_t iid, arangodb::LogicalCollection& collection,
                                     arangodb::velocypack::Slice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::vpack(),
                   /*useCache*/ false),
      _deduplicate(
          arangodb::basics::VelocyPackHelper::getBooleanValue(info,
                                                              "deduplicate", true)),
      _allowPartialIndex(true),
      _estimator(nullptr) {
  TRI_ASSERT(_cf == RocksDBColumnFamily::vpack());

  if (!_unique && !ServerState::instance()->isCoordinator()) {
    // We activate the estimator for all non unique-indexes.
    // And only on DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(
        RocksDBIndex::ESTIMATOR_SIZE);
    TRI_ASSERT(_estimator != nullptr);
  }
  TRI_ASSERT(!_fields.empty());

  TRI_ASSERT(iid != 0);

  fillPaths(_paths, _expanding);
}

/// @brief destroy the index
RocksDBVPackIndex::~RocksDBVPackIndex() {}

double RocksDBVPackIndex::selectivityEstimate(arangodb::velocypack::StringRef const&) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

/// @brief return a VelocyPack representation of the index
void RocksDBVPackIndex::toVelocyPack(VPackBuilder& builder,
                                     std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(_unique));
  builder.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(_sparse));
  builder.add("deduplicate", VPackValue(_deduplicate));
  builder.close();
}

/// @brief helper function to insert a document into any index type
/// Should result in an elements vector filled with the new index entries
/// uses the _unique field to determine the kind of key structure
int RocksDBVPackIndex::fillElement(VPackBuilder& leased, LocalDocumentId const& documentId,
                                   VPackSlice const& doc, SmallVector<RocksDBKey>& elements,
                                   SmallVector<uint64_t>& hashes) {
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
      key.constructUniqueVPackIndexValue(_objectId, leased.slice());
    } else {
      // Non-unique VPack index values are stored as follows:
      // - Key: 6 + 8-byte object ID of index + VPack array with index
      // value(s) + revisionID
      // - Value: empty
      key.constructVPackIndexValue(_objectId, leased.slice(), documentId);
      hashes.push_back(leased.slice().normalizedHash());
    }
  } else {
    // other path for handling array elements, too

    SmallVector<VPackSlice>::allocator_type::arena_type sliceStackArena;
    SmallVector<VPackSlice> sliceStack{sliceStackArena};

    try {
      buildIndexValues(leased, documentId, doc, 0, elements, hashes, sliceStack);
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

void RocksDBVPackIndex::addIndexValue(VPackBuilder& leased, LocalDocumentId const& documentId,
                                      VPackSlice const& document,
                                      SmallVector<RocksDBKey>& elements,
                                      SmallVector<uint64_t>& hashes,
                                      SmallVector<VPackSlice>& sliceStack) {
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
    key.constructUniqueVPackIndexValue(_objectId, leased.slice());
    elements.emplace_back(std::move(key));
  } else {
    // Non-unique VPack index values are stored as follows:
    // - Key: 6 + 8-byte object ID of index + VPack array with index value(s)
    // + primary key
    // - Value: empty
    RocksDBKey key;
    key.constructVPackIndexValue(_objectId, leased.slice(), documentId);
    elements.emplace_back(std::move(key));
    hashes.push_back(leased.slice().normalizedHash());
  }
}

/// @brief helper function to create a set of index combinations to insert
void RocksDBVPackIndex::buildIndexValues(VPackBuilder& leased,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const doc, size_t level,
                                         SmallVector<RocksDBKey>& elements,
                                         SmallVector<uint64_t>& hashes,
                                         SmallVector<VPackSlice>& sliceStack) {
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
    buildIndexValues(leased, documentId, doc, level + 1, elements, hashes, sliceStack);
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

  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash, arangodb::basics::VelocyPackHelper::VPackEqual>
      seen(2, arangodb::basics::VelocyPackHelper::VPackHash(),
           arangodb::basics::VelocyPackHelper::VPackEqual());

  auto moveOn = [&](VPackSlice something) -> void {
    auto it = seen.find(something);
    if (it == seen.end()) {
      seen.insert(something);
      sliceStack.emplace_back(something);
      buildIndexValues(leased, documentId, doc, level + 1, elements, hashes, sliceStack);
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
void RocksDBVPackIndex::fillPaths(std::vector<std::vector<std::string>>& paths,
                                  std::vector<int>& expanding) {
  paths.clear();
  expanding.clear();
  for (std::vector<arangodb::basics::AttributeName> const& list : _fields) {
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
    expanding.emplace_back(expands);
  }
}

/// @brief inserts a document into the index
Result RocksDBVPackIndex::insert(transaction::Methods& trx, RocksDBMethods* mthds,
                                 LocalDocumentId const& documentId,
                                 velocypack::Slice const& doc,
                                 Index::OperationMode mode) {
  Result res;
  rocksdb::Status s;
  SmallVector<RocksDBKey>::allocator_type::arena_type elementsArena;
  SmallVector<RocksDBKey> elements{elementsArena};
  SmallVector<uint64_t>::allocator_type::arena_type hashesArena;
  SmallVector<uint64_t> hashes{hashesArena};

  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    int r = fillElement(*(leased.get()), documentId, doc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  IndexingDisabler guard(mthds, !_unique && trx.state()->hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL));

  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  RocksDBValue value = _unique ? RocksDBValue::UniqueVPackIndexValue(documentId)
                               : RocksDBValue::VPackIndexValue();

  size_t const count = elements.size();
  rocksdb::PinnableSlice existing;
  for (size_t i = 0; i < count; ++i) {
    RocksDBKey& key = elements[i];
    if (_unique) {
      s = mthds->Get(_cf, key.string(), &existing);
      if (s.ok()) {  // detected conflicting index entry
        res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
        break;
      }
      
      s = mthds->Put(_cf, key, value.string());
    } else {
      TRI_ASSERT(key.containsLocalDocumentId(documentId));
      s = mthds->PutUntracked(_cf, key, value.string());
    }

    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::index);
      break;
    }
  }

  if (res.ok() && !_unique) {
    auto state = RocksDBTransactionState::toState(&trx);

    for (auto& it : hashes) {
      // The estimator is only useful if we are in a non-unique indexes
      TRI_ASSERT(!_unique);
      state->trackIndexInsert(_collection.id(), id(), it);
    }
  } else if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
    // find conflicting document
    LocalDocumentId docId = RocksDBValue::documentId(existing);
    std::string existingKey;
    auto success = _collection.getPhysical()->readDocumentWithCallback(
        &trx, docId, [&](LocalDocumentId const&, VPackSlice doc) {
          existingKey = transaction::helpers::extractKeyFromDocument(doc).copyString();
        });
    TRI_ASSERT(success);

    if (mode == OperationMode::internal) {
      res.resetErrorMessage(std::move(existingKey));
    } else {
      addErrorMsg(res, existingKey);
    }
  } else if (res.fail()) {
    addErrorMsg(res);
  }

  return res;
}

namespace {
  bool attributesEqual(VPackSlice first, VPackSlice second,
                       std::vector<arangodb::basics::AttributeName>::const_iterator begin,
                       std::vector<arangodb::basics::AttributeName>::const_iterator end) {
    for (; begin != end; ++begin) {
      // fetch subattribute
      first = first.get(begin->name);
      second = second.get(begin->name);
      if (first.isExternal()) {
        first = first.resolveExternal();
      }
      if (second.isExternal()) {
        second = second.resolveExternal();
      }
      
      if (begin->shouldExpand &&
          first.isArray() && second.isArray()) {
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
      
      int dist = std::distance(begin, end);
      bool notF1 = first.isNone() || (dist == 1 && !first.isObject());
      bool notF2 = second.isNone() || (dist == 1 && !second.isObject());
      if (notF1 != notF2) {
        return false;
      }
      if (notF1 || notF2) { // one of the paths was not found
        break;
      }
    }
    
    return (basics::VelocyPackHelper::compare(first, second, true) == 0);
  }
} // namespace

Result RocksDBVPackIndex::update(transaction::Methods& trx, RocksDBMethods* mthds,
                                 LocalDocumentId const& oldDocumentId,
                                 velocypack::Slice const& oldDoc,
                                 LocalDocumentId const& newDocumentId,
                                 velocypack::Slice const& newDoc,
                                 Index::OperationMode mode) {
  if (!_unique) {
    // only unique index supports in-place updates
    // lets also not handle the complex case of expanded arrays
    return RocksDBIndex::update(trx, mthds, oldDocumentId, oldDoc,
                                newDocumentId, newDoc, mode);
  }
  
  bool equal = true;
  for (std::vector<basics::AttributeName> const& path : _fields) {
    if (!::attributesEqual(oldDoc, newDoc, path.begin(), path.end())) {
      equal = false;
      break;
    }
  }
  if (!equal) {
    // we can only use in-place updates if no indexed attributes changed
    return RocksDBIndex::update(trx, mthds, oldDocumentId, oldDoc,
                                newDocumentId, newDoc, mode);
  }
  
  Result res;
  // more expensive method to
  SmallVector<RocksDBKey>::allocator_type::arena_type elementsArena;
  SmallVector<RocksDBKey> elements{elementsArena};
  SmallVector<uint64_t>::allocator_type::arena_type hashesArena;
  SmallVector<uint64_t> hashes{hashesArena};
  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    int r = fillElement(*(leased.get()), newDocumentId, newDoc, elements, hashes);
    
    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }
  
  RocksDBValue value = RocksDBValue::UniqueVPackIndexValue(newDocumentId);
  size_t const count = elements.size();
  for (size_t i = 0; i < count; ++i) {
    RocksDBKey& key = elements[i];
    rocksdb::Status s = mthds->Put(_cf, key, value.string());
    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::index);
      break;
    }
  }
  
  return res;
}

/// @brief removes a document from the index
Result RocksDBVPackIndex::remove(transaction::Methods& trx, RocksDBMethods* mthds,
                                 LocalDocumentId const& documentId,
                                 velocypack::Slice const& doc,
                                 Index::OperationMode mode) {
  Result res;
  rocksdb::Status s;
  SmallVector<RocksDBKey>::allocator_type::arena_type elementsArena;
  SmallVector<RocksDBKey> elements{elementsArena};
  SmallVector<uint64_t>::allocator_type::arena_type hashesArena;
  SmallVector<uint64_t> hashes{hashesArena};

  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    int r = fillElement(*(leased.get()), documentId, doc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  IndexingDisabler guard(mthds, !_unique && trx.state()->hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL));

  size_t const count = elements.size();

  if (_unique) {
    for (size_t i = 0; i < count; ++i) {
      s = mthds->Delete(_cf, elements[i]);

      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s, rocksutils::index));
      }
    }
  } else {
    // non-unique index contain the unique objectID written exactly once
    for (size_t i = 0; i < count; ++i) {
      s = mthds->SingleDelete(_cf, elements[i]);
      
      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s, rocksutils::index));
      }
    }
  }

  if (res.ok() && !_unique) {
    auto state = RocksDBTransactionState::toState(&trx);

    for (auto& it : hashes) {
      // The estimator is only useful if we are in a non-unique indexes
      TRI_ASSERT(!_unique);
      state->trackIndexRemove(_collection.id(), id(), it);
    }
  } else if (res.fail()) {
    addErrorMsg(res);
  }

  return res;
}

/// @brief attempts to locate an entry in the index
/// Warning: who ever calls this function is responsible for destroying
/// the RocksDBVPackIndexIterator* results
IndexIterator* RocksDBVPackIndex::lookup(transaction::Methods* trx,
                                         VPackSlice const searchValues, bool reverse) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(searchValues.length() <= _fields.size());

  VPackBuilder leftSearch;

  VPackSlice lastNonEq;
  leftSearch.openArray();
  for (auto const& it : VPackArrayIterator(searchValues)) {
    TRI_ASSERT(it.isObject());
    VPackSlice eq = it.get(StaticStrings::IndexEq);
    if (eq.isNone()) {
      lastNonEq = it;
      break;
    }
    leftSearch.add(eq);
  }

  if (lastNonEq.isNone() && _unique && searchValues.length() == _fields.size()) {
    leftSearch.close();

    return new RocksDBVPackUniqueIndexIterator(&_collection, trx, this,
                                               leftSearch.slice());
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

  RocksDBKeyBounds bounds =
      _unique ? RocksDBKeyBounds::UniqueVPackIndex(_objectId, leftBorder, rightBorder)
              : RocksDBKeyBounds::VPackIndex(_objectId, leftBorder, rightBorder);

  return new RocksDBVPackIndexIterator(&_collection, trx, this, reverse, std::move(bounds));
}

bool RocksDBVPackIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex, size_t& estimatedItems, double& estimatedCost) const {
  return SortedIndexAttributeMatcher::supportsFilterCondition(allIndexes, this,
                                                                node, reference,
                                                                itemsInIndex, estimatedItems,
                                                                estimatedCost);
}

bool RocksDBVPackIndex::supportsSortCondition(arangodb::aql::SortCondition const* sortCondition,
                                              arangodb::aql::Variable const* reference,
                                              size_t itemsInIndex, double& estimatedCost,
                                              size_t& coveredAttributes) const {
  return SortedIndexAttributeMatcher::supportsSortCondition(this, sortCondition, reference,
                                                              itemsInIndex, estimatedCost,
                                                              coveredAttributes);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBVPackIndex::specializeCondition(
    arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference) const {
  return SortedIndexAttributeMatcher::specializeCondition(this, node, reference);
}

IndexIterator* RocksDBVPackIndex::iteratorForCondition(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts) {
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

    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
    std::unordered_set<std::string> nonNullAttributes;
    size_t unused = 0;

    SortedIndexAttributeMatcher::matchAttributes(this, node, reference, found,
                                                 unused, nonNullAttributes, true);

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
      std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>> paramPair;
      if (!(access->isAttributeAccessForVariable(paramPair) && paramPair.first == reference)) {
        access = comp->getMember(1);
        value = comp->getMember(0);
        if (!(access->isAttributeAccessForVariable(paramPair) && paramPair.first == reference)) {
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
              return new EmptyIndexIterator(&_collection, trx);
          }

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
    std::vector<IndexIterator*> iterators;

    try {
      for (VPackSlice val : VPackArrayIterator(expandedSlice)) {
        auto iterator = lookup(trx, val, !opts.ascending);

        try {
          iterators.push_back(iterator);
        } catch (...) {
          // avoid leak
          delete iterator;
          throw;
        }
      }

      if (!opts.ascending) {
        std::reverse(iterators.begin(), iterators.end());
      }
    } catch (...) {
      for (auto& it : iterators) {
        delete it;
      }
      throw;
    }

    return new MultiIndexIterator(&_collection, trx, this, iterators);
  }

  VPackSlice searchSlice = searchValues.slice();
  TRI_ASSERT(searchSlice.length() == 1);
  searchSlice = searchSlice.at(0);
  return lookup(trx, searchSlice, !opts.ascending);
}

void RocksDBVPackIndex::afterTruncate(TRI_voc_tick_t tick) {
  if (unique()) {
    return;
  }
  TRI_ASSERT(_estimator != nullptr);
  _estimator->bufferTruncate(tick);
  RocksDBIndex::afterTruncate(tick);
}

RocksDBCuckooIndexEstimator<uint64_t>* RocksDBVPackIndex::estimator() {
  return _estimator.get();
}

void RocksDBVPackIndex::setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> est) {
  TRI_ASSERT(!_unique);
  _estimator = std::move(est);
}

void RocksDBVPackIndex::recalculateEstimates() {
  if (unique()) {
    return;
  }

  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
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
    _estimator->insert(hash);
  }
  _estimator->setCommitSeq(seq);
}
