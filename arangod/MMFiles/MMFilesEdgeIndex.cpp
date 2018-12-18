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
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const
    IndexAttributes{{arangodb::basics::AttributeName("_from", false)},
                    {arangodb::basics::AttributeName("_to", false)}};

MMFilesEdgeIndexIterator::MMFilesEdgeIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mdr, arangodb::MMFilesEdgeIndex const* index,
    TRI_MMFilesEdgeIndexHash_t const* indexImpl,
    std::unique_ptr<VPackBuilder> keys)
    : IndexIterator(collection, trx),
      _index(indexImpl),
      _context(trx, collection, mdr, index->fields().size()),
      _keys(std::move(keys)),
      _iterator(_keys->slice()),
      _posInBuffer(0),
      _batchSize(1000),
      _lastElement() {
}

MMFilesEdgeIndexIterator::~MMFilesEdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool MMFilesEdgeIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
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
      TRI_ASSERT(_posInBuffer < _buffer.size());
      cb(LocalDocumentId{_buffer[_posInBuffer++].localDocumentId()});
      limit--;
    }
  }
  return true;
}

bool MMFilesEdgeIndexIterator::nextDocument(DocumentCallback const& cb, size_t limit) {
  _documentIds.clear();
  _documentIds.reserve(limit);

  if (limit == 0 || (_buffer.empty() && !_iterator.valid())) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  bool done = false;
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
        done = true;
        break;
      }
    } else {
      _lastElement = _buffer.back();
      // found something
      TRI_ASSERT(_posInBuffer < _buffer.size());
      _documentIds.emplace_back(std::make_pair(_buffer[_posInBuffer++].localDocumentId(), nullptr));
      limit--;
    }
  }
  
  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  physical->readDocumentWithCallback(_trx, _documentIds, cb);
  return !done;
}

void MMFilesEdgeIndexIterator::reset() {
  _posInBuffer = 0;
  _buffer.clear();
  _iterator.reset();
  _lastElement = MMFilesSimpleIndexElement();
}

MMFilesEdgeIndex::MMFilesEdgeIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
)
    : MMFilesIndex(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::FromString,
                                                  false)},
                 {arangodb::basics::AttributeName(StaticStrings::ToString,
                                                  false)}}),
            false, false) {
  TRI_ASSERT(iid != 0);
  size_t initialSize = 64;
  auto physical = static_cast<MMFilesCollection*>(collection.getPhysical());
  TRI_ASSERT(physical != nullptr);
  size_t indexBuckets = static_cast<size_t>(physical->indexBuckets());

  if (collection.isAStub()) {
    // in order to reduce memory usage
    indexBuckets = 1;
    initialSize = 4;
  }

  auto context = [this]() -> std::string { return this->context(); };

  _edgesFrom.reset(new TRI_MMFilesEdgeIndexHash_t(MMFilesEdgeIndexHelper(), indexBuckets, static_cast<uint32_t>(initialSize), context));
  _edgesTo.reset(new TRI_MMFilesEdgeIndexHash_t(MMFilesEdgeIndexHelper(), indexBuckets, static_cast<uint32_t>(initialSize), context));
}

/// @brief return a selectivity estimate for the index
double MMFilesEdgeIndex::selectivityEstimate(
    arangodb::StringRef const& attribute) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  if (_edgesFrom == nullptr || _edgesTo == nullptr) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return 0.1;
  }

  if (!attribute.empty()) {
    // the index attribute is given here
    // now check if we can restrict the selectivity estimation to the correct
    // part of the index
    if (attribute.compare(StaticStrings::FromString) == 0) {
      // _from
      return _edgesFrom->selectivity();
    } else if (attribute.compare(StaticStrings::ToString) == 0) {
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
void MMFilesEdgeIndex::toVelocyPack(VPackBuilder& builder,
       std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  // hard-coded
  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(false)
  );
  builder.add(
    arangodb::StaticStrings::IndexSparse,
    arangodb::velocypack::Value(false)
  );
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
}

Result MMFilesEdgeIndex::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;
  MMFilesSimpleIndexElement fromElement(buildFromElement(documentId, doc));
  MMFilesSimpleIndexElement toElement(buildToElement(documentId, doc));
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, 1);

  _edgesFrom->insert(&context, fromElement, true,
                     mode == OperationMode::rollback);

  try {
    _edgesTo->insert(&context, toElement, true,
                    mode == OperationMode::rollback);
  } catch (std::bad_alloc const&) {
    // roll back partial insert
    _edgesFrom->remove(&context, fromElement);
    res.reset(TRI_ERROR_OUT_OF_MEMORY);
    return addErrorMsg(res);
  } catch (...) {
    // roll back partial insert
    _edgesFrom->remove(&context, fromElement);
    res.reset(TRI_ERROR_INTERNAL);
    return addErrorMsg(res);
  }

  return res;
}

Result MMFilesEdgeIndex::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;
  MMFilesSimpleIndexElement fromElement(buildFromElement(documentId, doc));
  MMFilesSimpleIndexElement toElement(buildToElement(documentId, doc));
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, 1);

  try {
    _edgesFrom->remove(&context, fromElement);
    _edgesTo->remove(&context, toElement);

    return res;
  } catch (...) {
    if (mode == OperationMode::rollback) {
      return res;
    }
    res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    return addErrorMsg(res);
  }
}

void MMFilesEdgeIndex::batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, VPackSlice>> const& documents,
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

    return new MMFilesIndexLookupContext(&trx, &_collection, result, 1);
  };
  auto destroyer = [](void* userData) {
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };

  // TODO: create parallel tasks for this

  // _from
  for (auto const& it : documents) {
    VPackSlice value(transaction::helpers::extractFromFromDocument(it.second));
    fromElements->emplace_back(it.first, value,
        static_cast<uint32_t>(value.begin() - it.second.begin()));
  }

  // _to
  for (auto const& it : documents) {
    VPackSlice value(transaction::helpers::extractToFromDocument(it.second));
    toElements->emplace_back(it.first, value,
        static_cast<uint32_t>(value.begin() - it.second.begin()));
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
Result MMFilesEdgeIndex::sizeHint(transaction::Methods& trx, size_t size) {
  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesFrom->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, 1);
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
    std::vector<std::shared_ptr<arangodb::Index>> const&,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
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
    return createEqIterator(trx, mdr, attrNode, valNode);
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      // a.b IN non-array
      return new EmptyIndexIterator(&_collection, trx);
    }

    return createInIterator(trx, mdr, attrNode, valNode);
  }

  // operator type unsupported
  return new EmptyIndexIterator(&_collection, trx);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* MMFilesEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief create the iterator
IndexIterator* MMFilesEdgeIndex::createEqIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr,
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

  return new MMFilesEdgeIndexIterator(
    &_collection,
    trx,
    mdr,
    this,
    isFrom ? _edgesFrom.get() : _edgesTo.get(),
    std::move(keys)
  );
}

/// @brief create the iterator
IndexIterator* MMFilesEdgeIndex::createInIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr,
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

  return new MMFilesEdgeIndexIterator(
    &_collection,
    trx,
    mdr,
    this,
    isFrom ? _edgesFrom.get() : _edgesTo.get(),
    std::move(keys)
  );
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
    LocalDocumentId const& documentId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(transaction::helpers::extractFromFromDocument(doc));
  TRI_ASSERT(value.isString());
  return MMFilesSimpleIndexElement(
      documentId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}

MMFilesSimpleIndexElement MMFilesEdgeIndex::buildToElement(
    LocalDocumentId const& documentId, VPackSlice const& doc) const {
  TRI_ASSERT(doc.isObject());
  VPackSlice value(transaction::helpers::extractToFromDocument(doc));
  TRI_ASSERT(value.isString());
  return MMFilesSimpleIndexElement(
      documentId, value, static_cast<uint32_t>(value.begin() - doc.begin()));
}
