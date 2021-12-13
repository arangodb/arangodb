////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBPrimaryIndex.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cache/CachedValue.h"
#include "Cache/TransactionalCache.h"
#include "Cluster/ServerState.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"

#include "RocksDBEngine/RocksDBPrefixExtractor.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/VirtualClusterSmartEdgeCollection.h"
#endif

#include <rocksdb/iterator.h>
#include <rocksdb/utilities/transaction.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
std::string const lowest;            // smallest possible key
std::string const highest(KeyGenerator::maxKeyLength, std::numeric_limits<std::string::value_type>::max());  // greatest possible key
}  // namespace

// ================ Primary Index Iterators ================

namespace arangodb {

class RocksDBPrimaryIndexEqIterator final : public IndexIterator {
 public:
  RocksDBPrimaryIndexEqIterator(LogicalCollection* collection,
                                transaction::Methods* trx, RocksDBPrimaryIndex* index,
                                std::unique_ptr<VPackBuilder> key,
                                bool allowCoveringIndexOptimization,
                                ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _key(std::move(key)),
        _done(false),
        _allowCoveringIndexOptimization(allowCoveringIndexOptimization) {
    TRI_ASSERT(_key->slice().isString());
  }

  ~RocksDBPrimaryIndexEqIterator() {
    if (_key != nullptr) {
      // return the VPackBuilder to the transaction context
      _trx->transactionContextPtr()->returnBuilder(_key.release());
    }
  }

  char const* typeName() const override { return "primary-index-eq-iterator"; }

  /// @brief index supports rearming
  bool canRearm() const override { return true; }

  /// @brief rearm the index iterator
  bool rearmImpl(arangodb::aql::AstNode const* node, arangodb::aql::Variable const* variable,
                 IndexIteratorOptions const& opts) override {
    TRI_ASSERT(node != nullptr);
    TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
    TRI_ASSERT(node->numMembers() == 1);
    AttributeAccessParts aap(node->getMember(0), variable);
    TRI_ASSERT(aap.opType == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ);

    // handle the sole element
    _key->clear();
    _index->handleValNode(_trx, _key.get(), aap.value, !_allowCoveringIndexOptimization);

    TRI_IF_FAILURE("PrimaryIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    return !_key->isEmpty();
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    if (limit == 0 || _done) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    _done = true;
    LocalDocumentId documentId =
        _index->lookupKey(_trx, _key->slice().stringView(), canReadOwnWrites());
    if (documentId.isSet()) {
      cb(documentId);
    }
    return false;
  }

  /// @brief extracts just _key. not supported for use with _id
  bool nextCoveringImpl(DocumentCallback const& cb, size_t limit) override {
    TRI_ASSERT(_allowCoveringIndexOptimization);
    if (limit == 0 || _done) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    _done = true;
    LocalDocumentId documentId =
        _index->lookupKey(_trx, _key->slice().stringView(), canReadOwnWrites());
    if (documentId.isSet()) {
      cb(documentId, _key->slice());
    }
    return false;
  }

  void resetImpl() override { _done = false; }

  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override { return _allowCoveringIndexOptimization; }

 private:
  RocksDBPrimaryIndex* _index;
  std::unique_ptr<VPackBuilder> _key;
  bool _done;
  bool const _allowCoveringIndexOptimization;
};

class RocksDBPrimaryIndexInIterator final : public IndexIterator {
 public:
  RocksDBPrimaryIndexInIterator(LogicalCollection* collection,
                                transaction::Methods* trx, RocksDBPrimaryIndex* index,
                                std::unique_ptr<VPackBuilder> keys,
                                bool allowCoveringIndexOptimization)
      : IndexIterator(collection, trx, ReadOwnWrites::no), // "in"-checks never need to observe own writes.
        _index(index),
        _keys(std::move(keys)),
        _iterator(_keys->slice()),
        _allowCoveringIndexOptimization(allowCoveringIndexOptimization) {
    TRI_ASSERT(_keys->slice().isArray());
  }

  ~RocksDBPrimaryIndexInIterator() {
    if (_keys != nullptr) {
      // return the VPackBuilder to the transaction context
      _trx->transactionContextPtr()->returnBuilder(_keys.release());
    }
  }

  char const* typeName() const override { return "primary-index-in-iterator"; }

  /// @brief index supports rearming
  bool canRearm() const override { return true; }

  /// @brief rearm the index iterator
  bool rearmImpl(arangodb::aql::AstNode const* node, arangodb::aql::Variable const* variable,
                 IndexIteratorOptions const& opts) override {
    TRI_ASSERT(node != nullptr);
    TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
    TRI_ASSERT(node->numMembers() == 1);
    AttributeAccessParts aap(node->getMember(0), variable);
    TRI_ASSERT(aap.opType == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);

    if (aap.value->isArray()) {
      _index->fillInLookupValues(_trx, *(_keys.get()), aap.value, opts.ascending,
                                 !_allowCoveringIndexOptimization);
      _iterator = VPackArrayIterator(_keys->slice());
      return true;
    }

    return false;
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    if (limit == 0 || !_iterator.valid()) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    while (limit > 0) {
      // This is an in-iterator, and "in"-checks never need to observe own writes.
      LocalDocumentId documentId =
          _index->lookupKey(_trx, (*_iterator).stringView(), ReadOwnWrites::no);
      if (documentId.isSet()) {
        cb(documentId);
        --limit;
      }

      _iterator.next();
      if (!_iterator.valid()) {
        return false;
      }
    }
    return true;
  }

  bool nextCoveringImpl(DocumentCallback const& cb, size_t limit) override {
    TRI_ASSERT(_allowCoveringIndexOptimization);
    if (limit == 0 || !_iterator.valid()) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    while (limit > 0) {
      // This is an in-iterator, and "in"-checks never need to observe own writes.
      LocalDocumentId documentId =
          _index->lookupKey(_trx, (*_iterator).stringView(), ReadOwnWrites::no);
      if (documentId.isSet()) {
        cb(documentId, *_iterator);
        --limit;
      }

      _iterator.next();
      if (!_iterator.valid()) {
        return false;
      }
    }
    return true;
  }

  void resetImpl() override { _iterator.reset(); }

  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override { return _allowCoveringIndexOptimization; }

 private:
  RocksDBPrimaryIndex* _index;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
  bool const _allowCoveringIndexOptimization;
};

template <bool reverse>
class RocksDBPrimaryIndexRangeIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBPrimaryIndexRangeIterator(LogicalCollection* collection, transaction::Methods* trx,
                                   arangodb::RocksDBPrimaryIndex const* index,
                                   RocksDBKeyBounds&& bounds, ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _cmp(index->comparator()),
        _mustSeek(true),
        _mustCheckBounds(RocksDBTransactionState::toState(trx)->iteratorMustCheckBounds(collection->id(), readOwnWrites)),
        _bounds(std::move(bounds)),
        _rangeBound(reverse ? _bounds.start() : _bounds.end()) {
    TRI_ASSERT(index->columnFamily() ==
               RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::PrimaryIndex));
  }

 public:
  char const* typeName() const override {
    return "primary-index-range-iterator";
  }
  
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

      cb(RocksDBValue::documentId(_iterator->value()));

      --limit;
      if constexpr (reverse) {
        _iterator->Prev();
      } else {
        _iterator->Next();
      }

      if (ADB_UNLIKELY(!_iterator->Valid())) {
        // validate that Iterator is in a good shape and hasn't failed
        arangodb::rocksutils::checkIteratorStatus(_iterator.get());
        return false;
      } else if (outOfRange()) {
        return false;
      }

      if (limit == 0) {
        return true;
      }
    } while (true);
  }
  
  bool nextCoveringImpl(DocumentCallback const& cb, size_t limit) override {
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

    transaction::BuilderLeaser builder(transaction());

    do {
      LocalDocumentId documentId = RocksDBValue::documentId(_iterator->value());
      std::string_view key = RocksDBKey::primaryKey(_iterator->key());

      builder->clear();
      builder->add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
      cb(documentId, builder->slice());

      --limit;
      if constexpr (reverse) {
        _iterator->Prev();
      } else {
        _iterator->Next();
      }
      
      if (ADB_UNLIKELY(!_iterator->Valid())) {
        // validate that Iterator is in a good shape and hasn't failed
        arangodb::rocksutils::checkIteratorStatus(_iterator.get());
        return false;
      } else if (outOfRange()) {
        return false;
      }

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
        TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(_iterator->key()));

        --count;
        ++skipped;
        if constexpr (reverse) {
          _iterator->Prev();
        } else {
          _iterator->Next();
        }
      
        if (!_iterator->Valid() || outOfRange()) {
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
  bool hasCovering() const override { return true; }

 private:
  void ensureIterator() {
    if (_iterator == nullptr) {
      auto state = RocksDBTransactionState::toState(_trx);
      RocksDBTransactionMethods* mthds = state->rocksdbMethods(_collection->id());
      _iterator = mthds->NewIterator(_index->columnFamily(), [&](rocksdb::ReadOptions& options) {
        TRI_ASSERT(options.prefix_same_as_start);
        // we need to have a pointer to a slice for the upper bound
        // so we need to assign the slice to an instance variable here
        if constexpr (reverse) {
          options.iterate_lower_bound = &_rangeBound;
        } else {
          options.iterate_upper_bound = &_rangeBound;
        }
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

  inline bool outOfRange() const {
    TRI_ASSERT(_trx->state()->isRunning());
    // we can effectively disable the out-of-range checks for read-only
    // transactions, as our Iterator is a snapshot-based iterator with a 
    // configured iterate_upper_bound/iterate_lower_bound value. 
    // this makes RocksDB filter out non-matching keys automatically.
    // however, for a write transaction our Iterator is a rocksdb BaseDeltaIterator,
    // which will merge the values from a snapshot iterator and the changes in
    // the current transaction. here rocksdb will only apply the bounds checks
    // for the base iterator (from the snapshot), but not for the delta iterator
    // (from the current transaction), so we still have to carry out the checks
    // ourselves.
    if constexpr (reverse) {
      return _mustCheckBounds && (_cmp->Compare(_iterator->key(), _rangeBound) < 0);
    } else {
      return _mustCheckBounds && (_cmp->Compare(_iterator->key(), _rangeBound) > 0);
    }
  }

  arangodb::RocksDBPrimaryIndex const* _index;
  rocksdb::Comparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  bool _mustSeek;
  bool const _mustCheckBounds;
  RocksDBKeyBounds const _bounds;
  // used for iterate_upper_bound iterate_lower_bound
  rocksdb::Slice _rangeBound;
};

}  // namespace arangodb

// ================ PrimaryIndex ================

RocksDBPrimaryIndex::RocksDBPrimaryIndex(arangodb::LogicalCollection& collection,
                                         arangodb::velocypack::Slice const& info)
    : RocksDBIndex(
          IndexId::primary(), collection, StaticStrings::IndexNamePrimary,
          std::vector<std::vector<arangodb::basics::AttributeName>>(
              {{arangodb::basics::AttributeName(StaticStrings::KeyString, false)}}),
          true, false,
          RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::PrimaryIndex),
          basics::VelocyPackHelper::stringUInt64(info, StaticStrings::ObjectId),
          static_cast<RocksDBCollection*>(collection.getPhysical())->cacheEnabled()),
      _coveredFields({{AttributeName(StaticStrings::KeyString, false)},
                      {AttributeName(StaticStrings::IdString, false)}}),
      _isRunningInCluster(ServerState::instance()->isRunningInCluster()) {
  TRI_ASSERT(_cf == RocksDBColumnFamilyManager::get(
                        RocksDBColumnFamilyManager::Family::PrimaryIndex));
  TRI_ASSERT(objectId() != 0);
}

RocksDBPrimaryIndex::~RocksDBPrimaryIndex() = default;

std::vector<std::vector<arangodb::basics::AttributeName>> const& RocksDBPrimaryIndex::coveredFields() const {
  return _coveredFields;
}

void RocksDBPrimaryIndex::load() {
  RocksDBIndex::load();
  if (useCache()) {
    // FIXME: make the factor configurable
    RocksDBCollection* rdb = static_cast<RocksDBCollection*>(_collection.getPhysical());
    uint64_t numDocs = rdb->meta().numberDocuments();

    if (numDocs > 0) {
      _cache->sizeHint(static_cast<uint64_t>(0.3 * numDocs));
    }
  }
}

/// @brief return a VelocyPack representation of the index
void RocksDBPrimaryIndex::toVelocyPack(VPackBuilder& builder,
                                       std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.close();
}

LocalDocumentId RocksDBPrimaryIndex::lookupKey(transaction::Methods* trx,
                                               std::string_view keyRef,
                                               ReadOwnWrites readOwnWrites) const {
  RocksDBKeyLeaser key(trx);
  key->constructPrimaryIndexValue(objectId(), keyRef);

  bool lockTimeout = false;
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {
      rocksdb::Slice s(reinterpret_cast<char const*>(f.value()->value()),
                       f.value()->valueSize());
      return RocksDBValue::documentId(s);
    } else if (f.result().errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
      // assuming someone is currently holding a write lock, which
      // is why we cannot access the TransactionalBucket.
      lockTimeout = true;  // we skip the insert in this case
    }
  }

  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx, _collection.id());
  rocksdb::PinnableSlice val;
  rocksdb::Status s = mthds->Get(_cf, key->string(), &val, readOwnWrites);
  if (!s.ok()) {
    return LocalDocumentId();
  }

  if (useCache() && !lockTimeout) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    std::size_t attempts = 0;
    cache::Cache::Inserter inserter(*_cache, key->string().data(),
                                    static_cast<uint32_t>(key->string().size()),
                                    val.data(), static_cast<uint64_t>(val.size()),
                                    [&attempts](Result const& res) -> bool {
                                      return res.is(TRI_ERROR_LOCK_TIMEOUT) &&
                                             ++attempts < 2;
                                    });
  }

  return RocksDBValue::documentId(val);
}

/// @brief reads a revision id from the primary index
/// if the document does not exist, this function will return false
/// if the document exists, the function will return true
/// the revision id will only be non-zero if the primary index
/// value contains the document's revision id. note that this is not
/// the case for older collections
/// in this case the caller must fetch the revision id from the actual
/// document
bool RocksDBPrimaryIndex::lookupRevision(transaction::Methods* trx,
                                         std::string_view keyRef,
                                         LocalDocumentId& documentId,
                                         RevisionId& revisionId,
                                         ReadOwnWrites readOwnWrites) const {
  documentId = LocalDocumentId::none();
  revisionId = RevisionId::none();

  RocksDBKeyLeaser key(trx);
  key->constructPrimaryIndexValue(objectId(), keyRef);

  // acquire rocksdb transaction
  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx, _collection.id());
  rocksdb::PinnableSlice val;
  rocksdb::Status s = mthds->Get(_cf, key->string(), &val, readOwnWrites);
  if (!s.ok()) {
    return false;
  }

  documentId = RocksDBValue::documentId(val);

  // this call will populate revisionId if the revision id value is
  // stored in the primary index
  revisionId = RocksDBValue::revisionId(val);
  return true;
}

Result RocksDBPrimaryIndex::probeKey(transaction::Methods& trx, 
                                     RocksDBMethods* mthd,
                                     RocksDBKeyLeaser const& key,
                                     arangodb::velocypack::Slice keySlice,
                                     OperationOptions const& options,
                                     bool insert) {
  TRI_ASSERT(keySlice.isString());

  bool const lock = !RocksDBTransactionState::toState(&trx)->isOnlyExclusiveTransaction();
  IndexOperationMode mode = options.indexOperationMode;
  
  transaction::StringLeaser leased(&trx);
  rocksdb::PinnableSlice ps(leased.get());
  Result res;
  rocksdb::Status s;
  if (lock) {
    s = mthd->GetForUpdate(_cf, key->string(), &ps);
  } else {
    // modifications always need to observe all changes in order to validate uniqueness constraints
    s = mthd->Get(_cf, key->string(), &ps, ReadOwnWrites::yes);
  }

  if (insert) {
    // INSERT case
    if (s.ok()) {  // detected conflicting primary key
      if (mode == IndexOperationMode::internal) {
      // in this error mode, we return the conflicting document's key
      // inside the error message string (and nothing else)!
        return res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, keySlice.copyString());
      }
      // build a proper error message
      res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
      return addErrorMsg(res, keySlice.copyString());
    } else if (!s.IsNotFound()) {
      // IsBusy(), IsTimedOut() etc... this indicates a conflict
      return addErrorMsg(res.reset(rocksutils::convertStatus(s)), keySlice.copyString());
    }
  } else {
    // UPDATE/REPLACE case
    if (!s.ok()) {
      return addErrorMsg(res.reset(rocksutils::convertStatus(s)), keySlice.copyString());
    }
  }

  return res;
}

Result RocksDBPrimaryIndex::checkInsert(transaction::Methods& trx, 
                                        RocksDBMethods* mthd,
                                        LocalDocumentId const& documentId,
                                        velocypack::Slice slice,
                                        OperationOptions const& options) {
  VPackSlice keySlice;
  RevisionId revision;
  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice, revision);
  TRI_ASSERT(keySlice.isString());

  RocksDBKeyLeaser key(&trx);
  key->constructPrimaryIndexValue(objectId(), keySlice.stringView());

  return probeKey(trx, mthd, key, keySlice, options, /*insert*/ true);
}

Result RocksDBPrimaryIndex::checkReplace(transaction::Methods& trx, 
                                         RocksDBMethods* mthd,
                                         LocalDocumentId const& documentId,
                                         velocypack::Slice slice,
                                         OperationOptions const& options) {
  VPackSlice keySlice;
  RevisionId revision;
  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice, revision);
  TRI_ASSERT(keySlice.isString());

  RocksDBKeyLeaser key(&trx);
  key->constructPrimaryIndexValue(objectId(), keySlice.stringView());

  return probeKey(trx, mthd, key, keySlice, options, /*insert*/ false);
}

Result RocksDBPrimaryIndex::insert(transaction::Methods& trx, 
                                   RocksDBMethods* mthd,
                                   LocalDocumentId const& documentId,
                                   velocypack::Slice slice,
                                   OperationOptions const& options,
                                   bool performChecks) {
  VPackSlice keySlice;
  RevisionId revision;
  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice, revision);
  TRI_ASSERT(keySlice.isString());
  
  RocksDBKeyLeaser key(&trx);
  key->constructPrimaryIndexValue(objectId(), keySlice.stringView());
  
  Result res;

  if (performChecks) {
    res = probeKey(trx, mthd, key, keySlice, options, /*insert*/ true);

    if (res.fail()) {
      return res;
    }
  }

  if (trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // invalidate new index cache entry to avoid caching without committing first
    invalidateCacheEntry(key->string().data(), static_cast<uint32_t>(key->string().size()));
  }

  TRI_ASSERT(revision.isSet());
  auto value = RocksDBValue::PrimaryIndexValue(documentId, revision);
  rocksdb::Status s = mthd->Put(_cf, key.ref(), value.string(), /*assume_tracked*/ true);
  if (!s.ok()) {
    res.reset(rocksutils::convertStatus(s, rocksutils::index));
    addErrorMsg(res, keySlice.copyString());
  }
  return res;
}

Result RocksDBPrimaryIndex::update(transaction::Methods& trx, RocksDBMethods* mthd,
                                   LocalDocumentId const& oldDocumentId,
                                   velocypack::Slice oldDoc,
                                   LocalDocumentId const& newDocumentId,
                                   velocypack::Slice newDoc,
                                   OperationOptions const& /*options*/,
                                   bool /*performChecks*/) {
  Result res;
  VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(oldDoc);
  TRI_ASSERT(keySlice.binaryEquals(oldDoc.get(StaticStrings::KeyString)));
  RocksDBKeyLeaser key(&trx);

  key->constructPrimaryIndexValue(objectId(), keySlice.stringView());

  RevisionId revision = transaction::helpers::extractRevFromDocument(newDoc);
  auto value = RocksDBValue::PrimaryIndexValue(newDocumentId, revision);

  // invalidate new index cache entry to avoid caching without committing first
  invalidateCacheEntry(key->string().data(), static_cast<uint32_t>(key->string().size()));

  rocksdb::Status s = mthd->Put(_cf, key.ref(), value.string(), /*assume_tracked*/ false);
  if (!s.ok()) {
    res.reset(rocksutils::convertStatus(s, rocksutils::index));
    addErrorMsg(res, keySlice.copyString());
  }
  return res;
}

Result RocksDBPrimaryIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                                   LocalDocumentId const& documentId,
                                   velocypack::Slice slice) {
  Result res;

  // TODO: deal with matching revisions?
  VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);
  TRI_ASSERT(keySlice.isString());
  RocksDBKeyLeaser key(&trx);
  key->constructPrimaryIndexValue(objectId(), keySlice.stringView());

  invalidateCacheEntry(key->string().data(), static_cast<uint32_t>(key->string().size()));

  // acquire rocksdb transaction
  auto* mthds = RocksDBTransactionState::toMethods(&trx, _collection.id());
  rocksdb::Status s = mthds->Delete(_cf, key.ref());
  if (!s.ok()) {
    res.reset(rocksutils::convertStatus(s, rocksutils::index));
    addErrorMsg(res, keySlice.copyString());
  }
  return res;
}

/// @brief checks whether the index supports the condition
Index::FilterCosts RocksDBPrimaryIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsFilterCondition(allIndexes, this, node,
                                                              reference, itemsInIndex);
}

Index::SortCosts RocksDBPrimaryIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsSortCondition(this, sortCondition,
                                                            reference, itemsInIndex);
}

/// @brief creates an IndexIterator for the given Condition
std::unique_ptr<IndexIterator> RocksDBPrimaryIndex::iteratorForCondition(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  if (node == nullptr) {
    // full range scan
    if (opts.ascending) {
      // forward version
      return std::make_unique<RocksDBPrimaryIndexRangeIterator<false>>(
          &_collection /*logical collection*/, trx, this,
          RocksDBKeyBounds::PrimaryIndex(objectId(), ::lowest, ::highest),
          readOwnWrites);
    }
    // reverse version
    return std::make_unique<RocksDBPrimaryIndexRangeIterator<true>>(
        &_collection /*logical collection*/, trx, this,
        RocksDBKeyBounds::PrimaryIndex(objectId(), ::lowest, ::highest),
        readOwnWrites);
  }

  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  size_t const n = node->numMembers();
  TRI_ASSERT(n >= 1);

  if (n == 1) {
    AttributeAccessParts aap(node->getMember(0), reference);

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // a.b == value
      return createEqIterator(trx, aap.attribute, aap.value, readOwnWrites);
    }
    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN && aap.value->isArray()) {
      // "in"-checks never need to observe own writes.
      TRI_ASSERT(readOwnWrites == ReadOwnWrites::no);
      // a.b IN array
      return createInIterator(trx, aap.attribute, aap.value, opts.ascending);
    }
    // fall-through intentional here
  }

  auto removeCollectionFromString = [this, &trx](bool isId, std::string& value) -> int {
    if (isId) {
      char const* key = nullptr;
      size_t outLength = 0;
      std::shared_ptr<LogicalCollection> collection;
      Result res = trx->resolveId(value.data(), value.length(), collection, key, outLength);

      if (!res.ok()) {
        // using the name of an unknown collection
        if (_isRunningInCluster) {
          // translate from our own shard name to "real" collection name
          return value.compare(trx->resolver()->getCollectionName(_collection.id()));
        }
        return value.compare(_collection.name());
      }

      TRI_ASSERT(key);
      TRI_ASSERT(collection);

      if (!_isRunningInCluster && collection->id() != _collection.id()) {
        // using the name of a different collection...
        return value.compare(_collection.name());
      } else if (_isRunningInCluster && collection->planId() != _collection.planId()) {
        // using a different collection
        // translate from our own shard name to "real" collection name
        return value.compare(trx->resolver()->getCollectionName(_collection.id()));
      }

      // strip collection name prefix
      value = std::string(key, outLength);
    }

    // usage of _key or same collection name
    return 0;
  };

  std::string lower;
  std::string upper;
  bool lowerFound = false;
  bool upperFound = false;

  for (size_t i = 0; i < n; ++i) {
    AttributeAccessParts aap(node->getMemberUnchecked(i), reference);

    auto type = aap.opType;

    if (!(type == aql::NODE_TYPE_OPERATOR_BINARY_LE || type == aql::NODE_TYPE_OPERATOR_BINARY_LT ||
          type == aql::NODE_TYPE_OPERATOR_BINARY_GE || type == aql::NODE_TYPE_OPERATOR_BINARY_GT ||
          type == aql::NODE_TYPE_OPERATOR_BINARY_EQ)) {
      return std::make_unique<EmptyIndexIterator>(&_collection, trx);
    }

    TRI_ASSERT(aap.attribute->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);
    bool const isId = (aap.attribute->stringEquals(StaticStrings::IdString));

    std::string value;  // empty string == lower bound
    if (aap.value->isStringValue()) {
      value = aap.value->getString();
    } else if (aap.value->isObject() || aap.value->isArray()) {
      // any array or object value is bigger than any potential key
      value = ::highest;
    } else if (aap.value->isNullValue() || aap.value->isBoolValue() ||
               aap.value->isIntValue()) {
      // any null, bool or numeric value is lower than any potential key
      // keep lower bound
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     std::string(
                                         "unhandled type for valNode: ") +
                                         aap.value->getTypeString());
    }

    // strip collection name prefix from comparison value
    int const cmpResult = removeCollectionFromString(isId, value);

    if (type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      if (cmpResult != 0) {
        // doc._id == different collection
        return std::make_unique<EmptyIndexIterator>(&_collection, trx);
      }
      if (!upperFound || value < upper) {
        upper = value;
        upperFound = true;
      }
      if (!lowerFound || value < lower) {
        lower = std::move(value);
        lowerFound = true;
      }
    } else if (type == aql::NODE_TYPE_OPERATOR_BINARY_LE ||
               type == aql::NODE_TYPE_OPERATOR_BINARY_LT) {
      // a.b < value
      if (cmpResult > 0) {
        // doc._id < collection with "bigger" name
        upper = ::highest;
      } else if (cmpResult < 0) {
        // doc._id < collection with "lower" name
        return std::make_unique<EmptyIndexIterator>(&_collection, trx);
      } else {
        if (type == aql::NODE_TYPE_OPERATOR_BINARY_LT && !value.empty()) {
          // modify upper bound so that it is not included
          // primary keys are ASCII only, so we don't need to care about UTF-8 characters here
          if (value.back() >= static_cast<std::string::value_type>(0x02)) {
            value.back() -= 0x01;  
            value.append(::highest);
          }
        }
        if (!upperFound || value < upper) {
          upper = std::move(value);
        }
      }
      upperFound = true;
    } else if (type == aql::NODE_TYPE_OPERATOR_BINARY_GE ||
               type == aql::NODE_TYPE_OPERATOR_BINARY_GT) {
      // a.b > value
      if (cmpResult < 0) {
        // doc._id > collection with "smaller" name
        lower = ::lowest;
      } else if (cmpResult > 0) {
        // doc._id > collection with "bigger" name
        return std::make_unique<EmptyIndexIterator>(&_collection, trx);
      } else {
        if (type == aql::NODE_TYPE_OPERATOR_BINARY_GE && !value.empty()) {
          // modify lower bound so it is included in the results
          // primary keys are ASCII only, so we don't need to care about UTF-8 characters here
          if (value.back() >= static_cast<std::string::value_type>(0x02)) {
            value.back() -= 0x01;  
            value.append(::highest);
          }
        }
        if (!lowerFound || value > lower) {
          lower = std::move(value);
        }
      }
      lowerFound = true;
    }
  }  // for nodes

  // if only one bound is given select the other (lowest or highest) accordingly
  if (upperFound && !lowerFound) {
    lower = ::lowest;
    lowerFound = true;
  } else if (lowerFound && !upperFound) {
    upper = ::highest;
    upperFound = true;
  }

  if (lowerFound && upperFound) {
    if (opts.ascending) {
      // forward version
      return std::make_unique<RocksDBPrimaryIndexRangeIterator<false>>(
          &_collection /*logical collection*/, trx, this,
          RocksDBKeyBounds::PrimaryIndex(objectId(), lower, upper), readOwnWrites);
    }
    // reverse version
    return std::make_unique<RocksDBPrimaryIndexRangeIterator<true>>(
        &_collection /*logical collection*/, trx, this,
        RocksDBKeyBounds::PrimaryIndex(objectId(), lower, upper), readOwnWrites);
  }

  // operator type unsupported or IN used on non-array
  return std::make_unique<EmptyIndexIterator>(&_collection, trx);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBPrimaryIndex::specializeCondition(
    arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference) const {
  return SortedIndexAttributeMatcher::specializeCondition(this, node, reference);
}

/// @brief create the iterator, for a single attribute, IN operator
std::unique_ptr<IndexIterator> RocksDBPrimaryIndex::createInIterator(
    transaction::Methods* trx, arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode, bool ascending) {
  // _key or _id?
  bool const isId = (attrNode->stringEquals(StaticStrings::IdString));

  TRI_ASSERT(valNode->isArray());

  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());

  fillInLookupValues(trx, *(keys.get()), valNode, ascending, isId);
  return std::make_unique<RocksDBPrimaryIndexInIterator>(&_collection, trx, this,
                                                         std::move(keys), !isId);
}

/// @brief create the iterator, for a single attribute, EQ operator
std::unique_ptr<IndexIterator> RocksDBPrimaryIndex::createEqIterator(
    transaction::Methods* trx, arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode, ReadOwnWrites readOwnWrites) {
  // _key or _id?
  bool const isId = (attrNode->stringEquals(StaticStrings::IdString));

  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> key(builder.steal());

  // handle the sole element
  handleValNode(trx, key.get(), valNode, isId);

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!key->isEmpty()) {
    return std::make_unique<RocksDBPrimaryIndexEqIterator>(&_collection, trx, this,
                                                           std::move(key), !isId, readOwnWrites);
  }

  return std::make_unique<EmptyIndexIterator>(&_collection, trx);
}

void RocksDBPrimaryIndex::fillInLookupValues(transaction::Methods* trx, VPackBuilder& keys,
                                             arangodb::aql::AstNode const* values,
                                             bool ascending, bool isId) const {
  TRI_ASSERT(values != nullptr);
  TRI_ASSERT(values->type == arangodb::aql::NODE_TYPE_ARRAY);

  keys.clear();
  keys.openArray();

  size_t const n = values->numMembers();

  // only leave the valid elements
  if (ascending) {
    for (size_t i = 0; i < n; ++i) {
      handleValNode(trx, &keys, values->getMemberUnchecked(i), isId);
      TRI_IF_FAILURE("PrimaryIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
  } else {
    size_t i = n;
    while (i > 0) {
      --i;
      handleValNode(trx, &keys, values->getMemberUnchecked(i), isId);
      TRI_IF_FAILURE("PrimaryIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
  }

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  keys.close();
}

/// @brief add a single value node to the iterator's keys
void RocksDBPrimaryIndex::handleValNode(transaction::Methods* trx, VPackBuilder* keys,
                                        arangodb::aql::AstNode const* valNode,
                                        bool isId) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  if (isId) {
    // lookup by _id. now validate if the lookup is performed for the
    // correct collection (i.e. _collection)
    char const* key = nullptr;
    size_t outLength = 0;
    std::shared_ptr<LogicalCollection> collection;
    Result res = trx->resolveId(valNode->getStringValue(), valNode->getStringLength(),
                                collection, key, outLength);

    if (!res.ok()) {
      return;
    }

    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(key != nullptr);

    if (!_isRunningInCluster && collection->id() != _collection.id()) {
      // only continue lookup if the id value is syntactically correct and
      // refers to "our" collection, using local collection id
      return;
    }

    if (_isRunningInCluster) {
#ifdef USE_ENTERPRISE
      if (collection->isSmart() && collection->type() == TRI_COL_TYPE_EDGE) {
        auto c = dynamic_cast<VirtualClusterSmartEdgeCollection const*>(collection.get());
        if (c == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL, "unable to cast smart edge collection");
        }

        if (!c->isDisjoint() && (_collection.planId() != c->getLocalCid() &&
                                 _collection.planId() != c->getFromCid() &&
                                 _collection.planId() != c->getToCid())) {
          // invalid planId
          return;
        } else if (c->isDisjoint() && _collection.planId() != c->getLocalCid()) {
          // invalid planId
          return;
        }
      } else
#endif
          if (collection->planId() != _collection.planId()) {
        // only continue lookup if the id value is syntactically correct and
        // refers to "our" collection, using cluster collection id
        return;
      }
    }

    // use _key value from _id
    keys->add(VPackValuePair(key, outLength, VPackValueType::String));
  } else {
    keys->add(VPackValuePair(valNode->getStringValue(),
                             valNode->getStringLength(), VPackValueType::String));
  }
}
