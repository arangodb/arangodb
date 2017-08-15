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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesPrimaryIndex.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"
#include "Indexes/IndexLookupContext.h"
#include "Indexes/IndexResult.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesToken.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const
    IndexAttributes{{arangodb::basics::AttributeName("_id", false)},
                    {arangodb::basics::AttributeName("_key", false)}};

static inline uint64_t HashKey(void*, uint8_t const* key) {
  return MMFilesSimpleIndexElement::hash(VPackSlice(key));
}

static inline uint64_t HashElement(void*,
                                   MMFilesSimpleIndexElement const& element) {
  return element.hash();
}

/// @brief determines if a key corresponds to an element
static bool IsEqualKeyElement(void* userData, uint8_t const* key, uint64_t hash,
                              MMFilesSimpleIndexElement const& right) {
  IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  TRI_ASSERT(context != nullptr);

  try {
    VPackSlice tmp = right.slice(context);
    TRI_ASSERT(tmp.isString());
    return VPackSlice(key).equals(tmp);
  } catch (...) {
    return false;
  }
}

/// @brief determines if two elements are equal
static bool IsEqualElementElement(void* userData,
                                  MMFilesSimpleIndexElement const& left,
                                  MMFilesSimpleIndexElement const& right) {
  IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  TRI_ASSERT(context != nullptr);

  VPackSlice l = left.slice(context);
  VPackSlice r = right.slice(context);
  TRI_ASSERT(l.isString());
  TRI_ASSERT(r.isString());
  return l.equals(r);
}

MMFilesPrimaryIndexIterator::MMFilesPrimaryIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, MMFilesPrimaryIndex const* index,
    std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index),
      _keys(keys.get()),
      _iterator(_keys->slice()) {
  keys.release();  // now we have ownership for _keys
  TRI_ASSERT(_keys->slice().isArray());
}

MMFilesPrimaryIndexIterator::~MMFilesPrimaryIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool MMFilesPrimaryIndexIterator::next(TokenCallback const& cb, size_t limit) {
  TRI_ASSERT(limit > 0);
  if (!_iterator.valid() || limit == 0) {
    return false;
  }
  while (_iterator.valid() && limit > 0) {
    MMFilesSimpleIndexElement result =
        _index->lookupKey(_trx, _iterator.value());
    _iterator.next();
    if (result) {
      cb(MMFilesToken{result.revisionId()});
      --limit;
    }
  }
  return _iterator.valid();
}

void MMFilesPrimaryIndexIterator::reset() { _iterator.reset(); }

MMFilesAllIndexIterator::MMFilesAllIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, MMFilesPrimaryIndex const* index,
    MMFilesPrimaryIndexImpl const* indexImpl, bool reverse)
    : IndexIterator(collection, trx, mmdr, index),
      _index(indexImpl),
      _reverse(reverse),
      _total(0) {}

bool MMFilesAllIndexIterator::next(TokenCallback const& cb, size_t limit) {
  while (limit > 0) {
    MMFilesSimpleIndexElement element;
    if (_reverse) {
      element = _index->findSequentialReverse(&_context, _position);
    } else {
      element = _index->findSequential(&_context, _position, _total);
    }
    if (element) {
      cb(MMFilesToken{element.revisionId()});
      --limit;
    } else {
      return false;
    }
  }
  return true;
}

// Skip the first count-many entries
void MMFilesAllIndexIterator::skip(uint64_t count, uint64_t& skipped) {
  while (count > 0) {
    MMFilesSimpleIndexElement element;
    if (_reverse) {
      element = _index->findSequentialReverse(&_context, _position);
    } else {
      element = _index->findSequential(&_context, _position, _total);
    }
    if (element) {
      ++skipped;
      --count;
    } else {
      break;
    }
  }
}

void MMFilesAllIndexIterator::reset() { _position.reset(); }

MMFilesAnyIndexIterator::MMFilesAnyIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, MMFilesPrimaryIndex const* index,
    MMFilesPrimaryIndexImpl const* indexImpl)
    : IndexIterator(collection, trx, mmdr, index),
      _index(indexImpl),
      _step(0),
      _total(0) {}

bool MMFilesAnyIndexIterator::next(TokenCallback const& cb, size_t limit) {
  while (limit > 0) {
    MMFilesSimpleIndexElement element =
        _index->findRandom(&_context, _initial, _position, _step, _total);
    if (element) {
      cb(MMFilesToken{element.revisionId()});
      --limit;
    } else {
      return false;
    }
  }
  return true;
}

void MMFilesAnyIndexIterator::reset() {
  _step = 0;
  _total = 0;
  _position = _initial;
}

MMFilesPrimaryIndex::MMFilesPrimaryIndex(
    arangodb::LogicalCollection* collection)
    : Index(0, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::KeyString,
                                                  false)}}),
            /*unique*/ true , /*sparse*/ false),
      _primaryIndex(nullptr) {
  size_t indexBuckets = 1;

  if (collection != nullptr) {
    // collection is a nullptr in the coordinator case
    auto physical =
        static_cast<arangodb::MMFilesCollection*>(collection->getPhysical());
    TRI_ASSERT(physical != nullptr);
    indexBuckets = static_cast<size_t>(physical->indexBuckets());
  }

  _primaryIndex = new MMFilesPrimaryIndexImpl(
      HashKey, HashElement, IsEqualKeyElement, IsEqualElementElement,
      IsEqualElementElement, indexBuckets,
      [this]() -> std::string { return this->context(); });
}

MMFilesPrimaryIndex::~MMFilesPrimaryIndex() { delete _primaryIndex; }

/// @brief return the number of documents from the index
size_t MMFilesPrimaryIndex::size() const { return _primaryIndex->size(); }

/// @brief return the memory usage of the index
size_t MMFilesPrimaryIndex::memory() const {
  return _primaryIndex->memoryUsage();
}

/// @brief return a VelocyPack representation of the index
void MMFilesPrimaryIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                       bool forPersistence) const {
  builder.openObject();
  {
    Index::toVelocyPack(builder, withFigures, forPersistence);
    // hard-coded
    builder.add("unique", VPackValue(true));
    builder.add("sparse", VPackValue(false));
  }
  builder.close();
}

/// @brief return a VelocyPack representation of the index figures
void MMFilesPrimaryIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  _primaryIndex->appendToVelocyPack(builder);
}

Result MMFilesPrimaryIndex::insert(transaction::Methods*, TRI_voc_rid_t,
                                   VPackSlice const&, bool) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "insert() called for primary index";
#endif
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "insert() called for primary index");
}

Result MMFilesPrimaryIndex::remove(transaction::Methods*, TRI_voc_rid_t,
                                   VPackSlice const&, bool) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "remove() called for primary index";
#endif
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "remove() called for primary index");
}

/// @brief unload the index data from memory
void MMFilesPrimaryIndex::unload() {
  _primaryIndex->truncate(
      [](MMFilesSimpleIndexElement const&) { return true; });
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement MMFilesPrimaryIndex::lookupKey(
    transaction::Methods* trx, VPackSlice const& key) const {
  ManagedDocumentResult mmdr;
  IndexLookupContext context(trx, _collection, &mmdr, 1);
  TRI_ASSERT(key.isString());
  return _primaryIndex->findByKey(&context, key.begin());
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement MMFilesPrimaryIndex::lookupKey(
    transaction::Methods* trx, VPackSlice const& key,
    ManagedDocumentResult& mmdr) const {
  IndexLookupContext context(trx, _collection, &mmdr, 1);
  TRI_ASSERT(key.isString());
  return _primaryIndex->findByKey(&context, key.begin());
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement* MMFilesPrimaryIndex::lookupKeyRef(
    transaction::Methods* trx, VPackSlice const& key) const {
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  TRI_ASSERT(key.isString());
  MMFilesSimpleIndexElement* element =
      _primaryIndex->findByKeyRef(&context, key.begin());
  if (element != nullptr && element->revisionId() == 0) {
    return nullptr;
  }
  return element;
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement* MMFilesPrimaryIndex::lookupKeyRef(
    transaction::Methods* trx, VPackSlice const& key,
    ManagedDocumentResult& mmdr) const {
  IndexLookupContext context(trx, _collection, &mmdr, 1);
  TRI_ASSERT(key.isString());
  MMFilesSimpleIndexElement* element =
      _primaryIndex->findByKeyRef(&context, key.begin());
  if (element != nullptr && element->revisionId() == 0) {
    return nullptr;
  }
  return element;
}

/// @brief a method to iterate over all elements in the index in
///        a sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
///        DEPRECATED
MMFilesSimpleIndexElement MMFilesPrimaryIndex::lookupSequential(
    transaction::Methods* trx, arangodb::basics::BucketPosition& position,
    uint64_t& total) {
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  return _primaryIndex->findSequential(&context, position, total);
}

/// @brief request an iterator over all elements in the index in
///        a sequential order.
IndexIterator* MMFilesPrimaryIndex::allIterator(transaction::Methods* trx,
                                                ManagedDocumentResult* mmdr,
                                                bool reverse) const {
  return new MMFilesAllIndexIterator(_collection, trx, mmdr, this,
                                     _primaryIndex, reverse);
}

/// @brief request an iterator over all elements in the index in
///        a random order. It is guaranteed that each element is found
///        exactly once unless the collection is modified.
IndexIterator* MMFilesPrimaryIndex::anyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr) const {
  return new MMFilesAnyIndexIterator(_collection, trx, mmdr, this,
                                     _primaryIndex);
}

/// @brief a method to iterate over all elements in the index in
///        reversed sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
///        DEPRECATED
MMFilesSimpleIndexElement MMFilesPrimaryIndex::lookupSequentialReverse(
    transaction::Methods* trx, arangodb::basics::BucketPosition& position) {
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  return _primaryIndex->findSequentialReverse(&context, position);
}

/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
Result MMFilesPrimaryIndex::insertKey(transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc) {
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  MMFilesSimpleIndexElement element(buildKeyElement(revisionId, doc));

  return IndexResult(_primaryIndex->insert(&context, element), this);
}

Result MMFilesPrimaryIndex::insertKey(transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc,
                                      ManagedDocumentResult& mmdr) {
  IndexLookupContext context(trx, _collection, &mmdr, 1);
  MMFilesSimpleIndexElement element(buildKeyElement(revisionId, doc));

  return IndexResult(_primaryIndex->insert(&context, element), this);
}

/// @brief removes an key/element from the index
Result MMFilesPrimaryIndex::removeKey(transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc) {
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);

  VPackSlice keySlice(transaction::helpers::extractKeyFromDocument(doc));
  MMFilesSimpleIndexElement found =
      _primaryIndex->removeByKey(&context, keySlice.begin());

  if (!found) {
    return IndexResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, this);
  }

  return Result(TRI_ERROR_NO_ERROR);
}

Result MMFilesPrimaryIndex::removeKey(transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc,
                                      ManagedDocumentResult& mmdr) {
  IndexLookupContext context(trx, _collection, &mmdr, 1);

  VPackSlice keySlice(transaction::helpers::extractKeyFromDocument(doc));
  MMFilesSimpleIndexElement found =
      _primaryIndex->removeByKey(&context, keySlice.begin());

  if (!found) {
    return IndexResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, this);
  }

  return Result(TRI_ERROR_NO_ERROR);
}

/// @brief resizes the index
int MMFilesPrimaryIndex::resize(transaction::Methods* trx, size_t targetSize) {
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  return _primaryIndex->resize(&context, targetSize);
}

void MMFilesPrimaryIndex::invokeOnAllElements(
    std::function<bool(DocumentIdentifierToken const&)> work) {
  auto wrappedWork = [&work](MMFilesSimpleIndexElement const& el) -> bool {
    return work(MMFilesToken{el.revisionId()});
  };
  _primaryIndex->invokeOnAllElements(wrappedWork);
}

void MMFilesPrimaryIndex::invokeOnAllElementsForRemoval(
    std::function<bool(MMFilesSimpleIndexElement const&)> work) {
  _primaryIndex->invokeOnAllElementsForRemoval(work);
}

/// @brief checks whether the index supports the condition
bool MMFilesPrimaryIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesPrimaryIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  TRI_ASSERT(node->numMembers() == 1);

  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // value == a.b  ->  flip the two sides
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }
  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, mmdr, attrNode, valNode);
  } else if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      // a.b IN non-array
      return new EmptyIndexIterator(_collection, trx, mmdr, this);
    }

    return createInIterator(trx, mmdr, attrNode, valNode);
  }

  // operator type unsupported
  return new EmptyIndexIterator(_collection, trx, mmdr, this);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* MMFilesPrimaryIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief create the iterator, for a single attribute, IN operator
IndexIterator* MMFilesPrimaryIndex::createInIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // _key or _id?
  bool const isId = (attrNode->stringEquals(StaticStrings::IdString));

  TRI_ASSERT(valNode->isArray());

  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  size_t const n = valNode->numMembers();

  // only leave the valid elements
  for (size_t i = 0; i < n; ++i) {
    handleValNode(trx, keys.get(), valNode->getMemberUnchecked(i), isId);
    TRI_IF_FAILURE("PrimaryIndex::iteratorValNodes") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();
  return new MMFilesPrimaryIndexIterator(_collection, trx, mmdr, this, keys);
}

/// @brief create the iterator, for a single attribute, EQ operator
IndexIterator* MMFilesPrimaryIndex::createEqIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // _key or _id?
  bool const isId = (attrNode->stringEquals(StaticStrings::IdString));

  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  // handle the sole element
  handleValNode(trx, keys.get(), valNode, isId);

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();
  return new MMFilesPrimaryIndexIterator(_collection, trx, mmdr, this, keys);
}

/// @brief add a single value node to the iterator's keys
void MMFilesPrimaryIndex::handleValNode(transaction::Methods* trx,
                                        VPackBuilder* keys,
                                        arangodb::aql::AstNode const* valNode,
                                        bool isId) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  if (isId) {
    // lookup by _id. now validate if the lookup is performed for the
    // correct collection (i.e. _collection)
    TRI_voc_cid_t cid;
    char const* key;
    size_t outLength;
    Result res =
        trx->resolveId(valNode->getStringValue(), valNode->getStringLength(),
                       cid, key, outLength);

    if (!res.ok()) {
      return;
    }

    TRI_ASSERT(cid != 0);
    TRI_ASSERT(key != nullptr);

    if (!trx->state()->isRunningInCluster() && cid != _collection->cid()) {
      // only continue lookup if the id value is syntactically correct and
      // refers to "our" collection, using local collection id
      return;
    }

    if (trx->state()->isRunningInCluster() && cid != _collection->planId()) {
      // only continue lookup if the id value is syntactically correct and
      // refers to "our" collection, using cluster collection id
      return;
    }

    // use _key value from _id
    keys->add(VPackValuePair(key, outLength, VPackValueType::String));
  } else {
    keys->add(VPackValuePair(valNode->getStringValue(),
                             valNode->getStringLength(),
                             VPackValueType::String));
  }
}

MMFilesSimpleIndexElement MMFilesPrimaryIndex::buildKeyElement(
    TRI_voc_rid_t revisionId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(transaction::helpers::extractKeyFromDocument(doc));
  TRI_ASSERT(value.isString());
  return MMFilesSimpleIndexElement(
      revisionId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}
