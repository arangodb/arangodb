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
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/VirtualCollection.h"
#endif

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

MMFilesPrimaryIndexEqIterator::MMFilesPrimaryIndexEqIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    MMFilesPrimaryIndex const* index,
    std::unique_ptr<VPackBuilder> key)
    : IndexIterator(collection, trx),
      _index(index),
      _key(std::move(key)),
      _done(false) {
  TRI_ASSERT(_key->slice().isString());
}

MMFilesPrimaryIndexEqIterator::~MMFilesPrimaryIndexEqIterator() {
  if (_key != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_key.release());
  }
}

bool MMFilesPrimaryIndexEqIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(limit > 0);
  if (_done || limit == 0) {
    return false;
  }
  
  _done = true;
  TRI_ASSERT(_key->slice().isString());
  MMFilesSimpleIndexElement result =
      _index->lookupKey(_trx, _key->slice());
  if (result) {
    cb(LocalDocumentId{result.localDocumentId()});
  }
  return false;
}

bool MMFilesPrimaryIndexEqIterator::nextDocument(DocumentCallback const& cb, size_t limit) {
  TRI_ASSERT(limit > 0);
  if (_done || limit == 0) {
    return false;
  }
  
  _done = true;
  ManagedDocumentResult mdr;
  TRI_ASSERT(_key->slice().isString());
  MMFilesSimpleIndexElement result =
      _index->lookupKey(_trx, _key->slice(), mdr);
  if (result) {
    cb(result.localDocumentId(), VPackSlice(mdr.vpack())); 
  }
  return false;
}

void MMFilesPrimaryIndexEqIterator::reset() { _done = false; }

MMFilesPrimaryIndexInIterator::MMFilesPrimaryIndexInIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    MMFilesPrimaryIndex const* index,
    std::unique_ptr<VPackBuilder> keys)
    : IndexIterator(collection, trx),
      _index(index),
      _keys(std::move(keys)),
      _iterator(_keys->slice()) {
  TRI_ASSERT(_keys->slice().isArray());
}

MMFilesPrimaryIndexInIterator::~MMFilesPrimaryIndexInIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool MMFilesPrimaryIndexInIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(limit > 0);
  if (!_iterator.valid() || limit == 0) {
    return false;
  }
  while (_iterator.valid() && limit > 0) {
    // TODO: use version that hands in an existing mdr
    MMFilesSimpleIndexElement result =
        _index->lookupKey(_trx, _iterator.value());
    _iterator.next();
    if (result) {
      cb(LocalDocumentId{result.localDocumentId()});
      --limit;
    }
  }
  return _iterator.valid();
}

void MMFilesPrimaryIndexInIterator::reset() { _iterator.reset(); }

MMFilesAllIndexIterator::MMFilesAllIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    MMFilesPrimaryIndex const* index,
    MMFilesPrimaryIndexImpl const* indexImpl)
    : IndexIterator(collection, trx),
      _index(indexImpl),
      _total(0) {}

bool MMFilesAllIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  while (limit > 0) {
    MMFilesSimpleIndexElement element = _index->findSequential(nullptr, _position, _total);

    if (element) {
      cb(LocalDocumentId{element.localDocumentId()});
      --limit;
    } else {
      return false;
    }
  }
  return true;
}

bool MMFilesAllIndexIterator::nextDocument(DocumentCallback const& cb, size_t limit) {
  _documentIds.clear();
  _documentIds.reserve(limit);

  bool done = false;
  while (limit > 0) {
    MMFilesSimpleIndexElement element = _index->findSequential(nullptr, _position, _total);

    if (element) {
      _documentIds.emplace_back(std::make_pair(element.localDocumentId(), nullptr));
      --limit;
    } else {
      done = true;
      break;
    }
  }

  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  physical->readDocumentWithCallback(_trx, _documentIds, cb);
  return !done;
}

// Skip the first count-many entries
void MMFilesAllIndexIterator::skip(uint64_t count, uint64_t& skipped) {
  while (count > 0) {
    MMFilesSimpleIndexElement element = _index->findSequential(nullptr, _position, _total);

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
    MMFilesPrimaryIndex const* index,
    MMFilesPrimaryIndexImpl const* indexImpl)
    : IndexIterator(collection, trx),
      _index(indexImpl),
      _step(0),
      _total(0) {}

bool MMFilesAnyIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  if (limit == 0) {
    return false;
  }

  do {
    MMFilesSimpleIndexElement element =
        _index->findRandom(nullptr, _initial, _position, _step, _total);
    if (!element) {
      return false;
    }

    cb(LocalDocumentId{element.localDocumentId()});
    --limit;
  } while (limit > 0);

  return true;
}

void MMFilesAnyIndexIterator::reset() {
  _step = 0;
  _total = 0;
  _position = _initial;
}

MMFilesPrimaryIndex::MMFilesPrimaryIndex(
    arangodb::LogicalCollection& collection
)
    : MMFilesIndex(0, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::KeyString,
                                                  false)}}),
            /*unique*/ true , /*sparse*/ false) {
  auto physical =
    static_cast<arangodb::MMFilesCollection*>(collection.getPhysical());

  TRI_ASSERT(physical != nullptr);
  size_t indexBuckets = static_cast<size_t>(physical->indexBuckets());

  if (collection.isAStub()) {
      // in order to reduce memory usage
      indexBuckets = 1;
  }

  _primaryIndex.reset(new MMFilesPrimaryIndexImpl(MMFilesPrimaryIndexHelper(), indexBuckets,
      [this]() -> std::string { return this->context(); }));
}

/// @brief return the number of documents from the index
size_t MMFilesPrimaryIndex::size() const { return _primaryIndex->size(); }

/// @brief return the memory usage of the index
size_t MMFilesPrimaryIndex::memory() const {
  return _primaryIndex->memoryUsage();
}

/// @brief return a VelocyPack representation of the index
void MMFilesPrimaryIndex::toVelocyPack(VPackBuilder& builder,
                                       std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  // hard-coded
  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(true)
  );
  builder.add(
    arangodb::StaticStrings::IndexSparse,
    arangodb::velocypack::Value(false)
  );
  builder.close();
}

/// @brief return a VelocyPack representation of the index figures
void MMFilesPrimaryIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  _primaryIndex->appendToVelocyPack(builder);
}

Result MMFilesPrimaryIndex::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const&,
    Index::OperationMode mode
) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
      << "insert() called for primary index";
#endif
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "insert() called for primary index");
}

Result MMFilesPrimaryIndex::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const&,
    Index::OperationMode mode
) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
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
  ManagedDocumentResult mdr;
  return lookupKey(trx, key, mdr);
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement MMFilesPrimaryIndex::lookupKey(
    transaction::Methods* trx, VPackSlice const& key,
    ManagedDocumentResult& mdr) const {
  MMFilesIndexLookupContext context(trx, &_collection, &mdr, 1);
  TRI_ASSERT(key.isString());

  return _primaryIndex->findByKey(&context, key.begin());
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement* MMFilesPrimaryIndex::lookupKeyRef(
    transaction::Methods* trx, VPackSlice const& key) const {
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, 1);
  TRI_ASSERT(key.isString());
  MMFilesSimpleIndexElement* element =
      _primaryIndex->findByKeyRef(&context, key.begin());
  TRI_ASSERT(element != nullptr);

  if (!element->isSet()) {
    return nullptr;
  }

  return element;
}

/// @brief looks up an element given a key
MMFilesSimpleIndexElement* MMFilesPrimaryIndex::lookupKeyRef(
    transaction::Methods* trx, VPackSlice const& key,
    ManagedDocumentResult& mdr) const {
  MMFilesIndexLookupContext context(trx, &_collection, &mdr, 1);
  TRI_ASSERT(key.isString());
  MMFilesSimpleIndexElement* element =
      _primaryIndex->findByKeyRef(&context, key.begin());
  TRI_ASSERT(element != nullptr);

  if (!element->isSet()) {
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
  MMFilesIndexLookupContext context(trx, &_collection, &result, 1);

  return _primaryIndex->findSequential(&context, position, total);
}

/// @brief request an iterator over all elements in the index in
///        a sequential order.
IndexIterator* MMFilesPrimaryIndex::allIterator(transaction::Methods* trx) const {
  return new MMFilesAllIndexIterator(
    &_collection, trx, this, _primaryIndex.get()
  );
}

/// @brief request an iterator over all elements in the index in
///        a random order. It is guaranteed that each element is found
///        exactly once unless the collection is modified.
IndexIterator* MMFilesPrimaryIndex::anyIterator(transaction::Methods* trx) const {
  return new MMFilesAnyIndexIterator(
    &_collection, trx, this, _primaryIndex.get()
  );
}

/// @brief a method to iterate over all elements in the index in
///        reversed sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
///        DEPRECATED
MMFilesSimpleIndexElement MMFilesPrimaryIndex::lookupSequentialReverse(
    transaction::Methods* trx, arangodb::basics::BucketPosition& position) {
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, 1);

  return _primaryIndex->findSequentialReverse(&context, position);
}

/// @brief adds a key/element to the index
Result MMFilesPrimaryIndex::insertKey(transaction::Methods* trx,
                                      LocalDocumentId const& documentId,
                                      VPackSlice const& doc,
                                      OperationMode mode) {
  ManagedDocumentResult mdr;
  return insertKey(trx, documentId, doc, mdr, mode);
}

Result MMFilesPrimaryIndex::insertKey(transaction::Methods* trx,
                                      LocalDocumentId const& documentId,
                                      VPackSlice const& doc,
                                      ManagedDocumentResult& mdr,
                                      OperationMode mode) {
  MMFilesIndexLookupContext context(trx, &_collection, &mdr, 1);
  MMFilesSimpleIndexElement element(buildKeyElement(documentId, doc));
  Result res;

// TODO: we can pass in a special MMFilesIndexLookupContext which has some more on the information
// about the to-be-inserted document. this way we can spare one lookup in
// IsEqualElementElementByKey
  int r = _primaryIndex->insert(&context, element);

  if (r == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    std::string existingId(doc.get(StaticStrings::KeyString).copyString());
    if (mode == OperationMode::internal) {
      return res.reset(r, std::move(existingId));
    }

    return addErrorMsg(res, r, existingId);
  }

  return addErrorMsg(res, r);
}

/// @brief removes a key/element from the index
Result MMFilesPrimaryIndex::removeKey(transaction::Methods* trx,
                                      LocalDocumentId const& documentId,
                                      VPackSlice const& doc,
                                      OperationMode mode) {
  ManagedDocumentResult mdr;
  return removeKey(trx, documentId, doc, mdr, mode);
}

Result MMFilesPrimaryIndex::removeKey(transaction::Methods* trx,
                                      LocalDocumentId const&,
                                      VPackSlice const& doc,
                                      ManagedDocumentResult& mdr,
                                      OperationMode mode) {
  MMFilesIndexLookupContext context(trx, &_collection, &mdr, 1);
  VPackSlice keySlice(transaction::helpers::extractKeyFromDocument(doc));
  MMFilesSimpleIndexElement found =
      _primaryIndex->removeByKey(&context, keySlice.begin());

  Result res;
  if (!found) {
    return addErrorMsg(res, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }

  return res;
}

/// @brief resizes the index
int MMFilesPrimaryIndex::resize(transaction::Methods* trx, size_t targetSize) {
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, 1);
  return _primaryIndex->resize(&context, targetSize);
}

void MMFilesPrimaryIndex::invokeOnAllElements(
    std::function<bool(LocalDocumentId const&)> work) {
  auto wrappedWork = [&work](MMFilesSimpleIndexElement const& el) -> bool {
    return work(LocalDocumentId{el.localDocumentId()});
  };
  _primaryIndex->invokeOnAllElements(wrappedWork);
}

void MMFilesPrimaryIndex::invokeOnAllElementsForRemoval(
    std::function<bool(MMFilesSimpleIndexElement const&)> work) {
  _primaryIndex->invokeOnAllElementsForRemoval(work);
}

/// @brief checks whether the index supports the condition
bool MMFilesPrimaryIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const&,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesPrimaryIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult*,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);

  auto comp = node;
  if (node->type == aql::NODE_TYPE_OPERATOR_NARY_AND) {
    TRI_ASSERT(node->numMembers() == 1);
    comp = node->getMember(0);
  }

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
    return createEqIterator(trx, attrNode, valNode);
  }
   
  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (valNode->isArray()) {
      // a.b IN array
      return createInIterator(trx, attrNode, valNode);
    }
  }

  // operator type unsupported or IN used on non-array
  return new EmptyIndexIterator(&_collection, trx);
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
    transaction::Methods* trx,
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

  return new MMFilesPrimaryIndexInIterator(
    &_collection, trx, this, std::move(keys)
  );
}

/// @brief create the iterator, for a single attribute, EQ operator
IndexIterator* MMFilesPrimaryIndex::createEqIterator(
    transaction::Methods* trx,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
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
    return new MMFilesPrimaryIndexEqIterator(
      &_collection, trx, this, std::move(key)
    );
  }
  
  return new EmptyIndexIterator(&_collection, trx);
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
    char const* key = nullptr;
    size_t outLength = 0;
    std::shared_ptr<LogicalCollection> collection;
    Result res =
        trx->resolveId(valNode->getStringValue(),
                       valNode->getStringLength(), collection, key, outLength);

    if (!res.ok()) {
      return;
    }

    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(key != nullptr);

    bool const isInCluster = trx->state()->isRunningInCluster();

    if (!isInCluster && collection->id() != _collection.id()) {
      // only continue lookup if the id value is syntactically correct and
      // refers to "our" collection, using local collection id
      return;
    }

    if (isInCluster) {
#ifdef USE_ENTERPRISE
      if (collection->isSmart() && collection->type() == TRI_COL_TYPE_EDGE) {
        auto c = dynamic_cast<VirtualSmartEdgeCollection const*>(collection.get());
        if (c == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to cast smart edge collection");
        }

        if (_collection.planId() != c->getLocalCid() &&
            _collection.planId() != c->getFromCid() &&
            _collection.planId() != c->getToCid()) {
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
                             valNode->getStringLength(),
                             VPackValueType::String));
  }
}

MMFilesSimpleIndexElement MMFilesPrimaryIndex::buildKeyElement(
    LocalDocumentId const& documentId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(transaction::helpers::extractKeyFromDocument(doc));
  TRI_ASSERT(value.isString());
  return MMFilesSimpleIndexElement(
      documentId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}
