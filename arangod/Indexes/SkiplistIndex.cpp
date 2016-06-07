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

#include "SkiplistIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/Transaction.h"
#include "VocBase/document-collection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

static size_t sortWeight(arangodb::aql::AstNode const* node) {
  switch (node->type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      return 1;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
      return 2;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      return 3;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      return 4;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      return 5;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      return 6;
    default:
      return 42;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an element in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void FreeElm(void* e) {
  auto element = static_cast<TRI_index_element_t*>(e);
  TRI_index_element_t::freeElement(element);
}

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

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key with an element, version with proper types
////////////////////////////////////////////////////////////////////////////////

static int CompareKeyElement(VPackSlice const* left,
                             TRI_index_element_t const* right,
                             size_t rightPosition) {
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);
  auto rightSubobjects = right->subObjects();
  return arangodb::basics::VelocyPackHelper::compare(*left,
      rightSubobjects[rightPosition].slice(right->document()), true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares elements, version with proper types
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement(TRI_index_element_t const* left,
                                 size_t leftPosition,
                                 TRI_index_element_t const* right,
                                 size_t rightPosition) {
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);

  auto leftSubobjects = left->subObjects();
  auto rightSubobjects = right->subObjects();
  VPackSlice l = leftSubobjects[leftPosition].slice(left->document());
  VPackSlice r = rightSubobjects[rightPosition].slice(right->document());
  return arangodb::basics::VelocyPackHelper::compare(l, r, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reset the cursor
////////////////////////////////////////////////////////////////////////////////

void SkiplistIterator::reset() {
  if (_reverse) {
    _cursor = _rightEndPoint;
  } else {
    _cursor = _leftEndPoint;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next element in the skiplist
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* SkiplistIterator::next() {
  if (_cursor == nullptr) {
    // We are exhausted already, sorry
    return nullptr;
  }
  Node* tmp = _cursor;
  if (_reverse) {
    if (_cursor == _leftEndPoint) {
      _cursor = nullptr;
    } else {
      _cursor = _cursor->prevNode();
    }
  } else {
    if (_cursor == _rightEndPoint) {
      _cursor = nullptr;
    } else {
      _cursor = _cursor->nextNode();
    }
  }
  TRI_ASSERT(tmp != nullptr);
  TRI_ASSERT(tmp->document() != nullptr);
  return tmp->document()->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the skiplist index
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex::SkiplistIndex(
    TRI_idx_iid_t iid, TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse)
    : PathBasedIndex(iid, collection, fields, unique, sparse, true),
      CmpElmElm(this),
      CmpKeyElm(this),
      _skiplistIndex(nullptr) {
  _skiplistIndex =
      new TRI_Skiplist(CmpElmElm, CmpKeyElm, FreeElm, unique, _useExpansion);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex::SkiplistIndex(VPackSlice const& slice)
    : PathBasedIndex(slice, true),
      CmpElmElm(this),
      CmpKeyElm(this),
      _skiplistIndex(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the skiplist index
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex::~SkiplistIndex() { delete _skiplistIndex; }

size_t SkiplistIndex::memory() const {
  return _skiplistIndex->memoryUsage() +
         static_cast<size_t>(_skiplistIndex->getNrUsed()) * elementSize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndex::toVelocyPack(VPackBuilder& builder,
                                 bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  builder.add("unique", VPackValue(_unique));
  builder.add("sparse", VPackValue(_sparse));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index figures
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("memory", VPackValue(memory()));
  _skiplistIndex->appendToVelocyPack(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a skiplist index
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex::insert(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool) {
  std::vector<TRI_index_element_t*> elements;

  int res;
  try {
    res = fillElement(elements, doc);
  } catch (...) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      TRI_index_element_t::freeElement(it);
    }

    return res;
  }

  // insert into the index. the memory for the element will be owned or freed
  // by the index

  size_t const count = elements.size();

  for (size_t i = 0; i < count; ++i) {
    res = _skiplistIndex->insert(elements[i]);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_index_element_t::freeElement(elements[i]);
      // Note: this element is freed already
      for (size_t j = i + 1; j < count; ++j) {
        TRI_index_element_t::freeElement(elements[j]);
      }
      for (size_t j = 0; j < i; ++j) {
        _skiplistIndex->remove(elements[j]);
        // No need to free elements[j] skiplist has taken over already
      }
    
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && !_unique) {
        // We ignore unique_constraint violated if we are not unique
        res = TRI_ERROR_NO_ERROR;
      }
      break;
    }
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex::remove(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool) {
  std::vector<TRI_index_element_t*> elements;

  int res;
  try {
    res = fillElement(elements, doc);
  } catch (...) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      TRI_index_element_t::freeElement(it);
    }

    return res;
  }

  // attempt the removal for skiplist indexes
  // ownership for the index element is transferred to the index

  size_t const count = elements.size();

  for (size_t i = 0; i < count; ++i) {
    int result = _skiplistIndex->remove(elements[i]);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (result != TRI_ERROR_NO_ERROR) {
      res = result;
    }

    TRI_index_element_t::freeElement(elements[i]);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the interval is valid. It is declared invalid if
///        one border is nullptr or the right is lower than left.
////////////////////////////////////////////////////////////////////////////////

bool SkiplistIndex::intervalValid(Node* left, Node* right) const {
  if (left == nullptr) {
    return false;
  }
  if (right == nullptr) {
    return false;
  }
  if (left == right) {
    // Exactly one result. Improve speed on unique indexes
    return true;
  }
  if (CmpElmElm(left->document(), right->document(),
                arangodb::basics::SKIPLIST_CMP_TOTORDER) > 0) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
///
/// Warning: who ever calls this function is responsible for destroying
/// the SkiplistIterator* results
////////////////////////////////////////////////////////////////////////////////

SkiplistIterator* SkiplistIndex::lookup(arangodb::Transaction* trx,
                                        VPackSlice const searchValues,
                                        bool reverse) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(searchValues.length() <= _fields.size());


  VPackBuilder leftSearch;
  VPackSlice lastNonEq;
  leftSearch.openArray();
  for (auto const& it : VPackArrayIterator(searchValues)) {
    TRI_ASSERT(it.isObject());
    VPackSlice eq = it.get(TRI_SLICE_KEY_EQUAL);
    if (eq.isNone()) {
      lastNonEq = it;
      break;
    }
    leftSearch.add(eq);
  }

  Node* leftBorder = nullptr;
  Node* rightBorder = nullptr;
  if (lastNonEq.isNone()) {
    // We only have equality!
    leftSearch.close();
    VPackSlice search = leftSearch.slice();
    rightBorder = _skiplistIndex->rightKeyLookup(&search);
    if (rightBorder == _skiplistIndex->startNode()) {
      // No matching elements
      rightBorder = nullptr;
      leftBorder = nullptr;
    } else {
      leftBorder = _skiplistIndex->leftKeyLookup(&search);
      leftBorder = leftBorder->nextNode();
      // NOTE: rightBorder < leftBorder => no Match.
      // Will be checked by interval valid
    }
  } else {
    // Copy rightSearch = leftSearch for right border
    VPackBuilder rightSearch = leftSearch;

    // Define Lower-Bound 
    VPackSlice lastLeft = lastNonEq.get(TRI_SLICE_KEY_GE);
    if (!lastLeft.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(TRI_SLICE_KEY_GT));
      leftSearch.add(lastLeft);
      leftSearch.close();
      VPackSlice search = leftSearch.slice();
      leftBorder = _skiplistIndex->leftKeyLookup(&search);
      // leftKeyLookup guarantees that we find the element before search. This
      // should not be in the cursor, but the next one
      // This is also save for the startNode, it should never be contained in the index.
      leftBorder = leftBorder->nextNode();
    } else {
      lastLeft = lastNonEq.get(TRI_SLICE_KEY_GT);
      if (!lastLeft.isNone()) {
        leftSearch.add(lastLeft);
        leftSearch.close();
        VPackSlice search = leftSearch.slice();
        leftBorder = _skiplistIndex->rightKeyLookup(&search);
        // leftBorder is identical or smaller than search, skip it.
        // It is guaranteed that the next element is greater than search
        leftBorder = leftBorder->nextNode();
      } else {
        // No lower bound set default to (null <= x)
        leftSearch.close();
        VPackSlice search = leftSearch.slice();
        leftBorder = _skiplistIndex->leftKeyLookup(&search);
        leftBorder = leftBorder->nextNode();
        // Now this is the correct leftBorder.
        // It is either the first equal one, or the first one greater than.
      }
    }
    // NOTE: leftBorder could be nullptr (no element fulfilling condition.)
    // This is checked later

    // Define upper-bound

    VPackSlice lastRight = lastNonEq.get(TRI_SLICE_KEY_LE);
    if (!lastRight.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(TRI_SLICE_KEY_LT));
      rightSearch.add(lastRight);
      rightSearch.close();
      VPackSlice search = rightSearch.slice();
      rightBorder = _skiplistIndex->rightKeyLookup(&search);
      if (rightBorder == _skiplistIndex->startNode()) {
        // No match make interval invalid
        rightBorder = nullptr;
      }
      // else rightBorder is correct
    } else {
      lastRight = lastNonEq.get(TRI_SLICE_KEY_LT);
      if (!lastRight.isNone()) {
        rightSearch.add(lastRight);
        rightSearch.close();
        VPackSlice search = rightSearch.slice();
        rightBorder = _skiplistIndex->leftKeyLookup(&search);
        if (rightBorder == _skiplistIndex->startNode()) {
          // No match make interval invalid
          rightBorder = nullptr;
        }
        // else rightBorder is correct
      } else {
        // No upper bound set default to (x <= INFINITY)
        rightSearch.close();
        VPackSlice search = rightSearch.slice();
        rightBorder = _skiplistIndex->rightKeyLookup(&search);
        if (rightBorder == _skiplistIndex->startNode()) {
          // No match make interval invalid
          rightBorder = nullptr;
        }
        // else rightBorder is correct
      }
    }
  }

  // Check if the interval is valid and not empty
  if (intervalValid(leftBorder, rightBorder)) {
    auto iterator = std::make_unique<SkiplistIterator>(reverse, leftBorder, rightBorder);
    if (iterator == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    return iterator.release();
  }

  // Creates an empty iterator
  auto iterator = std::make_unique<SkiplistIterator>(reverse, nullptr, nullptr);
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return iterator.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key with an element in a skip list, generic callback
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex::KeyElementComparator::operator()(
    VPackSlice const* leftKey,
    TRI_index_element_t const* rightElement) const {
  TRI_ASSERT(nullptr != leftKey);
  TRI_ASSERT(nullptr != rightElement);

  // Note that the key might contain fewer fields than there are indexed
  // attributes, therefore we only run the following loop to
  // leftKey->_numFields.
  TRI_ASSERT(leftKey->isArray());
  size_t numFields = leftKey->length();
  for (size_t j = 0; j < numFields; j++) {
    VPackSlice field = leftKey->at(j);
    int compareResult =
        CompareKeyElement(&field, rightElement, j);
    if (compareResult != 0) {
      return compareResult;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two elements in a skip list, this is the generic callback
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex::ElementElementComparator::operator()(
    TRI_index_element_t const* leftElement,
    TRI_index_element_t const* rightElement,
    arangodb::basics::SkipListCmpType cmptype) const {
  TRI_ASSERT(nullptr != leftElement);
  TRI_ASSERT(nullptr != rightElement);

  // ..........................................................................
  // The document could be the same -- so no further comparison is required.
  // ..........................................................................

  if (leftElement == rightElement ||
      (!_idx->_skiplistIndex->isArray() &&
       leftElement->document() == rightElement->document())) {
    return 0;
  }

  for (size_t j = 0; j < _idx->numPaths(); j++) {
    int compareResult =
        CompareElementElement(leftElement, j, rightElement, j);

    if (compareResult != 0) {
      return compareResult;
    }
  }

  // ...........................................................................
  // This is where the difference between the preorder and the proper total
  // order comes into play. Here if the 'keys' are the same,
  // but the doc ptr is different (which it is since we are here), then
  // we return 0 if we use the preorder and look at the _key attribute
  // otherwise.
  // ...........................................................................

  if (arangodb::basics::SKIPLIST_CMP_PREORDER == cmptype) {
    return 0;
  }

  // We break this tie in the key comparison by looking at the key:
  VPackSlice leftKey = VPackSlice(leftElement->document()->vpack()).get(StaticStrings::KeyString);
  VPackSlice rightKey = VPackSlice(rightElement->document()->vpack()).get(StaticStrings::KeyString);
 
  int compareResult = leftKey.compareString(rightKey.copyString());

  if (compareResult < 0) {
    return -1;
  } else if (compareResult > 0) {
    return 1;
  }
  return 0;
}

bool SkiplistIndex::accessFitsIndex(
    arangodb::aql::AstNode const* access, arangodb::aql::AstNode const* other,
    arangodb::aql::AstNode const* op, arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    bool isExecution) const {
  if (!this->canUseConditionPart(access, other, op, reference, isExecution)) {
    return false;
  }

  arangodb::aql::AstNode const* what = access;
  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>> attributeData;

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
      TRI_IF_FAILURE("SkiplistIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      return true;
    }
  }

  return false;
}

void SkiplistIndex::matchAttributes(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    size_t& values, bool isExecution) const {
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
                        found, isExecution);
        accessFitsIndex(op->getMember(1), op->getMember(0), op, reference,
                        found, isExecution);
        break;

      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
        if (accessFitsIndex(op->getMember(0), op->getMember(1), op, reference,
                            found, isExecution)) {
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

bool SkiplistIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  size_t values = 0;
  matchAttributes(node, reference, found, values, false);

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

bool SkiplistIndex::supportsSortCondition(
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
        estimatedCost = 0.0;
        return true;
      } else if (coveredAttributes > 0) {
        estimatedCost = (itemsInIndex / coveredAttributes) *
                        std::log2(static_cast<double>(itemsInIndex));
        return true;
      }
    }
  }

  coveredAttributes = 0;
  // by default no sort conditions are supported
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(static_cast<double>(itemsInIndex));
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

IndexIterator* SkiplistIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  VPackBuilder searchValues;
  searchValues.openArray();
  bool needNormalize = false;
  if (node == nullptr) {
    // We only use this index for sort. Empty searchValue
    VPackArrayBuilder guard(&searchValues);

    TRI_IF_FAILURE("SkiplistIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  } else {
    // Create the search Values for the lookup
    VPackArrayBuilder guard(&searchValues);

    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
    size_t unused = 0;
    matchAttributes(node, reference, found, unused, true);

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
                std::vector<arangodb::basics::AttributeName>> paramPair;
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
        searchValues.add(VPackValue(TRI_SLICE_KEY_EQUAL));
        TRI_IF_FAILURE("SkiplistIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      } else if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        if (isAttributeExpanded(usedFields)) {
          searchValues.openObject();
          searchValues.add(VPackValue(TRI_SLICE_KEY_EQUAL));
          TRI_IF_FAILURE("SkiplistIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        } else {
          needNormalize = true;
          searchValues.openObject();
          searchValues.add(VPackValue(TRI_SLICE_KEY_IN));
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
                searchValues.add(VPackValue(TRI_SLICE_KEY_GT));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LT));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GE));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LE));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LT));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GT));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(TRI_SLICE_KEY_LE));
              } else {
                searchValues.add(VPackValue(TRI_SLICE_KEY_GE));
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

  TRI_IF_FAILURE("SkiplistIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (needNormalize) {
    VPackBuilder expandedSearchValues;
    expandInSearchValues(searchValues.slice(), expandedSearchValues);
    VPackSlice expandedSlice = expandedSearchValues.slice();
    std::vector<IndexIterator*> iterators;
    try {
      for (auto const& val : VPackArrayIterator(expandedSlice)) {
        auto iterator = lookup(trx, val, reverse);
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
    }
    catch (...) {
      for (auto& it : iterators) {
        delete it;
      }
      throw; 
    }
    return new MultiIndexIterator(iterators);
  }
  VPackSlice searchSlice = searchValues.slice();
  TRI_ASSERT(searchSlice.length() == 1);
  searchSlice = searchSlice.at(0);
  return lookup(trx, searchSlice, reverse);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* SkiplistIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  size_t values = 0;
  matchAttributes(node, reference, found, values, false);

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

    std::sort(
        nodes.begin(), nodes.end(),
        [](arangodb::aql::AstNode const* lhs, arangodb::aql::AstNode const* rhs)
            -> bool { return sortWeight(lhs) < sortWeight(rhs); });

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

bool SkiplistIndex::isDuplicateOperator(
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
