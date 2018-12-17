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
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesPersistentIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/FixedSizeAllocator.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "Indexes/PersistentIndexAttributeMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "MMFiles/MMFilesPersistentIndexKeyComparator.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesTransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

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

MMFilesPersistentIndexIterator::MMFilesPersistentIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    arangodb::MMFilesPersistentIndex const* index,
    arangodb::MMFilesPrimaryIndex* primaryIndex,
    rocksdb::OptimisticTransactionDB* db, bool reverse, VPackSlice const& left,
    VPackSlice const& right)
    : IndexIterator(collection, trx),
      _primaryIndex(primaryIndex),
      _db(db),
      _reverse(reverse),
      _probe(false) {
  TRI_idx_iid_t const id = index->id();
  std::string const prefix = MMFilesPersistentIndex::buildPrefix(
    trx->vocbase().id(), _primaryIndex->collection().id(), id
  );

  TRI_ASSERT(prefix.size() == MMFilesPersistentIndex::keyPrefixSize());

  _leftEndpoint.reset(new arangodb::velocypack::Buffer<char>());
  _leftEndpoint->reserve(MMFilesPersistentIndex::keyPrefixSize() +
                         left.byteSize());
  _leftEndpoint->append(prefix.c_str(), prefix.size());
  _leftEndpoint->append(left.startAs<char const>(), left.byteSize());

  _rightEndpoint.reset(new arangodb::velocypack::Buffer<char>());
  _rightEndpoint->reserve(MMFilesPersistentIndex::keyPrefixSize() +
                          right.byteSize());
  _rightEndpoint->append(prefix.c_str(), prefix.size());
  _rightEndpoint->append(right.startAs<char const>(), right.byteSize());

  TRI_ASSERT(_leftEndpoint->size() > 8);
  TRI_ASSERT(_rightEndpoint->size() > 8);

  // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "prefix: " <<
  // fasthash64(prefix.c_str(), prefix.size(), 0);
  // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "iterator left key: " <<
  // left.toJson();
  // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "iterator right key: " <<
  // right.toJson();

  _cursor.reset(_db->GetBaseDB()->NewIterator(rocksdb::ReadOptions()));

  reset();
}

/// @brief Reset the cursor
void MMFilesPersistentIndexIterator::reset() {
  if (_reverse) {
    _probe = true;
    _cursor->Seek(
        rocksdb::Slice(_rightEndpoint->data(), _rightEndpoint->size()));
    if (!_cursor->Valid()) {
      _cursor->SeekToLast();
    }
  } else {
    _cursor->Seek(rocksdb::Slice(_leftEndpoint->data(), _leftEndpoint->size()));
  }
}

bool MMFilesPersistentIndexIterator::next(LocalDocumentIdCallback const& cb,
                                          size_t limit) {
  auto comparator = MMFilesPersistentIndexFeature::instance()->comparator();
  while (limit > 0) {
    if (!_cursor->Valid()) {
      // We are exhausted already, sorry
      return false;
    }

    rocksdb::Slice key = _cursor->key();
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "cursor key: " <<
    // VPackSlice(key.data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson();

    int res = comparator->Compare(
        key, rocksdb::Slice(_leftEndpoint->data(), _leftEndpoint->size()));
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "comparing: " <<
    // VPackSlice(key.data() + MMFilesPersistentIndex::keyPrefixSize()).toJson()
    // << " with " << VPackSlice((char const*) _leftEndpoint->data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson() << " - res: " << res;

    if (res < 0) {
      if (_reverse) {
        // We are done
        return false;
      } else {
        _cursor->Next();
      }
      continue;
    }

    res = comparator->Compare(
        key, rocksdb::Slice(_rightEndpoint->data(), _rightEndpoint->size()));
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "comparing: " <<
    // VPackSlice(key.data() + MMFilesPersistentIndex::keyPrefixSize()).toJson()
    // << " with " << VPackSlice((char const*) _rightEndpoint->data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson() << " - res: " << res;

    if (res <= 0) {
      // get the value for _key, which is the last entry in the key array
      VPackSlice const keySlice = comparator->extractKeySlice(key);
      TRI_ASSERT(keySlice.isArray());
      VPackValueLength const n = keySlice.length();
      TRI_ASSERT(n > 1);  // one value + _key

      // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "looking up document with
      // key: " << keySlice.toJson();
      // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "looking up document with
      // primary key: " << keySlice[n - 1].toJson();

      // use primary index to lookup the document
      MMFilesSimpleIndexElement element =
          _primaryIndex->lookupKey(_trx, keySlice[n - 1]);
      if (element) {
        LocalDocumentId doc = element.localDocumentId();
        if (doc.isSet()) {
          cb(doc);
          --limit;
        }
      }
    }

    if (_reverse) {
      _cursor->Prev();
    } else {
      _cursor->Next();
    }

    if (res > 0) {
      if (!_probe) {
        return false;
      }
      _probe = false;
    }
  }
  return true;
}

bool MMFilesPersistentIndexIterator::nextDocument(DocumentCallback const& cb,
                                                  size_t limit) {
  _documentIds.clear();
  _documentIds.reserve(limit);

  auto comparator = MMFilesPersistentIndexFeature::instance()->comparator();
  bool done = false;
  while (limit > 0) {
    if (!_cursor->Valid()) {
      // We are exhausted already, sorry
      done = true;
      break;
    }

    rocksdb::Slice key = _cursor->key();
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "cursor key: " <<
    // VPackSlice(key.data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson();

    int res = comparator->Compare(
        key, rocksdb::Slice(_leftEndpoint->data(), _leftEndpoint->size()));
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "comparing: " <<
    // VPackSlice(key.data() + MMFilesPersistentIndex::keyPrefixSize()).toJson()
    // << " with " << VPackSlice((char const*) _leftEndpoint->data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson() << " - res: " << res;

    if (res < 0) {
      if (_reverse) {
        // We are done
        done = true;
        break;
      } else {
        _cursor->Next();
      }
      continue;
    }

    res = comparator->Compare(
        key, rocksdb::Slice(_rightEndpoint->data(), _rightEndpoint->size()));
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "comparing: " <<
    // VPackSlice(key.data() + MMFilesPersistentIndex::keyPrefixSize()).toJson()
    // << " with " << VPackSlice((char const*) _rightEndpoint->data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson() << " - res: " << res;

    if (res <= 0) {
      // get the value for _key, which is the last entry in the key array
      VPackSlice const keySlice = comparator->extractKeySlice(key);
      TRI_ASSERT(keySlice.isArray());
      VPackValueLength const n = keySlice.length();
      TRI_ASSERT(n > 1);  // one value + _key

      // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "looking up document with
      // key: " << keySlice.toJson();
      // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "looking up document with
      // primary key: " << keySlice[n - 1].toJson();

      // use primary index to lookup the document
      MMFilesSimpleIndexElement element =
          _primaryIndex->lookupKey(_trx, keySlice[n - 1]);
      if (element) {
        LocalDocumentId doc = element.localDocumentId();
        if (doc.isSet()) {
          _documentIds.emplace_back(doc, nullptr);
          --limit;
        }
      }
    }

    if (_reverse) {
      _cursor->Prev();
    } else {
      _cursor->Next();
    }

    if (res > 0) {
      if (!_probe) {
        done = true;
        break;
      }
      _probe = false;
    }
  }

  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  physical->readDocumentWithCallback(_trx, _documentIds, cb);
  return !done;
}

/// @brief create the index
MMFilesPersistentIndex::MMFilesPersistentIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
)
    : MMFilesPathBasedIndex(iid, collection, info, sizeof(LocalDocumentId),
                            true) {}

/// @brief destroy the index
MMFilesPersistentIndex::~MMFilesPersistentIndex() {}

size_t MMFilesPersistentIndex::memory() const {
  return 0;  // TODO
}

/// @brief inserts a document into the index
Result MMFilesPersistentIndex::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  std::vector<MMFilesSkiplistIndexElement*> elements;
  Result res;
  int r;

  try {
    r = fillElement(elements, documentId, doc);
  } catch (basics::Exception const& ex) {
    r = ex.code();
  } catch (std::bad_alloc const&) {
    r = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    r = TRI_ERROR_INTERNAL;
  }

  // make sure we clean up before we leave this method
  auto cleanup = [this, &elements] {
    for (auto& it : elements) {
      _allocator->deallocate(it);
    }
  };

  TRI_DEFER(cleanup());

  if (r != TRI_ERROR_NO_ERROR) {
    return addErrorMsg(res, r);
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, numPaths());
  VPackSlice const key = transaction::helpers::extractKeyFromDocument(doc);
  auto prefix = buildPrefix(trx.vocbase().id(), _collection.id(), _iid);
  VPackBuilder builder;
  std::vector<std::string> values;

  values.reserve(elements.size());

  // lower and upper bounds, only required if the index is unique
  std::vector<std::pair<std::string, std::string>> bounds;

  if (_unique) {
    bounds.reserve(elements.size());
  }

  for (auto const& it : elements) {
    builder.clear();
    builder.openArray();

    for (size_t i = 0; i < _fields.size(); ++i) {
      builder.add(it->slice(&context, i));
    }

    builder.add(key);  // always append _key value to the end of the array
    builder.close();

    VPackSlice const s = builder.slice();
    std::string value;
    value.reserve(keyPrefixSize() + s.byteSize());
    value += prefix;
    value.append(s.startAs<char const>(), s.byteSize());
    values.emplace_back(std::move(value));

    if (_unique) {
      builder.clear();
      builder.openArray();
      for (size_t i = 0; i < _fields.size(); ++i) {
        builder.add(it->slice(&context, i));
      }
      builder.add(VPackSlice::minKeySlice());
      builder.close();

      VPackSlice s = builder.slice();
      std::string value;
      value.reserve(keyPrefixSize() + s.byteSize());
      value += prefix;
      value.append(s.startAs<char const>(), s.byteSize());

      std::pair<std::string, std::string> p;
      p.first = value;

      builder.clear();
      builder.openArray();
      for (size_t i = 0; i < _fields.size(); ++i) {
        builder.add(it->slice(&context, i));
      }
      builder.add(VPackSlice::maxKeySlice());
      builder.close();

      s = builder.slice();
      value.clear();
      value += prefix;
      value.append(s.startAs<char const>(), s.byteSize());

      p.second = value;
      bounds.emplace_back(std::move(p));
    }
  }

  auto rocksTransaction =
    static_cast<MMFilesTransactionState*>(trx.state())->rocksTransaction();
  TRI_ASSERT(rocksTransaction != nullptr);

  auto comparator = MMFilesPersistentIndexFeature::instance()->comparator();
  rocksdb::ReadOptions readOptions;

  size_t const count = elements.size();
  std::string existingId;
  for (size_t i = 0; i < count; ++i) {
    if (_unique) {
      bool uniqueConstraintViolated = false;
      auto iterator = rocksTransaction->GetIterator(readOptions);

      if (iterator != nullptr) {
        auto& bound = bounds[i];
        iterator->Seek(rocksdb::Slice(bound.first.c_str(), bound.first.size()));

        if (iterator->Valid()) {
          int cmp = comparator->Compare(
              iterator->key(),
              rocksdb::Slice(bound.second.c_str(), bound.second.size()));

          if (cmp <= 0) {
            uniqueConstraintViolated = true;
            VPackSlice slice(comparator->extractKeySlice(iterator->key()));
            uint64_t length = slice.length();
            TRI_ASSERT(length > 0);
            existingId = slice.at(length - 1).copyString();
          }
        }

        delete iterator;
      }

      if (uniqueConstraintViolated) {
        // duplicate key
        r = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
        auto physical =
            static_cast<MMFilesCollection*>(_collection.getPhysical());
        TRI_ASSERT(physical != nullptr);

        if (!physical->useSecondaryIndexes()) {
          // suppress the error during recovery
          r = TRI_ERROR_NO_ERROR;
        }
      }
    }

    if (r == TRI_ERROR_NO_ERROR) {
      auto status = rocksTransaction->Put(values[i], std::string());

      if (!status.ok()) {
        r = TRI_ERROR_INTERNAL;
      }
    }

    if (r != TRI_ERROR_NO_ERROR) {
      for (size_t j = 0; j < i; ++j) {
        rocksTransaction->Delete(values[i]);
      }

      if (r == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && !_unique) {
        // We ignore unique_constraint violated if we are not unique
        r = TRI_ERROR_NO_ERROR;
      }
      break;
    }
  }

  if (r == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    if (mode == OperationMode::internal) {
      return res.reset(r, existingId);
    }
    return addErrorMsg(res, r, existingId);
  }

  return res;
}

/// @brief removes a document from the index
Result MMFilesPersistentIndex::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  std::vector<MMFilesSkiplistIndexElement*> elements;
  Result res;
  int r;

  try {
    r = fillElement(elements, documentId, doc);
  } catch (basics::Exception const& ex) {
    r = ex.code();
  } catch (std::bad_alloc const&) {
    r = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    r = TRI_ERROR_INTERNAL;
  }

  // make sure we clean up before we leave this method
  auto cleanup = [this, &elements] {
    for (auto& it : elements) {
      _allocator->deallocate(it);
    }
  };

  TRI_DEFER(cleanup());

  if (r != TRI_ERROR_NO_ERROR) {
    return addErrorMsg(res, r);
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, numPaths());
  VPackSlice const key = transaction::helpers::extractKeyFromDocument(doc);
  VPackBuilder builder;
  std::vector<std::string> values;

  for (auto const& it : elements) {
    builder.clear();
    builder.openArray();

    for (size_t i = 0; i < _fields.size(); ++i) {
      builder.add(it->slice(&context, i));
    }

    builder.add(key);  // always append _key value to the end of the array
    builder.close();

    VPackSlice const s = builder.slice();
    std::string value;

    value.reserve(keyPrefixSize() + s.byteSize());
    value.append(buildPrefix(trx.vocbase().id(), _collection.id(), _iid));
    value.append(s.startAs<char const>(), s.byteSize());
    values.emplace_back(std::move(value));
  }

  auto rocksTransaction =
    static_cast<MMFilesTransactionState*>(trx.state())->rocksTransaction();
  TRI_ASSERT(rocksTransaction != nullptr);

  size_t const count = elements.size();

  for (size_t i = 0; i < count; ++i) {
    // LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "removing key: " <<
    // VPackSlice(values[i].c_str() + keyPrefixSize()).toJson();
    auto status = rocksTransaction->Delete(values[i]);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (!status.ok()) {
      addErrorMsg(res, TRI_ERROR_INTERNAL);
    }
  }

  return res;
}

/// @brief called when the index is dropped
Result MMFilesPersistentIndex::drop() {
  return MMFilesPersistentIndexFeature::instance()->dropIndex(
    _collection.vocbase().id(), _collection.id(), _iid
  );
}

/// @brief attempts to locate an entry in the index
/// Warning: who ever calls this function is responsible for destroying
/// the MMFilesPersistentIndexIterator* results
MMFilesPersistentIndexIterator* MMFilesPersistentIndex::lookup(
    transaction::Methods* trx,
    VPackSlice const searchValues, bool reverse) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(searchValues.length() <= _fields.size());

  VPackBuilder leftSearch;
  VPackBuilder rightSearch;

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

  VPackSlice leftBorder;
  VPackSlice rightBorder;

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

  // Secured by trx. The shared_ptr index stays valid in
  // _collection at least as long as trx is running.
  // Same for the iterator
  auto physical = static_cast<MMFilesCollection*>(_collection.getPhysical());
  auto idx = physical->primaryIndex();
  auto db = MMFilesPersistentIndexFeature::instance()->db();
  return new MMFilesPersistentIndexIterator(
    &_collection, trx, this, idx, db, reverse, leftBorder, rightBorder
  );
}

bool MMFilesPersistentIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  return PersistentIndexAttributeMatcher::supportsFilterCondition(allIndexes, this, node, reference,
                                                                  itemsInIndex, estimatedItems, estimatedCost);
}

bool MMFilesPersistentIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost, size_t& coveredAttributes) const {
  return PersistentIndexAttributeMatcher::supportsSortCondition(this, sortCondition, reference, itemsInIndex,
                                                                estimatedCost, coveredAttributes);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* MMFilesPersistentIndex::specializeCondition(arangodb::aql::AstNode* node,
                                                                    arangodb::aql::Variable const* reference) const {
  return PersistentIndexAttributeMatcher::specializeCondition(this, node, reference);
}

IndexIterator* MMFilesPersistentIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult*,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
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
  } else {
    // Create the search Values for the lookup
    VPackArrayBuilder guard(&searchValues);

    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>
        found;
    std::unordered_set<std::string> nonNullAttributes;
    size_t unused = 0;
    PersistentIndexAttributeMatcher::matchAttributes(this, node, reference, found, unused,
                                                     nonNullAttributes, true);

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
      } else if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        if (isAttributeExpanded(usedFields)) {
          searchValues.openObject();
          searchValues.add(VPackValue(StaticStrings::IndexEq));
          TRI_IF_FAILURE("PersistentIndex::permutationArrayIN") {
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
              return nullptr;
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

  if (needNormalize) {
    VPackBuilder expandedSearchValues;
    expandInSearchValues(searchValues.slice(), expandedSearchValues);
    VPackSlice expandedSlice = expandedSearchValues.slice();
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
