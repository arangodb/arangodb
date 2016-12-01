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

#include "EdgeIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Indexes/IndexLookupContext.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/transaction.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes
    {{arangodb::basics::AttributeName("_from", false)},
     {arangodb::basics::AttributeName("_to", false)}};

/// @brief hashes an edge key
static uint64_t HashElementKey(void*, VPackSlice const* key) {
  TRI_ASSERT(key != nullptr);
  // we can get away with the fast hash function here, as edge
  // index values are restricted to strings
  return SimpleIndexElement::hash(*key);
}

/// @brief hashes an edge
static uint64_t HashElementEdge(void*, SimpleIndexElement const& element, bool byKey) {
  if (byKey) {
    return element.hash();
  }

  TRI_voc_rid_t revisionId = element.revisionId();
  return fasthash64(&revisionId, sizeof(revisionId), 0x56781234);
}

/// @brief checks if key and element match
static bool IsEqualKeyEdge(void* userData, VPackSlice const* left, SimpleIndexElement const& right) {
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
static bool IsEqualElementEdge(void*, SimpleIndexElement const& left, SimpleIndexElement const& right) {
  return left.revisionId() == right.revisionId();
}

/// @brief checks for elements are equal
static bool IsEqualElementEdgeByKey(void* userData, SimpleIndexElement const& left, SimpleIndexElement const& right) {
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
  
EdgeIndexIterator::EdgeIndexIterator(LogicalCollection* collection, arangodb::Transaction* trx,
                                     ManagedDocumentResult* mmdr,
                                     arangodb::EdgeIndex const* index,
                                     TRI_EdgeIndexHash_t const* indexImpl,
                                     std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _index(indexImpl),
      _keys(keys.get()),
      _iterator(_keys->slice()),
      _posInBuffer(0),
      _batchSize(1000),
      _lastElement() {
  
  keys.release(); // now we have ownership for _keys
}

EdgeIndexIterator::~EdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context 
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

IndexLookupResult EdgeIndexIterator::next() {
  while (_iterator.valid()) {
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
      _lastElement = SimpleIndexElement();
    } else {
      _lastElement = _buffer.back();
      // found something
      return IndexLookupResult(_buffer[_posInBuffer++].revisionId());
    }

    // found no result. now go to next lookup value in _keys
    _iterator.next();
  }

  return IndexLookupResult();
}

void EdgeIndexIterator::nextBabies(std::vector<IndexLookupResult>& buffer, size_t limit) {
  size_t atMost = _batchSize > limit ? limit : _batchSize;

  if (atMost == 0) {
    // nothing to do
    buffer.clear();
    _lastElement = SimpleIndexElement();
    return;
  }

  while (_iterator.valid()) {
    _buffer.clear();
    if (buffer.empty()) {
      VPackSlice tmp = _iterator.value();
      if (tmp.isObject()) {
        tmp = tmp.get(StaticStrings::IndexEq);
      }
      _index->lookupByKey(&_context, &tmp, _buffer, atMost);
      // fallthrough intentional
    } else {
      // Continue the lookup
      buffer.clear();

      _index->lookupByKeyContinue(&_context, _lastElement, _buffer, atMost);
    }

    for (auto& it : _buffer) {
      buffer.emplace_back(it.revisionId());
    }
    
    if (_buffer.empty()) {
      _lastElement = SimpleIndexElement();
    } else {
      _lastElement = _buffer.back();
      // found something
      return;
    }
    
    // found no result. now go to next lookup value in _keys
    _iterator.next();
  }

  buffer.clear();
}

void EdgeIndexIterator::reset() {
  _posInBuffer = 0;
  _buffer.clear();
  _iterator.reset();
  _lastElement = SimpleIndexElement();
}
  
AnyDirectionEdgeIndexIterator::AnyDirectionEdgeIndexIterator(LogicalCollection* collection,
                                arangodb::Transaction* trx,
                                ManagedDocumentResult* mmdr,
                                arangodb::EdgeIndex const* index,
                                EdgeIndexIterator* outboundIterator,
                                EdgeIndexIterator* inboundIterator)
    : IndexIterator(collection, trx, mmdr, index),
      _outbound(outboundIterator),
      _inbound(inboundIterator),
      _useInbound(false) {}

IndexLookupResult AnyDirectionEdgeIndexIterator::next() {
  IndexLookupResult res;
  if (_useInbound) {
    do {
      res = _inbound->next();
    } while (res && _seen.find(res.revisionId()) != _seen.end());
    return res;
  }
  res = _outbound->next();
  if (!res) {
    _useInbound = true;
    return next();
  }
  _seen.emplace(res.revisionId());
  return res;
}

void AnyDirectionEdgeIndexIterator::nextBabies(std::vector<IndexLookupResult>& result, size_t limit) {
  result.clear();
  for (size_t i = 0; i < limit; ++i) {
    IndexLookupResult res = next();
    if (!res) {
      return;
    }
    result.emplace_back(res);
  }
}

void AnyDirectionEdgeIndexIterator::reset() {
  _useInbound = false;
  _seen.clear();
  _outbound->reset();
  _inbound->reset();
}

EdgeIndex::EdgeIndex(TRI_idx_iid_t iid, arangodb::LogicalCollection* collection)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::FromString, false)},
                 {arangodb::basics::AttributeName(StaticStrings::ToString, false)}}),
            false, false),
      _edgesFrom(nullptr),
      _edgesTo(nullptr),
      _numBuckets(1) {
  TRI_ASSERT(iid != 0);

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    _numBuckets = static_cast<size_t>(collection->indexBuckets());
  }

  auto context = [this]() -> std::string { return this->context(); };

  _edgesFrom = new TRI_EdgeIndexHash_t(HashElementKey, HashElementEdge,
                                       IsEqualKeyEdge, IsEqualElementEdge,
                                       IsEqualElementEdgeByKey, _numBuckets,
                                       64, context);

  _edgesTo = new TRI_EdgeIndexHash_t(
      HashElementKey, HashElementEdge, IsEqualKeyEdge, IsEqualElementEdge,
      IsEqualElementEdgeByKey, _numBuckets, 64, context);
}

EdgeIndex::~EdgeIndex() {
  delete _edgesFrom;
  delete _edgesTo;
}

void EdgeIndex::buildSearchValue(TRI_edge_direction_e dir,
                                 std::string const& id, VPackBuilder& builder) {
  builder.openArray();
  switch (dir) {
    case TRI_EDGE_OUT:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
  }
  builder.close();
}

void EdgeIndex::buildSearchValue(TRI_edge_direction_e dir,
                                 VPackSlice const& id, VPackBuilder& builder) {
  TRI_ASSERT(id.isString());
  builder.openArray();
  switch (dir) {
    case TRI_EDGE_OUT:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
  }
  builder.close();
}

void EdgeIndex::buildSearchValueFromArray(TRI_edge_direction_e dir,
                                          VPackSlice const ids,
                                          VPackBuilder& builder) {
  TRI_ASSERT(ids.isArray());
  builder.openArray();
  switch (dir) {
    case TRI_EDGE_OUT:
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
  }
  builder.close();
}

/// @brief return a selectivity estimate for the index
double EdgeIndex::selectivityEstimate(arangodb::StringRef const* attribute) const {
  if (_edgesFrom == nullptr || 
      _edgesTo == nullptr || 
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
size_t EdgeIndex::memory() const {
  TRI_ASSERT(_edgesFrom != nullptr);
  TRI_ASSERT(_edgesTo != nullptr);
  return _edgesFrom->memoryUsage() + _edgesTo->memoryUsage();
}

/// @brief return a VelocyPack representation of the index
void EdgeIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);

  // hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
}

/// @brief return a VelocyPack representation of the index figures
void EdgeIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  builder.add("buckets", VPackValue(_numBuckets));
}

int EdgeIndex::insert(arangodb::Transaction* trx, TRI_voc_rid_t revisionId,
                      VPackSlice const& doc, bool isRollback) {
  SimpleIndexElement fromElement(buildFromElement(revisionId, doc));
  SimpleIndexElement toElement(buildToElement(revisionId, doc));

  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, 1); 
  _edgesFrom->insert(&context, fromElement, true, isRollback);

  try {
    _edgesTo->insert(&context, toElement, true, isRollback);
  } catch (...) {
    // roll back partial insert
    _edgesFrom->remove(&context, fromElement);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

int EdgeIndex::remove(arangodb::Transaction* trx, TRI_voc_rid_t revisionId,
                      VPackSlice const& doc, bool isRollback) {
  SimpleIndexElement fromElement(buildFromElement(revisionId, doc));
  SimpleIndexElement toElement(buildToElement(revisionId, doc));
  
  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, 1); 
 
  try { 
    _edgesFrom->remove(&context, fromElement);
    _edgesTo->remove(&context, toElement);
    return TRI_ERROR_NO_ERROR;
  } catch (...) {
    if (isRollback) {
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
}

int EdgeIndex::batchInsert(arangodb::Transaction* trx,
                           std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
                           size_t numThreads) {
  if (documents.empty()) {
    return TRI_ERROR_NO_ERROR;
  }
  
  std::vector<SimpleIndexElement> elements;
  elements.reserve(documents.size());

  // functions that will be called for each thread
  auto creator = [&trx, this]() -> void* {
    ManagedDocumentResult* result = new ManagedDocumentResult(trx);
    return new IndexLookupContext(trx, _collection, result, 1);
  };
  auto destroyer = [](void* userData) {
    IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };

  // _from
  for (auto const& it : documents) {
    VPackSlice value(Transaction::extractFromFromDocument(it.second));
    elements.emplace_back(SimpleIndexElement(it.first, value, static_cast<uint32_t>(value.begin() - it.second.begin())));
  }

  int res = _edgesFrom->batchInsert(creator, destroyer, &elements, numThreads);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // _to
  elements.clear();
  for (auto const& it : documents) {
    VPackSlice value(Transaction::extractToFromDocument(it.second));
    elements.emplace_back(SimpleIndexElement(it.first, value, static_cast<uint32_t>(value.begin() - it.second.begin())));
  }

  res = _edgesTo->batchInsert(creator, destroyer, &elements, numThreads);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  return TRI_ERROR_NO_ERROR;
}

/// @brief unload the index data from memory
int EdgeIndex::unload() {
  _edgesFrom->truncate([](SimpleIndexElement const&) { return true; });
  _edgesTo->truncate([](SimpleIndexElement const&) { return true; });

  return TRI_ERROR_NO_ERROR;
}

/// @brief provides a size hint for the edge index
int EdgeIndex::sizeHint(arangodb::Transaction* trx, size_t size) {
  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesFrom->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  ManagedDocumentResult result(trx); 
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
bool EdgeIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {

  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* EdgeIndex::iteratorForCondition(
    arangodb::Transaction* trx, 
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
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
      return nullptr;
    }

    return createInIterator(trx, mmdr, attrNode, valNode);
  }

  // operator type unsupported
  return nullptr;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* EdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief Transform the list of search slices to search values.
///        This will multiply all IN entries and simply return all other
///        entries.
void EdgeIndex::expandInSearchValues(VPackSlice const slice,
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
/// @brief creates an IndexIterator for the given VelocyPackSlices.
///        The searchValue is a an Array with exactly two Entries.
///        If the first is set it means we are searching for _from (OUTBOUND),
///        if the second is set we are searching for _to (INBOUND).
///        if both are set we are search for ANY direction. Result is made
///        DISTINCT.
///        Each defined slice that is set has to be list of keys to search for.
///        Each key needs to have the following formats:
///
///        1) {"eq": <compareValue>} // The value in index is exactly this
///        
///        Reverse is not supported, hence ignored
///        NOTE: The iterator is only valid as long as the slice points to
///        a valid memory region.
IndexIterator* EdgeIndex::iteratorForSlice(
    arangodb::Transaction* trx, 
    ManagedDocumentResult* mmdr,
    arangodb::velocypack::Slice const searchValues, bool) const {
  if (!searchValues.isArray() || searchValues.length() != 2) {
    // Invalid searchValue
    return nullptr;
  }

  VPackArrayIterator it(searchValues);
  TRI_ASSERT(it.valid());

  VPackSlice const from = it.value();
  
  it.next();
  TRI_ASSERT(it.valid());
  VPackSlice const to = it.value();

  if (!from.isNull()) {
    TRI_ASSERT(from.isArray());
    if (!to.isNull()) {
      // ANY search
      TRI_ASSERT(to.isArray());
      TransactionBuilderLeaser fromBuilder(trx);
      std::unique_ptr<VPackBuilder> fromKeys(fromBuilder.steal());
      fromKeys->add(from);
      auto left = std::make_unique<EdgeIndexIterator>(_collection, trx, mmdr, this, _edgesFrom, fromKeys);

      TransactionBuilderLeaser toBuilder(trx);
      std::unique_ptr<VPackBuilder> toKeys(toBuilder.steal());
      toKeys->add(to);
      auto right = std::make_unique<EdgeIndexIterator>(_collection, trx, mmdr, this, _edgesTo, toKeys);
      return new AnyDirectionEdgeIndexIterator(_collection, trx, mmdr, this, left.release(), right.release());
    }
    // OUTBOUND search
    TRI_ASSERT(to.isNull());
    TransactionBuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->add(from);
    return new EdgeIndexIterator(_collection, trx, mmdr, this, _edgesFrom, keys);
  } else {
    // INBOUND search
    TRI_ASSERT(to.isArray());
    TransactionBuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->add(to);
    return new EdgeIndexIterator(_collection, trx, mmdr, this, _edgesTo, keys);
  }
}

/// @brief create the iterator
IndexIterator* EdgeIndex::createEqIterator(
    arangodb::Transaction* trx, 
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  
  // lease builder, but immediately pass it to the unique_ptr so we don't leak  
  TransactionBuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  handleValNode(keys.get(), valNode);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  // _from or _to?
  bool const isFrom = (attrNode->stringEquals(StaticStrings::FromString));

  return new EdgeIndexIterator(_collection, trx, mmdr, this, isFrom ? _edgesFrom : _edgesTo, keys);
}

/// @brief create the iterator
IndexIterator* EdgeIndex::createInIterator(
    arangodb::Transaction* trx, 
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  
  // lease builder, but immediately pass it to the unique_ptr so we don't leak  
  TransactionBuilderLeaser builder(trx);
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

  return new EdgeIndexIterator(_collection, trx, mmdr, this, isFrom ? _edgesFrom : _edgesTo, keys);
}

/// @brief add a single value node to the iterator's keys
void EdgeIndex::handleValNode(VPackBuilder* keys,
                              arangodb::aql::AstNode const* valNode) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(StaticStrings::IndexEq, VPackValuePair(valNode->getStringValue(), valNode->getStringLength(), VPackValueType::String));
  keys->close();
  
  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

SimpleIndexElement EdgeIndex::buildFromElement(TRI_voc_rid_t revisionId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(Transaction::extractFromFromDocument(doc));
  TRI_ASSERT(value.isString());
  return SimpleIndexElement(revisionId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}

SimpleIndexElement EdgeIndex::buildToElement(TRI_voc_rid_t revisionId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(Transaction::extractToFromDocument(doc));
  TRI_ASSERT(value.isString());
  return SimpleIndexElement(revisionId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}
