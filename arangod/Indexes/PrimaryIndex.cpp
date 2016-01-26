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

#include "PrimaryIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"

using namespace arangodb;

static inline uint64_t HashKey(void* userData, char const* key) {
  return TRI_FnvHashString(key);
}

static inline uint64_t HashElement(void* userData,
                                   TRI_doc_mptr_t const* element) {
  return element->_hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement(void* userData, char const* key,
                              uint64_t const hash,
                              TRI_doc_mptr_t const* element) {
  return (hash == element->_hash &&
          strcmp(key, TRI_EXTRACT_MARKER_KEY(element)) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement(void* userData, TRI_doc_mptr_t const* left,
                                  TRI_doc_mptr_t const* right) {
  return left->_hash == right->_hash &&
         strcmp(TRI_EXTRACT_MARKER_KEY(left), TRI_EXTRACT_MARKER_KEY(right)) ==
             0;
}



TRI_doc_mptr_t* PrimaryIndexIterator::next() {
  while (true) {
    if (_position >= _keys.size()) {
      // we're at the end of the lookup values
      return nullptr;
    }

    auto result = _index->lookupKey(_trx, _keys[_position++]);

    if (result != nullptr) {
      // found a result
      return result;
    }

    // found no result. now go to next lookup value in _keys
  }
}

void PrimaryIndexIterator::reset() { _position = 0; }



PrimaryIndex::PrimaryIndex(TRI_document_collection_t* collection)
    : Index(0, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{{TRI_VOC_ATTRIBUTE_KEY, false}}}),
            true, false),
      _primaryIndex(nullptr) {
  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info.indexBuckets();
  }

  _primaryIndex = new TRI_PrimaryIndex_t(
      HashKey, HashElement, IsEqualKeyElement, IsEqualElementElement,
      IsEqualElementElement, indexBuckets,
      []() -> std::string { return "primary"; });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

PrimaryIndex::PrimaryIndex(VPackSlice const& slice)
    : Index(slice), _primaryIndex(nullptr) {}

PrimaryIndex::~PrimaryIndex() { delete _primaryIndex; }


////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of documents from the index
////////////////////////////////////////////////////////////////////////////////

size_t PrimaryIndex::size() const { return _primaryIndex->size(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory usage of the index
////////////////////////////////////////////////////////////////////////////////

size_t PrimaryIndex::memory() const { return _primaryIndex->memoryUsage(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::toVelocyPack(
    VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  // hard-coded
  builder.add("unique", VPackValue(true));
  builder.add("sparse", VPackValue(false));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index figures
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::toVelocyPackFigures(
    VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  _primaryIndex->appendToVelocyPack(builder);
}

int PrimaryIndex::insert(arangodb::Transaction*, TRI_doc_mptr_t const*,
                         bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

int PrimaryIndex::remove(arangodb::Transaction*, TRI_doc_mptr_t const*,
                         bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupKey(arangodb::Transaction* trx,
                                        char const* key) const {
  return _primaryIndex->findByKey(trx, key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
/// returns the index position into which a key would belong in the second
/// parameter. also returns the hash value for the object
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupKey(
    arangodb::Transaction* trx, char const* key,
    arangodb::basics::BucketPosition& position, uint64_t& hash) const {
  return _primaryIndex->findByKey(trx, key, position, hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a random order.
///        Returns nullptr if all documents have been returned.
///        Convention: step === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupRandom(
    arangodb::Transaction* trx,
    arangodb::basics::BucketPosition& initialPosition,
    arangodb::basics::BucketPosition& position, uint64_t& step,
    uint64_t& total) {
  return _primaryIndex->findRandom(trx, initialPosition, position, step, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequential(
    arangodb::Transaction* trx,
    arangodb::basics::BucketPosition& position, uint64_t& total) {
  return _primaryIndex->findSequential(trx, position, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        reversed sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequentialReverse(
    arangodb::Transaction* trx,
    arangodb::basics::BucketPosition& position) {
  return _primaryIndex->findSequentialReverse(trx, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey(arangodb::Transaction* trx,
                            TRI_doc_mptr_t* header, void const** found) {
  *found = nullptr;
  int res = _primaryIndex->insert(trx, header);

  if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    *found = _primaryIndex->find(trx, header);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized version that receives the target slot index
/// from a previous lookupKey call
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey(arangodb::Transaction* trx,
                            TRI_doc_mptr_t* header,
                            arangodb::basics::BucketPosition const& position) {
  return _primaryIndex->insertAtPosition(trx, header, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element from the index
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::removeKey(arangodb::Transaction* trx,
                                        char const* key) {
  return _primaryIndex->removeByKey(trx, key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::resize(arangodb::Transaction* trx,
                         size_t targetSize) {
  return _primaryIndex->resize(trx, targetSize);
}

uint64_t PrimaryIndex::calculateHash(arangodb::Transaction* trx,
                                     char const* key) {
  return HashKey(trx, key);
}

uint64_t PrimaryIndex::calculateHash(arangodb::Transaction* trx,
                                     char const* key, size_t length) {
  return TRI_FnvHashPointer(static_cast<void const*>(key), length);
}

void PrimaryIndex::invokeOnAllElements(
    std::function<void(TRI_doc_mptr_t*)> work) {
  _primaryIndex->invokeOnAllElements(work);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the index supports the condition
////////////////////////////////////////////////////////////////////////////////

bool PrimaryIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(
      {{arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_ID, false)},
       {arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_KEY, false)}});

  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given Condition
////////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::Ast* ast, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  SimpleAttributeEqualityMatcher matcher(
      {{arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_ID, false)},
       {arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_KEY, false)}});

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
    return createIterator(
        trx, context, attrNode,
        std::vector<arangodb::aql::AstNode const*>({valNode}));
  } else if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      return nullptr;
    }

    std::vector<arangodb::aql::AstNode const*> valNodes;
    size_t const n = valNode->numMembers();
    valNodes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      valNodes.emplace_back(valNode->getMemberUnchecked(i));
      TRI_IF_FAILURE("PrimaryIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    return createIterator(trx, context, attrNode, valNodes);
  }

  // operator type unsupported
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* PrimaryIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(
      {{arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_ID, false)},
       {arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_KEY, false)}});

  return matcher.specializeOne(this, node, reference);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////

IndexIterator* PrimaryIndex::createIterator(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* attrNode,
    std::vector<arangodb::aql::AstNode const*> const& valNodes) const {
  // _key or _id?
  bool const isId =
      (strcmp(attrNode->getStringValue(), TRI_VOC_ATTRIBUTE_ID) == 0);

  // only leave the valid elements in the vector
  size_t const n = valNodes.size();
  std::vector<char const*> keys;
  keys.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    auto valNode = valNodes[i];

    if (!valNode->isStringValue()) {
      continue;
    }
    if (valNode->getStringLength() == 0) {
      continue;
    }

    if (isId) {
      // lookup by _id. now validate if the lookup is performed for the
      // correct collection (i.e. _collection)
      TRI_voc_cid_t cid;
      char const* key;
      int res = context->resolveId(valNode->getStringValue(), cid, key);

      if (res != TRI_ERROR_NO_ERROR) {
        continue;
      }

      TRI_ASSERT(cid != 0);
      TRI_ASSERT(key != nullptr);

      if (!context->isCluster() && cid != _collection->_info.id()) {
        // only continue lookup if the id value is syntactically correct and
        // refers to "our" collection, using local collection id
        continue;
      }

      if (context->isCluster() && cid != _collection->_info.planId()) {
        // only continue lookup if the id value is syntactically correct and
        // refers to "our" collection, using cluster collection id
        continue;
      }

      // use _key value from _id
      keys.push_back(key);
    } else {
      keys.emplace_back(valNode->getStringValue());
    }
  }

  if (keys.empty()) {
    // nothing to do: still new to return an empty iterator
    return new PrimaryIndexIterator(trx, this, keys);
  }

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return new PrimaryIndexIterator(trx, this, keys);
}


