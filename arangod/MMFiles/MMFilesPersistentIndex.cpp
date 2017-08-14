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
#include "Indexes/IndexLookupContext.h"
#include "Indexes/IndexResult.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "MMFiles/MMFilesPersistentIndexKeyComparator.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesToken.h"
#include "MMFiles/MMFilesTransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

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
    ManagedDocumentResult* mmdr, arangodb::MMFilesPersistentIndex const* index,
    arangodb::MMFilesPrimaryIndex* primaryIndex,
    rocksdb::OptimisticTransactionDB* db, bool reverse, VPackSlice const& left,
    VPackSlice const& right)
    : IndexIterator(collection, trx, mmdr, index),
      _primaryIndex(primaryIndex),
      _db(db),
      _reverse(reverse),
      _probe(false) {
  TRI_idx_iid_t const id = index->id();
  std::string const prefix = MMFilesPersistentIndex::buildPrefix(
      trx->vocbase()->id(), _primaryIndex->collection()->cid(), id);
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

  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "prefix: " <<
  // fasthash64(prefix.c_str(), prefix.size(), 0);
  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "iterator left key: " <<
  // left.toJson();
  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "iterator right key: " <<
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

bool MMFilesPersistentIndexIterator::next(TokenCallback const& cb,
                                          size_t limit) {
  auto comparator = MMFilesPersistentIndexFeature::instance()->comparator();
  while (limit > 0) {
    if (!_cursor->Valid()) {
      // We are exhausted already, sorry
      return false;
    }

    rocksdb::Slice key = _cursor->key();
    // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cursor key: " <<
    // VPackSlice(key.data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson();

    int res = comparator->Compare(
        key, rocksdb::Slice(_leftEndpoint->data(), _leftEndpoint->size()));
    // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "comparing: " <<
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
    // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "comparing: " <<
    // VPackSlice(key.data() + MMFilesPersistentIndex::keyPrefixSize()).toJson()
    // << " with " << VPackSlice((char const*) _rightEndpoint->data() +
    // MMFilesPersistentIndex::keyPrefixSize()).toJson() << " - res: " << res;

    if (res <= 0) {
      // get the value for _key, which is the last entry in the key array
      VPackSlice const keySlice = comparator->extractKeySlice(key);
      TRI_ASSERT(keySlice.isArray());
      VPackValueLength const n = keySlice.length();
      TRI_ASSERT(n > 1);  // one value + _key

      // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "looking up document with
      // key: " << keySlice.toJson();
      // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "looking up document with
      // primary key: " << keySlice[n - 1].toJson();

      // use primary index to lookup the document
      MMFilesSimpleIndexElement element =
          _primaryIndex->lookupKey(_trx, keySlice[n - 1]);
      if (element) {
        MMFilesToken doc = MMFilesToken{element.revisionId()};
        if (doc != 0) {
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

/// @brief create the index
MMFilesPersistentIndex::MMFilesPersistentIndex(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    arangodb::velocypack::Slice const& info)
    : MMFilesPathBasedIndex(iid, collection, info, sizeof(TRI_voc_rid_t),
                            true) {}

/// @brief destroy the index
MMFilesPersistentIndex::~MMFilesPersistentIndex() {}

size_t MMFilesPersistentIndex::memory() const {
  return 0;  // TODO
}

/// @brief inserts a document into the index
Result MMFilesPersistentIndex::insert(transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc, bool isRollback) {
  std::vector<MMFilesSkiplistIndexElement*> elements;

  int res;
  try {
    res = fillElement(elements, revisionId, doc);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // make sure we clean up before we leave this method
  auto cleanup = [this, &elements] {
    for (auto& it : elements) {
      _allocator->deallocate(it);
    }
  };

  TRI_DEFER(cleanup());

  if (res != TRI_ERROR_NO_ERROR) {
    return IndexResult(res, this);
  }

  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, numPaths());
  VPackSlice const key = transaction::helpers::extractKeyFromDocument(doc);
  std::string const prefix =
      buildPrefix(trx->vocbase()->id(), _collection->cid(), _iid);

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
      static_cast<MMFilesTransactionState*>(trx->state())->rocksTransaction();
  TRI_ASSERT(rocksTransaction != nullptr);

  auto comparator = MMFilesPersistentIndexFeature::instance()->comparator();
  rocksdb::ReadOptions readOptions;

  size_t const count = elements.size();
  for (size_t i = 0; i < count; ++i) {
    if (_unique) {
      bool uniqueConstraintViolated = false;
      auto iterator = rocksTransaction->GetIterator(readOptions);

      if (iterator != nullptr) {
        auto& bound = bounds[i];
        iterator->Seek(rocksdb::Slice(bound.first.c_str(), bound.first.size()));

        if (iterator->Valid()) {
          int res = comparator->Compare(
              iterator->key(),
              rocksdb::Slice(bound.second.c_str(), bound.second.size()));

          if (res <= 0) {
            uniqueConstraintViolated = true;
          }
        }

        delete iterator;
      }

      if (uniqueConstraintViolated) {
        // duplicate key
        res = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
        auto physical =
            static_cast<MMFilesCollection*>(_collection->getPhysical());
        TRI_ASSERT(physical != nullptr);
        if (!physical->useSecondaryIndexes()) {
          // suppress the error during recovery
          res = TRI_ERROR_NO_ERROR;
        }
      }
    }

    if (res == TRI_ERROR_NO_ERROR) {
      auto status = rocksTransaction->Put(values[i], std::string());

      if (!status.ok()) {
        res = TRI_ERROR_INTERNAL;
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      for (size_t j = 0; j < i; ++j) {
        rocksTransaction->Delete(values[i]);
      }

      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && !_unique) {
        // We ignore unique_constraint violated if we are not unique
        res = TRI_ERROR_NO_ERROR;
      }
      break;
    }
  }

  return IndexResult(res, this);
}

/// @brief removes a document from the index
Result MMFilesPersistentIndex::remove(transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc, bool isRollback) {
  std::vector<MMFilesSkiplistIndexElement*> elements;

  int res;
  try {
    res = fillElement(elements, revisionId, doc);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // make sure we clean up before we leave this method
  auto cleanup = [this, &elements] {
    for (auto& it : elements) {
      _allocator->deallocate(it);
    }
  };

  TRI_DEFER(cleanup());

  if (res != TRI_ERROR_NO_ERROR) {
    return IndexResult(res, this);
  }

  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, numPaths());
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
    value.append(buildPrefix(trx->vocbase()->id(), _collection->cid(), _iid));
    value.append(s.startAs<char const>(), s.byteSize());
    values.emplace_back(std::move(value));
  }

  auto rocksTransaction =
      static_cast<MMFilesTransactionState*>(trx->state())->rocksTransaction();
  TRI_ASSERT(rocksTransaction != nullptr);

  size_t const count = elements.size();

  for (size_t i = 0; i < count; ++i) {
    // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing key: " <<
    // VPackSlice(values[i].c_str() + keyPrefixSize()).toJson();
    auto status = rocksTransaction->Delete(values[i]);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (!status.ok()) {
      res = TRI_ERROR_INTERNAL;
    }
  }

  return IndexResult(res, this);
}

/// @brief called when the index is dropped
int MMFilesPersistentIndex::drop() {
  return MMFilesPersistentIndexFeature::instance()->dropIndex(
      _collection->vocbase()->id(), _collection->cid(), _iid);
}

/// @brief attempts to locate an entry in the index
/// Warning: who ever calls this function is responsible for destroying
/// the MMFilesPersistentIndexIterator* results
MMFilesPersistentIndexIterator* MMFilesPersistentIndex::lookup(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
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
  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  auto idx = physical->primaryIndex();
  auto db = MMFilesPersistentIndexFeature::instance()->db();
  return new MMFilesPersistentIndexIterator(
      _collection, trx, mmdr, this, idx, db, reverse, leftBorder, rightBorder);
}

bool MMFilesPersistentIndex::accessFitsIndex(
    arangodb::aql::AstNode const* access, arangodb::aql::AstNode const* other,
    arangodb::aql::AstNode const* op, arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    std::unordered_set<std::string>& nonNullAttributes,
    bool isExecution) const {
  if (!canUseConditionPart(access, other, op, reference, nonNullAttributes,
                           isExecution)) {
    return false;
  }

  arangodb::aql::AstNode const* what = access;
  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>>
      attributeData;

  if (op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    if (!what->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }
    if (arangodb::basics::TRI_AttributeNamesHaveExpansion(
            attributeData.second)) {
      // doc.value[*] == 'value'
      return false;
    }
    if (isAttributeExpanded(attributeData.second)) {
      // doc.value == 'value' (with an array index)
      return false;
    }
  } else {
    // ok, we do have an IN here... check if it's something like 'value' IN
    // doc.value[*]
    TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);
    bool canUse = false;

    if (what->isAttributeAccessForVariable(attributeData) &&
        attributeData.first == reference &&
        !arangodb::basics::TRI_AttributeNamesHaveExpansion(
            attributeData.second) &&
        attributeMatches(attributeData.second)) {
      // doc.value IN 'value'
      // can use this index
      canUse = true;
    } else {
      // check for  'value' IN doc.value  AND  'value' IN doc.value[*]
      what = other;
      if (what->isAttributeAccessForVariable(attributeData) &&
          attributeData.first == reference &&
          isAttributeExpanded(attributeData.second) &&
          attributeMatches(attributeData.second)) {
        canUse = true;
      }
    }

    if (!canUse) {
      return false;
    }
  }

  std::vector<arangodb::basics::AttributeName> const& fieldNames =
      attributeData.second;

  for (size_t i = 0; i < _fields.size(); ++i) {
    if (_fields[i].size() != fieldNames.size()) {
      // attribute path length differs
      continue;
    }

    if (this->isAttributeExpanded(i) &&
        op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // If this attribute is correct or not, it could only serve for IN
      continue;
    }

    bool match = arangodb::basics::AttributeName::isIdentical(_fields[i],
                                                              fieldNames, true);

    if (match) {
      // mark ith attribute as being covered
      auto it = found.find(i);

      if (it == found.end()) {
        found.emplace(i, std::vector<arangodb::aql::AstNode const*>{op});
      } else {
        (*it).second.emplace_back(op);
      }
      TRI_IF_FAILURE("PersistentIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      return true;
    }
  }

  return false;
}

void MMFilesPersistentIndex::matchAttributes(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    size_t& values, std::unordered_set<std::string>& nonNullAttributes,
    bool isExecution) const {
  for (size_t i = 0; i < node->numMembers(); ++i) {
    auto op = node->getMember(i);

    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
        TRI_ASSERT(op->numMembers() == 2);
        accessFitsIndex(op->getMember(0), op->getMember(1), op, reference,
                        found, nonNullAttributes, isExecution);
        accessFitsIndex(op->getMember(1), op->getMember(0), op, reference,
                        found, nonNullAttributes, isExecution);
        break;

      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
        if (accessFitsIndex(op->getMember(0), op->getMember(1), op, reference,
                            found, nonNullAttributes, isExecution)) {
          auto m = op->getMember(1);
          if (m->isArray() && m->numMembers() > 1) {
            // attr IN [ a, b, c ]  =>  this will produce multiple items, so
            // count them!
            values += m->numMembers() - 1;
          }
        }
        break;

      default:
        break;
    }
  }
}

bool MMFilesPersistentIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  std::unordered_set<std::string> nonNullAttributes;
  size_t values = 0;
  matchAttributes(node, reference, found, values, nonNullAttributes, false);

  bool lastContainsEquality = true;
  size_t attributesCovered = 0;
  size_t attributesCoveredByEquality = 0;
  double equalityReductionFactor = 20.0;
  estimatedCost = static_cast<double>(itemsInIndex);

  for (size_t i = 0; i < _fields.size(); ++i) {
    auto it = found.find(i);

    if (it == found.end()) {
      // index attribute not covered by condition
      break;
    }

    // check if the current condition contains an equality condition
    auto const& nodes = (*it).second;
    bool containsEquality = false;
    for (size_t j = 0; j < nodes.size(); ++j) {
      if (nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
          nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        containsEquality = true;
        break;
      }
    }

    if (!lastContainsEquality) {
      // unsupported condition. must abort
      break;
    }

    ++attributesCovered;
    if (containsEquality) {
      ++attributesCoveredByEquality;
      estimatedCost /= equalityReductionFactor;

      // decrease the effect of the equality reduction factor
      equalityReductionFactor *= 0.25;
      if (equalityReductionFactor < 2.0) {
        // equalityReductionFactor shouldn't get too low
        equalityReductionFactor = 2.0;
      }
    } else {
      // quick estimate for the potential reductions caused by the conditions
      if (nodes.size() >= 2) {
        // at least two (non-equality) conditions. probably a range with lower
        // and upper bound defined
        estimatedCost /= 7.5;
      } else {
        // one (non-equality). this is either a lower or a higher bound
        estimatedCost /= 2.0;
      }
    }

    lastContainsEquality = containsEquality;
  }

  if (values == 0) {
    values = 1;
  }

  if (attributesCoveredByEquality == _fields.size() && unique()) {
    // index is unique and condition covers all attributes by equality
    if (estimatedItems >= values) {
      // reduce costs due to uniqueness
      estimatedItems = values;
      estimatedCost = static_cast<double>(estimatedItems);
    } else {
      // cost is already low... now slightly prioritize the unique index
      estimatedCost *= 0.995;
    }
    return true;
  }

  if (attributesCovered > 0 &&
      (!_sparse || attributesCovered == _fields.size())) {
    // if the condition contains at least one index attribute and is not sparse,
    // or the index is sparse and all attributes are covered by the condition,
    // then it can be used (note: additional checks for condition parts in
    // sparse indexes are contained in Index::canUseConditionPart)
    estimatedItems = static_cast<size_t>((std::max)(
        static_cast<size_t>(estimatedCost * values), static_cast<size_t>(1)));
    estimatedCost *= static_cast<double>(values);
    return true;
  }

  // no condition
  estimatedItems = itemsInIndex;
  estimatedCost = static_cast<double>(estimatedItems);
  return false;
}

bool MMFilesPersistentIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost, size_t& coveredAttributes) const {
  TRI_ASSERT(sortCondition != nullptr);

  if (!_sparse) {
    // only non-sparse indexes can be used for sorting
    if (!_useExpansion && sortCondition->isUnidirectional() &&
        sortCondition->isOnlyAttributeAccess()) {
      coveredAttributes = sortCondition->coveredAttributes(reference, _fields);

      if (coveredAttributes >= sortCondition->numAttributes()) {
        // sort is fully covered by index. no additional sort costs!
        // forward iteration does not have high costs
        estimatedCost = itemsInIndex * 0.001;
        if (sortCondition->isDescending()) {
          // reverse iteration has higher costs than forward iteration
          estimatedCost *= 4;
        }
        return true;
      } else if (coveredAttributes > 0) {
        estimatedCost = (itemsInIndex / coveredAttributes) *
                        std::log2(static_cast<double>(itemsInIndex));
        if (sortCondition->isAscending()) {
          // reverse iteration is more expensive
          estimatedCost *= 4;
        }
        return true;
      }
    }
  }

  coveredAttributes = 0;
  // by default no sort conditions are supported
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(static_cast<double>(itemsInIndex));
    // slightly penalize this type of index against other indexes which
    // are in memory
    estimatedCost *= 1.05;
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

IndexIterator* MMFilesPersistentIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) {
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
    matchAttributes(node, reference, found, unused, nonNullAttributes, true);

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
      for (auto const& val : VPackArrayIterator(expandedSlice)) {
        auto iterator = lookup(trx, mmdr, val, reverse);
        try {
          iterators.push_back(iterator);
        } catch (...) {
          // avoid leak
          delete iterator;
          throw;
        }
      }
      if (reverse) {
        std::reverse(iterators.begin(), iterators.end());
      }
    } catch (...) {
      for (auto& it : iterators) {
        delete it;
      }
      throw;
    }
    return new MultiIndexIterator(_collection, trx, mmdr, this, iterators);
  }

  VPackSlice searchSlice = searchValues.slice();
  TRI_ASSERT(searchSlice.length() == 1);
  searchSlice = searchSlice.at(0);
  return lookup(trx, mmdr, searchSlice, reverse);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* MMFilesPersistentIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  std::unordered_set<std::string> nonNullAttributes;
  size_t values = 0;
  matchAttributes(node, reference, found, values, nonNullAttributes, false);

  std::vector<arangodb::aql::AstNode const*> children;
  bool lastContainsEquality = true;

  for (size_t i = 0; i < _fields.size(); ++i) {
    auto it = found.find(i);

    if (it == found.end()) {
      // index attribute not covered by condition
      break;
    }

    // check if the current condition contains an equality condition
    auto& nodes = (*it).second;
    bool containsEquality = false;
    for (size_t j = 0; j < nodes.size(); ++j) {
      if (nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
          nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        containsEquality = true;
        break;
      }
    }

    if (!lastContainsEquality) {
      // unsupported condition. must abort
      break;
    }

    std::sort(nodes.begin(), nodes.end(),
              [](arangodb::aql::AstNode const* lhs,
                 arangodb::aql::AstNode const* rhs) -> bool {
                return sortWeight(lhs) < sortWeight(rhs);
              });

    lastContainsEquality = containsEquality;
    std::unordered_set<int> operatorsFound;
    for (auto& it : nodes) {
      // do not let duplicate or related operators pass
      if (isDuplicateOperator(it, operatorsFound)) {
        continue;
      }
      operatorsFound.emplace(static_cast<int>(it->type));
      children.emplace_back(it);
    }
  }

  while (node->numMembers() > 0) {
    node->removeMemberUnchecked(0);
  }

  for (auto& it : children) {
    node->addMember(it);
  }
  return node;
}

bool MMFilesPersistentIndex::isDuplicateOperator(
    arangodb::aql::AstNode const* node,
    std::unordered_set<int> const& operatorsFound) const {
  auto type = node->type;
  if (operatorsFound.find(static_cast<int>(type)) != operatorsFound.end()) {
    // duplicate operator
    return true;
  }

  if (operatorsFound.find(
          static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
          operatorsFound.end() ||
      operatorsFound.find(
          static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
          operatorsFound.end()) {
    return true;
  }

  bool duplicate = false;
  switch (type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
      duplicate = operatorsFound.find(static_cast<int>(
                      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
                  operatorsFound.end();
      break;
    default: {
      // ignore
    }
  }

  return duplicate;
}
