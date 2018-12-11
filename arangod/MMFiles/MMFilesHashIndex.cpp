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

#include "MMFilesHashIndex.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/FixedSizeAllocator.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/SmallVector.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

MMFilesHashIndexLookupBuilder::MMFilesHashIndexLookupBuilder(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields)
    : _builder(trx), 
      _usesIn(false), 
      _isEmpty(false),
      _mappingFieldCondition{_mappingFieldConditionArena},
      _inStorage(trx) {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  _coveredFields = fields.size();
  TRI_ASSERT(node->numMembers() == _coveredFields);

  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>>
      paramPair;
  
  _mappingFieldCondition.reserve(_coveredFields);

  SmallVector<size_t>::allocator_type::arena_type a;
  SmallVector<size_t> storageOrder{a};

  for (size_t i = 0; i < _coveredFields; ++i) {
    auto comp = node->getMemberUnchecked(i);
    auto attrNode = comp->getMember(0);
    auto valNode = comp->getMember(1);

    if (!attrNode->isAttributeAccessForVariable(paramPair) ||
        paramPair.first != reference) {
      attrNode = comp->getMember(1);
      valNode = comp->getMember(0);

      if (!attrNode->isAttributeAccessForVariable(paramPair) ||
          paramPair.first != reference) {
        _isEmpty = true;
        return;
      }
    }

    for (size_t j = 0; j < fields.size(); ++j) {
      if (arangodb::basics::AttributeName::isIdentical(
              fields[j], paramPair.second, true)) {
        if (TRI_AttributeNamesHaveExpansion(fields[j])) {
          TRI_IF_FAILURE("HashIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          _mappingFieldCondition.push_back(valNode);
        } else {
          TRI_IF_FAILURE("HashIndex::permutationEQ") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          arangodb::aql::AstNodeType type = comp->type;
          if (type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
            if (!_usesIn) {
              _inStorage->openArray();
            }
            valNode->toVelocyPackValue(*(_inStorage.get()));
            _inPosition.emplace(
                j,
                std::make_pair(0, std::vector<arangodb::velocypack::Slice>()));
            _usesIn = true;
            storageOrder.emplace_back(j);
            _mappingFieldCondition.push_back(nullptr);
          } else {
            _mappingFieldCondition.push_back(valNode);
          }
        }
        break;
      }
    }
  }
  
  TRI_ASSERT(_mappingFieldCondition.size() == _coveredFields);

  if (_usesIn) {
    _inStorage->close();
    arangodb::basics::VelocyPackHelper::VPackLess<true> sorter;
    std::unordered_set<VPackSlice,
                       arangodb::basics::VelocyPackHelper::VPackHash,
                       arangodb::basics::VelocyPackHelper::VPackEqual>
        tmp(16, arangodb::basics::VelocyPackHelper::VPackHash(),
            arangodb::basics::VelocyPackHelper::VPackEqual());
    VPackSlice storageSlice = _inStorage->slice();
    auto f = storageOrder.begin();
    for (auto const& values : VPackArrayIterator(storageSlice)) {
      tmp.clear();
      TRI_IF_FAILURE("Index::permutationIN") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      if (values.isArray()) {
        for (auto const& value : VPackArrayIterator(values)) {
          tmp.emplace(value);
        }
      }
      if (tmp.empty()) {
        // IN [] short-circuit, cannot be fullfilled;
        _isEmpty = true;
        return;
      }
      // Now the elements are unique
      auto& vector = _inPosition.find(*f)->second.second;
      vector.insert(vector.end(), tmp.begin(), tmp.end());
      std::sort(vector.begin(), vector.end(), sorter);
      f++;
    }
  }
  buildNextSearchValue();
}

VPackSlice MMFilesHashIndexLookupBuilder::lookup() { return _builder->slice(); }

bool MMFilesHashIndexLookupBuilder::hasAndGetNext() {
  _builder->clear();
  if (!_usesIn || _isEmpty) {
    return false;
  }
  if (!incrementInPosition()) {
    return false;
  }
  buildNextSearchValue();
  return true;
}

void MMFilesHashIndexLookupBuilder::reset() {
  if (_isEmpty) {
    return;
  }
  if (_usesIn) {
    for (auto& it : _inPosition) {
      it.second.first = 0;
    }
  }
  buildNextSearchValue();
}

bool MMFilesHashIndexLookupBuilder::incrementInPosition() {
  size_t i = _coveredFields - 1;
  while (true) {
    auto it = _inPosition.find(i);
    if (it != _inPosition.end()) {
      it->second.first++;
      if (it->second.first == it->second.second.size()) {
        //  Reached end of this array. start form begining.
        //  Increment another array.
        it->second.first = 0;
      } else {
        return true;
      }
    }
    if (i == 0) {
      return false;
    }
    --i;
  }
}

void MMFilesHashIndexLookupBuilder::buildNextSearchValue() {
  if (_isEmpty) {
    return;
  }
  TRI_ASSERT(_mappingFieldCondition.size() >= _coveredFields);

  _builder->openArray(true);
  if (!_usesIn) {
    // Fast path, do no search and checks
    for (size_t i = 0; i < _coveredFields; ++i) {
      TRI_ASSERT(_mappingFieldCondition[i] != nullptr);
      _mappingFieldCondition[i]->toVelocyPackValue(*(_builder.get()));
    }
  } else {
    for (size_t i = 0; i < _coveredFields; ++i) {
      auto in = _inPosition.find(i);
      if (in != _inPosition.end()) {
        TRI_ASSERT(_mappingFieldCondition[i] == nullptr);
        _builder->add(in->second.second[in->second.first]);
      } else {
        _mappingFieldCondition[i]->toVelocyPackValue(*(_builder.get()));
      }
    }
  }
  _builder->close();  // End of search Array
}

MMFilesHashIndexIterator::MMFilesHashIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    MMFilesHashIndex const* index,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference)
    : IndexIterator(collection, trx),
      _index(index),
      _lookups(trx, node, reference, index->fields()),
      _buffer(),
      _posInBuffer(0) {
  _index->lookup(_trx, _lookups.lookup(), _buffer);
}

bool MMFilesHashIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  while (limit > 0) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_lookups.hasAndGetNext()) {
        // we're at the end of the lookup values
        return false;
      }

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      _index->lookup(_trx, _lookups.lookup(), _buffer);
    }

    if (!_buffer.empty()) {
      // found something
      TRI_ASSERT(_posInBuffer < _buffer.size());
      cb(LocalDocumentId{_buffer[_posInBuffer++]->localDocumentId()});
      --limit;
    }
  }
  return true;
}

bool MMFilesHashIndexIterator::nextDocument(DocumentCallback const& cb, size_t limit) {
  _documentIds.clear();
  _documentIds.reserve(limit);

  bool done = false;
  while (limit > 0) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_lookups.hasAndGetNext()) {
        // we're at the end of the lookup values
        done = true;
        break;
      }

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      _index->lookup(_trx, _lookups.lookup(), _buffer);
    }

    if (!_buffer.empty()) {
      // found something
      TRI_ASSERT(_posInBuffer < _buffer.size());
      _documentIds.emplace_back(std::make_pair(_buffer[_posInBuffer++]->localDocumentId(), nullptr));
      --limit;
    }
  }
  
  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  physical->readDocumentWithCallback(_trx, _documentIds, cb);
  return !done;
}

void MMFilesHashIndexIterator::reset() {
  _buffer.clear();
  _documentIds.clear();
  _posInBuffer = 0;
  _lookups.reset();
  _index->lookup(_trx, _lookups.lookup(), _buffer);
}

/// @brief create the unique array
MMFilesHashIndex::UniqueArray::UniqueArray(
    size_t numPaths, std::unique_ptr<TRI_HashArray_t> hashArray)
    : _hashArray(std::move(hashArray)),
      _numPaths(numPaths) {
  TRI_ASSERT(_hashArray != nullptr);
}

/// @brief create the multi array
MMFilesHashIndex::MultiArray::MultiArray(
    size_t numPaths, std::unique_ptr<TRI_HashArrayMulti_t> hashArray)
    : _hashArray(std::move(hashArray)),
      _numPaths(numPaths) {
  TRI_ASSERT(_hashArray != nullptr);
}

MMFilesHashIndex::MMFilesHashIndex(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
)
    : MMFilesPathBasedIndex(iid, collection, info,
                            sizeof(LocalDocumentId) + sizeof(uint32_t), false),
      _uniqueArray(nullptr) {
  auto physical = static_cast<MMFilesCollection*>(collection.getPhysical());

  TRI_ASSERT(physical != nullptr);
  size_t indexBuckets = static_cast<size_t>(physical->indexBuckets());

  if (collection.isAStub()) {
    // in order to reduce memory usage
    indexBuckets = 1;
  }

  if (_unique) {
    auto array = std::make_unique<TRI_HashArray_t>(
        MMFilesUniqueHashIndexHelper(_paths.size(), _useExpansion),
        indexBuckets,
        [this]() -> std::string { return this->context(); });

    _uniqueArray = new MMFilesHashIndex::UniqueArray(numPaths(), std::move(array));
  } else {
    auto array = std::make_unique<TRI_HashArrayMulti_t>(
        MMFilesMultiHashIndexHelper(_paths.size(), _useExpansion),
        indexBuckets, 64,
        [this]() -> std::string { return this->context(); });

    _multiArray = new MMFilesHashIndex::MultiArray(numPaths(), std::move(array));
  }
}

/// @brief destroys the index
MMFilesHashIndex::~MMFilesHashIndex() {
  if (_unique) {
    delete _uniqueArray;
  } else {
    delete _multiArray;
  }
}

/// @brief returns a selectivity estimate for the index
double MMFilesHashIndex::selectivityEstimate(StringRef const&) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  if (_multiArray == nullptr) {
    return 0.1;
  }
  return _multiArray->_hashArray->selectivity();
}

/// @brief returns the index memory usage
size_t MMFilesHashIndex::memory() const {
  size_t elementSize = MMFilesHashIndexElement::baseMemoryUsage(_paths.size());

  if (_unique) {
    return static_cast<size_t>(elementSize * _uniqueArray->_hashArray->size() +
                               _uniqueArray->_hashArray->memoryUsage());
  }
  return static_cast<size_t>(elementSize * _multiArray->_hashArray->size() +
                             _multiArray->_hashArray->memoryUsage());
}

/// @brief return a velocypack representation of the index figures
void MMFilesHashIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  MMFilesPathBasedIndex::toVelocyPackFigures(builder);
  if (_unique) {
    _uniqueArray->_hashArray->appendToVelocyPack(builder);
  } else {
    _multiArray->_hashArray->appendToVelocyPack(builder);
  }
}

/// @brief Test if this index matches the definition
bool MMFilesHashIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
  }

  value = info.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }

  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexUnique, false
                 )
     ) {
    return false;
  }

  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexSparse, false
                 )
     ) {
    return false;
  }

  // This check does not take ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (auto const& f : VPackArrayIterator(value)) {
    bool found = false;
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    translate.clear();
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);

    for (size_t i = 0; i < n; ++i) {
      if (arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                       false)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

Result MMFilesHashIndex::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  if (_unique) {
    return insertUnique(&trx, documentId, doc, mode);
  }

  return insertMulti(&trx, documentId, doc, mode);
}

/// @brief removes an entry from the hash array part of the hash index
Result MMFilesHashIndex::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;
  std::vector<MMFilesHashIndexElement*> elements;
  int r = fillElement<MMFilesHashIndexElement>(elements, documentId, doc);

  if (r != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      _allocator->deallocate(hashElement);
    }
    return addErrorMsg(res, r);
  }

  for (auto& hashElement : elements) {
    int result;
    if (_unique) {
      result = removeUniqueElement(&trx, hashElement, mode);
    } else {
      result = removeMultiElement(&trx, hashElement, mode);
    }

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (result != TRI_ERROR_NO_ERROR) {
      addErrorMsg(res, result);
    }
    _allocator->deallocate(hashElement);
  }

  return res;
}

void MMFilesHashIndex::batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, velocypack::Slice>> const& documents,
    std::shared_ptr<basics::LocalTaskQueue> queue
) {
  TRI_ASSERT(queue != nullptr);
  if (_unique) {
    batchInsertUnique(&trx, documents, queue);
  } else {
    batchInsertMulti(&trx, documents, queue);
  }
}

void MMFilesHashIndex::unload() {
  if (_unique) {
    _uniqueArray->_hashArray->truncate(
        [](MMFilesHashIndexElement*) -> bool { return true; });
  } else {
    _multiArray->_hashArray->truncate(
        [](MMFilesHashIndexElement*) -> bool { return true; });
  }
  _allocator->deallocateAll();
}

/// @brief provides a size hint for the hash index
Result MMFilesHashIndex::sizeHint(transaction::Methods& trx, size_t size) {
  if (_sparse) {
    // for sparse indexes, we assume that we will have less index entries
    // than if the index would be fully populated
    size /= 5;
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, numPaths());

  if (_unique) {
    return _uniqueArray->_hashArray->resize(&context, size);
  }

  return _multiArray->_hashArray->resize(&context, size);
}

/// @brief locates entries in the hash index given VelocyPack slices
int MMFilesHashIndex::lookup(
    transaction::Methods* trx, VPackSlice key,
    std::vector<MMFilesHashIndexElement*>& documents) const {
  if (key.isNone()) {
    return TRI_ERROR_NO_ERROR;
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, numPaths());

  if (_unique) {
    MMFilesHashIndexElement* found =
        _uniqueArray->_hashArray->findByKey(&context, &key);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(found);
    }

    return TRI_ERROR_NO_ERROR;
  }

  documents.clear();
  try {
    _multiArray->_hashArray->lookupByKey(&context, &key, documents);
  } catch (std::bad_alloc const&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
  return TRI_ERROR_NO_ERROR;
}

Result MMFilesHashIndex::insertUnique(transaction::Methods* trx,
                                      LocalDocumentId const& documentId,
                                      VPackSlice const& doc,
                                      OperationMode mode) {
  Result res;
  
  std::vector<MMFilesHashIndexElement*> elements;
  int r = fillElement<MMFilesHashIndexElement>(elements, documentId, doc);

  if (r != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      _allocator->deallocate(it);
    }
    return addErrorMsg(res, r);
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, numPaths());

  auto work = [this, &context](MMFilesHashIndexElement* element,
                               OperationMode) -> int {
    TRI_IF_FAILURE("InsertHashIndex") { return TRI_ERROR_DEBUG; }
    return _uniqueArray->_hashArray->insert(&context, element);
  };

  size_t const n = elements.size();

  for (size_t i = 0; i < n; ++i) {
    auto hashElement = elements[i];
    r = work(hashElement, mode);

    if (r != TRI_ERROR_NO_ERROR) {
      
      if (r == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        LocalDocumentId rev(_uniqueArray->_hashArray->find(&context, hashElement)->localDocumentId());
        std::string existingId;

        _collection.getPhysical()->readDocumentWithCallback(trx, rev, [&existingId](LocalDocumentId const&, VPackSlice doc) {
          existingId = doc.get(StaticStrings::KeyString).copyString();
        });

        if (mode == OperationMode::internal) {
          res.reset(r, std::move(existingId));
        } else {
          addErrorMsg(res, r, existingId);
        }
      } else {
        addErrorMsg(res, r);
      }

      for (size_t j = i; j < n; ++j) {
        // Free all elements that are not yet in the index
        _allocator->deallocate(elements[j]);
      }
      
      break; // Already indexed elements will be removed by the rollback
    }
  }

  return res;
}

void MMFilesHashIndex::batchInsertUnique(
    transaction::Methods* trx,
    std::vector<std::pair<LocalDocumentId, VPackSlice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  TRI_ASSERT(queue != nullptr);
  std::shared_ptr<std::vector<MMFilesHashIndexElement*>> elements;
  elements.reset(new std::vector<MMFilesHashIndexElement*>());
  elements->reserve(documents.size());

  // TODO: create parallel tasks for this
  for (auto& doc : documents) {
    int res = fillElement<MMFilesHashIndexElement>(*(elements.get()), doc.first,
                                                   doc.second);

    if (res != TRI_ERROR_NO_ERROR) {
      for (auto& it : *(elements.get())) {
        // free all elements to prevent leak
        _allocator->deallocate(it);
      }
      queue->setStatus(res);
      return;
    }
  }

  if (elements->empty()) {
    // no elements left to insert
    return;
  }

  // functions that will be called for each thread
  auto creator = [&trx, this]() -> void* {
    ManagedDocumentResult* result = new ManagedDocumentResult;

    return new MMFilesIndexLookupContext(trx, &_collection, result, numPaths());
  };
  auto destroyer = [](void* userData) {
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };

  // queue the actual insertion tasks
  _uniqueArray->_hashArray->batchInsert(creator, destroyer, elements, queue);

  // queue cleanup callback
  auto allocator = _allocator.get();
  auto callback = [elements, queue, allocator]() -> void {
    if (queue->status() != TRI_ERROR_NO_ERROR) {
      for (auto& it : *(elements.get())) {
        // free all elements to prevent leak
        allocator->deallocate(it);
      }
    }
  };
  std::shared_ptr<arangodb::basics::LocalCallbackTask> cbTask;
  cbTask.reset(new arangodb::basics::LocalCallbackTask(queue, callback));
  queue->enqueueCallback(cbTask);
}

Result MMFilesHashIndex::insertMulti(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     VPackSlice const& doc, OperationMode mode) {
  Result res;
  
  std::vector<MMFilesHashIndexElement*> elements;
  int r = fillElement<MMFilesHashIndexElement>(elements, documentId, doc);

  if (r != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      _allocator->deallocate(hashElement);
    }

    return res.reset(r);
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, numPaths());
  auto work = [this, &context](MMFilesHashIndexElement*& element,
                               OperationMode) {
    TRI_IF_FAILURE("InsertHashIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    MMFilesHashIndexElement* found =
        _multiArray->_hashArray->insert(&context, element, false, true);

    if (found != nullptr) {
      // already got the exact same index entry. now free our local element...
      _allocator->deallocate(element);
    }
  };

  size_t const n = elements.size();

  for (size_t i = 0; i < n; ++i) {
    auto hashElement = elements[i];

    try {
      work(hashElement, mode);
    } catch (arangodb::basics::Exception const& ex) {
      r = ex.code();
    } catch (std::bad_alloc const&) {
      r = TRI_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      r = TRI_ERROR_INTERNAL;
    }

    if (r != TRI_ERROR_NO_ERROR) {
      for (size_t j = i; j < n; ++j) {
        // Free all elements that are not yet in the index
        _allocator->deallocate(elements[j]);
      }
      for (size_t j = 0; j < i; ++j) {
        // Remove all already indexed elements and free them
        if (elements[j] != nullptr) {
          removeMultiElement(trx, elements[j], mode);
        }
      }
      return addErrorMsg(res, r);
    }
  }

  return res;
}

void MMFilesHashIndex::batchInsertMulti(
    transaction::Methods* trx,
    std::vector<std::pair<LocalDocumentId, VPackSlice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  TRI_ASSERT(queue != nullptr);
  std::shared_ptr<std::vector<MMFilesHashIndexElement*>> elements;
  elements.reset(new std::vector<MMFilesHashIndexElement*>());
  elements->reserve(documents.size());

  // TODO: create parallel tasks for this
  for (auto& doc : documents) {
    int res = fillElement<MMFilesHashIndexElement>(*(elements.get()), doc.first,
                                                   doc.second);

    if (res != TRI_ERROR_NO_ERROR) {
      // Filling the elements failed for some reason. Assume loading as failed
      for (auto& el : *(elements.get())) {
        // Free all elements that are not yet in the index
        _allocator->deallocate(el);
      }
      return;
    }
  }

  if (elements->empty()) {
    // no elements left to insert
    return;
  }

  // functions that will be called for each thread
  auto creator = [&trx, this]() -> void* {
    ManagedDocumentResult* result = new ManagedDocumentResult;
    return new MMFilesIndexLookupContext(trx, &_collection, result, numPaths());
  };
  auto destroyer = [](void* userData) {
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };

  // queue actual insertion tasks
  _multiArray->_hashArray->batchInsert(creator, destroyer, elements, queue);

  // queue cleanup callback
  auto allocator = _allocator.get();
  auto callback = [elements, queue, allocator]() -> void {
    if (queue->status() != TRI_ERROR_NO_ERROR) {
      // free all elements to prevent leak
      for (auto& it : *(elements.get())) {
        allocator->deallocate(it);
      }
    }
  };
  std::shared_ptr<arangodb::basics::LocalCallbackTask> cbTask;
  cbTask.reset(new arangodb::basics::LocalCallbackTask(queue, callback));
  queue->enqueueCallback(cbTask);
}

int MMFilesHashIndex::removeUniqueElement(transaction::Methods* trx,
                                          MMFilesHashIndexElement* element,
                                          OperationMode mode) {
  TRI_IF_FAILURE("RemoveHashIndex") { return TRI_ERROR_DEBUG; }
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, numPaths());
  MMFilesHashIndexElement* old =
      _uniqueArray->_hashArray->remove(&context, element);

  if (old == nullptr) {
    // not found
    if (mode == OperationMode::rollback) {  // ignore in this case, because it
                                            // can happen
      return TRI_ERROR_NO_ERROR;
    }

    return TRI_ERROR_INTERNAL;
  }

  _allocator->deallocate(old);

  return TRI_ERROR_NO_ERROR;
}

int MMFilesHashIndex::removeMultiElement(transaction::Methods* trx,
                                         MMFilesHashIndexElement* element,
                                         OperationMode mode) {
  TRI_IF_FAILURE("RemoveHashIndex") { return TRI_ERROR_DEBUG; }
  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(trx, &_collection, &result, numPaths());
  MMFilesHashIndexElement* old =
      _multiArray->_hashArray->remove(&context, element);

  if (old == nullptr) {
    // not found
    if (mode == OperationMode::rollback) {  // ignore in this case, because it
                                            // can happen
      return TRI_ERROR_NO_ERROR;
    }

    return TRI_ERROR_INTERNAL;
  }

  _allocator->deallocate(old);

  return TRI_ERROR_NO_ERROR;
}

/// @brief checks whether the index supports the condition
bool MMFilesHashIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const&,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(_fields);
  return matcher.matchAll(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesHashIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult*,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_IF_FAILURE("HashIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return new MMFilesHashIndexIterator(&_collection, trx, this, node, reference);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* MMFilesHashIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(_fields);
  return matcher.specializeAll(this, node, reference);
}
