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

#include "MMFilesEdgeIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Indexes/IndexLookupContext.h"
#include "Indexes/IndexResult.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesToken.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const
    IndexAttributes{{arangodb::basics::AttributeName("_from", false)},
                    {arangodb::basics::AttributeName("_to", false)}};

/// @brief hashes an edge key
static uint64_t HashElementKey(void*, VPackSlice const* key) {
  TRI_ASSERT(key != nullptr);
  // we can get away with the fast hash function here, as edge
  // index values are restricted to strings
  return MMFilesSimpleIndexElement::hash(*key);
}

/// @brief hashes an edge
static uint64_t HashElementEdge(void*, MMFilesSimpleIndexElement const& element,
                                bool byKey) {
  if (byKey) {
    return element.hash();
  }

  TRI_voc_rid_t revisionId = element.revisionId();
  return fasthash64_uint64(revisionId, 0x56781234);
}

/// @brief checks if key and element match
static bool IsEqualKeyEdge(void* userData, VPackSlice const* left,
                           MMFilesSimpleIndexElement const& right) {
  TRI_ASSERT(left != nullptr);
  IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  TRI_ASSERT(context != nullptr);

  try {
    VPackSlice tmp = right.slice(context);
    TRI_ASSERT(tmp.isString());
    return left->equals(tmp);
  } catch (...) {
    return false;
  }
}

/// @brief checks for elements are equal
static bool IsEqualElementEdge(void*, MMFilesSimpleIndexElement const& left,
                               MMFilesSimpleIndexElement const& right) {
  return left.revisionId() == right.revisionId();
}

/// @brief checks for elements are equal
static bool IsEqualElementEdgeByKey(void* userData,
                                    MMFilesSimpleIndexElement const& left,
                                    MMFilesSimpleIndexElement const& right) {
  IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  try {
    VPackSlice lSlice = left.slice(context);
    VPackSlice rSlice = right.slice(context);

    TRI_ASSERT(lSlice.isString());
    TRI_ASSERT(rSlice.isString());

    return lSlice.equals(rSlice);
  } catch (...) {
    return false;
  }
}

MMFilesEdgeIndexIterator::MMFilesEdgeIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, arangodb::MMFilesEdgeIndex const* index,
    TRI_MMFilesEdgeIndexHash_t const* indexImpl,
    std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _index(indexImpl),
      _keys(keys.get()),
      _iterator(_keys->slice()),
      _posInBuffer(0),
      _batchSize(1000),
      _lastElement() {
  keys.release();  // now we have ownership for _keys
}

MMFilesEdgeIndexIterator::~MMFilesEdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool MMFilesEdgeIndexIterator::next(TokenCallback const& cb, size_t limit) {
  if (limit == 0 || (_buffer.empty() && !_iterator.valid())) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }
  while (limit > 0) {
    if (_buffer.empty()) {
      // We start a new lookup
      _posInBuffer = 0;

      VPackSlice tmp = _iterator.value();
      if (tmp.isObject()) {
        tmp = tmp.get(StaticStrings::IndexEq);
      }
      _index->lookupByKey(&_context, &tmp, _buffer, _batchSize);
    } else if (_posInBuffer >= _buffer.size()) {
      // We have to refill the buffer
      _buffer.clear();

      _posInBuffer = 0;
      _index->lookupByKeyContinue(&_context, _lastElement, _buffer, _batchSize);
    }

    if (_buffer.empty()) {
      _iterator.next();
      _lastElement = MMFilesSimpleIndexElement();
      if (!_iterator.valid()) {
        return false;
      }
    } else {
      _lastElement = _buffer.back();
      // found something
      cb(MMFilesToken{_buffer[_posInBuffer++].revisionId()});
      limit--;
    }
  }
  return true;
}

void MMFilesEdgeIndexIterator::reset() {
  _posInBuffer = 0;
  _buffer.clear();
  _iterator.reset();
  _lastElement = MMFilesSimpleIndexElement();
}

MMFilesEdgeIndex::MMFilesEdgeIndex(TRI_idx_iid_t iid,
                                   arangodb::LogicalCollection* collection)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::FromString,
                                                  false)},
                 {arangodb::basics::AttributeName(StaticStrings::ToString,
                                                  false)}}),
            false, false),
      _edgesFrom(nullptr),
      _edgesTo(nullptr),
      _numBuckets(1) {
  TRI_ASSERT(iid != 0);

  if (collection != nullptr) {
    // collection is a nullptr in the coordinator case
    auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
    TRI_ASSERT(physical != nullptr);
    _numBuckets = static_cast<size_t>(physical->indexBuckets());
  }

  auto context = [this]() -> std::string { return this->context(); };

  _edgesFrom = new TRI_MMFilesEdgeIndexHash_t(
      HashElementKey, HashElementEdge, IsEqualKeyEdge, IsEqualElementEdge,
      IsEqualElementEdgeByKey, _numBuckets, 64, context);

  _edgesTo = new TRI_MMFilesEdgeIndexHash_t(
      HashElementKey, HashElementEdge, IsEqualKeyEdge, IsEqualElementEdge,
      IsEqualElementEdgeByKey, _numBuckets, 64, context);
}

MMFilesEdgeIndex::~MMFilesEdgeIndex() {
  delete _edgesFrom;
  delete _edgesTo;
}

/// @brief return a selectivity estimate for the index
double MMFilesEdgeIndex::selectivityEstimateLocal(
    arangodb::StringRef const* attribute) const {
  if (_edgesFrom == nullptr || _edgesTo == nullptr ||
      ServerState::instance()->isCoordinator()) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return 0.1;
  }

  if (attribute != nullptr) {
    // the index attribute is given here
    // now check if we can restrict the selectivity estimation to the correct
    // part of the index
    if (attribute->compare(StaticStrings::FromString) == 0) {
      // _from
      return _edgesFrom->selectivity();
    } else if (attribute->compare(StaticStrings::ToString) == 0) {
      // _to
      return _edgesTo->selectivity();
    }
    // other attribute. now return the average selectivity
  }

  // return average selectivity of the two index parts
  double estimate = (_edgesFrom->selectivity() + _edgesTo->selectivity()) * 0.5;
  TRI_ASSERT(estimate >= 0.0 &&
             estimate <= 1.00001);  // floating-point tolerance
  return estimate;
}

/// @brief return the memory usage for the index
size_t MMFilesEdgeIndex::memory() const {
  TRI_ASSERT(_edgesFrom != nullptr);
  TRI_ASSERT(_edgesTo != nullptr);
  return _edgesFrom->memoryUsage() + _edgesTo->memoryUsage();
}

/// @brief return a VelocyPack representation of the index
void MMFilesEdgeIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                    bool forPersistence) const {
  builder.openObject();
  Index::toVelocyPack(builder, withFigures, forPersistence);
  // hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
  builder.close();
}

/// @brief return a VelocyPack representation of the index figures
void MMFilesEdgeIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);

  builder.add("from", VPackValue(VPackValueType::Object));
  _edgesFrom->appendToVelocyPack(builder);
  builder.close();

  builder.add("to", VPackValue(VPackValueType::Object));
  _edgesTo->appendToVelocyPack(builder);
  builder.close();
  // builder.add("buckets", VPackValue(_numBuckets));
}

Result MMFilesEdgeIndex::insert(transaction::Methods* trx,
                                TRI_voc_rid_t revisionId, VPackSlice const& doc,
                                bool isRollback) {
  MMFilesSimpleIndexElement fromElement(buildFromElement(revisionId, doc));
  MMFilesSimpleIndexElement toElement(buildToElement(revisionId, doc));

  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  _edgesFrom->insert(&context, fromElement, true, isRollback);

  try {
    _edgesTo->insert(&context, toElement, true, isRollback);
  } catch (std::bad_alloc const&) {
    // roll back partial insert
    _edgesFrom->remove(&context, fromElement);
    return IndexResult(TRI_ERROR_OUT_OF_MEMORY, this);
  } catch (...) {
    // roll back partial insert
    _edgesFrom->remove(&context, fromElement);
    return IndexResult(TRI_ERROR_INTERNAL, this);
  }

  return Result(TRI_ERROR_NO_ERROR);
}

Result MMFilesEdgeIndex::remove(transaction::Methods* trx,
                                TRI_voc_rid_t revisionId, VPackSlice const& doc,
                                bool isRollback) {
  MMFilesSimpleIndexElement fromElement(buildFromElement(revisionId, doc));
  MMFilesSimpleIndexElement toElement(buildToElement(revisionId, doc));

  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);

  try {
    _edgesFrom->remove(&context, fromElement);
    _edgesTo->remove(&context, toElement);
    return Result(TRI_ERROR_NO_ERROR);
  } catch (...) {
    if (isRollback) {
      return Result(TRI_ERROR_NO_ERROR);
    }
    return IndexResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, this);
  }
}

void MMFilesEdgeIndex::batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  if (documents.empty()) {
    return;
  }

  std::shared_ptr<std::vector<MMFilesSimpleIndexElement>> fromElements;
  fromElements.reset(new std::vector<MMFilesSimpleIndexElement>());
  fromElements->reserve(documents.size());

  std::shared_ptr<std::vector<MMFilesSimpleIndexElement>> toElements;
  toElements.reset(new std::vector<MMFilesSimpleIndexElement>());
  toElements->reserve(documents.size());

  // functions that will be called for each thread
  auto creator = [&trx, this]() -> void* {
    ManagedDocumentResult* result = new ManagedDocumentResult;
    return new IndexLookupContext(trx, _collection, result, 1);
  };
  auto destroyer = [](void* userData) {
    IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };

  // TODO: create parallel tasks for this

  // _from
  for (auto const& it : documents) {
    VPackSlice value(transaction::helpers::extractFromFromDocument(it.second));
    fromElements->emplace_back(MMFilesSimpleIndexElement(
        it.first, value,
        static_cast<uint32_t>(value.begin() - it.second.begin())));
  }

  // _to
  for (auto const& it : documents) {
    VPackSlice value(transaction::helpers::extractToFromDocument(it.second));
    toElements->emplace_back(MMFilesSimpleIndexElement(
        it.first, value,
        static_cast<uint32_t>(value.begin() - it.second.begin())));
  }

  _edgesFrom->batchInsert(creator, destroyer, fromElements, queue);
  _edgesTo->batchInsert(creator, destroyer, toElements, queue);
}

/// @brief unload the index data from memory
void MMFilesEdgeIndex::unload() {
  _edgesFrom->truncate([](MMFilesSimpleIndexElement const&) { return true; });
  _edgesTo->truncate([](MMFilesSimpleIndexElement const&) { return true; });
}

/// @brief provides a size hint for the edge index
int MMFilesEdgeIndex::sizeHint(transaction::Methods* trx, size_t size) {
  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesFrom->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  ManagedDocumentResult result;
  IndexLookupContext context(trx, _collection, &result, 1);
  int err = _edgesFrom->resize(&context, size + 2049);

  if (err != TRI_ERROR_NO_ERROR) {
    return err;
  }

  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesTo->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  return _edgesTo->resize(&context, size + 2049);
}

/// @brief checks whether the index supports the condition
bool MMFilesEdgeIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesEdgeIndex::iteratorForCondition(
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
    // got value == a.b  -> flip sides
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }
  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, mmdr, attrNode, valNode);
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
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
arangodb::aql::AstNode* MMFilesEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief Transform the list of search slices to search values.
///        This will multiply all IN entries and simply return all other
///        entries.
void MMFilesEdgeIndex::expandInSearchValues(VPackSlice const slice,
                                            VPackBuilder& builder) const {
  TRI_ASSERT(slice.isArray());
  builder.openArray();
  for (auto const& side : VPackArrayIterator(slice)) {
    if (side.isNull()) {
      builder.add(side);
    } else {
      TRI_ASSERT(side.isArray());
      builder.openArray();
      for (auto const& item : VPackArrayIterator(side)) {
        TRI_ASSERT(item.isObject());
        if (item.hasKey(StaticStrings::IndexEq)) {
          TRI_ASSERT(!item.hasKey(StaticStrings::IndexIn));
          builder.add(item);
        } else {
          TRI_ASSERT(item.hasKey(StaticStrings::IndexIn));
          VPackSlice list = item.get(StaticStrings::IndexIn);
          TRI_ASSERT(list.isArray());
          for (auto const& it : VPackArrayIterator(list)) {
            builder.openObject();
            builder.add(StaticStrings::IndexEq, it);
            builder.close();
          }
        }
      }
      builder.close();
    }
  }
  builder.close();
}

/// @brief create the iterator
IndexIterator* MMFilesEdgeIndex::createEqIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  handleValNode(keys.get(), valNode);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  // _from or _to?
  bool const isFrom = (attrNode->stringEquals(StaticStrings::FromString));

  return new MMFilesEdgeIndexIterator(_collection, trx, mmdr, this,
                                      isFrom ? _edgesFrom : _edgesTo, keys);
}

/// @brief create the iterator
IndexIterator* MMFilesEdgeIndex::createInIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  size_t const n = valNode->numMembers();
  for (size_t i = 0; i < n; ++i) {
    handleValNode(keys.get(), valNode->getMemberUnchecked(i));
    TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  // _from or _to?
  bool const isFrom = (attrNode->stringEquals(StaticStrings::FromString));

  return new MMFilesEdgeIndexIterator(_collection, trx, mmdr, this,
                                      isFrom ? _edgesFrom : _edgesTo, keys);
}

/// @brief add a single value node to the iterator's keys
void MMFilesEdgeIndex::handleValNode(
    VPackBuilder* keys, arangodb::aql::AstNode const* valNode) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(StaticStrings::IndexEq,
            VPackValuePair(valNode->getStringValue(),
                           valNode->getStringLength(), VPackValueType::String));
  keys->close();

  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

MMFilesSimpleIndexElement MMFilesEdgeIndex::buildFromElement(
    TRI_voc_rid_t revisionId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(transaction::helpers::extractFromFromDocument(doc));
  TRI_ASSERT(value.isString());
  return MMFilesSimpleIndexElement(
      revisionId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}

MMFilesSimpleIndexElement MMFilesEdgeIndex::buildToElement(
    TRI_voc_rid_t revisionId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(transaction::helpers::extractToFromDocument(doc));
  TRI_ASSERT(value.isString());
  return MMFilesSimpleIndexElement(
      revisionId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}
