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
#include "Basics/debugging.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

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
/// @brief Create an index operator for the given bound.
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* buildBoundOperator(VPackSlice const& bound,
                                                bool includeEqual, bool upper,
                                                VPackSlice const& parameters,
                                                VocShaper* shaper) {
  if (bound.isNone()) {
    return nullptr;
  }
  TRI_index_operator_type_e type;
  if (includeEqual) {
    if (upper) {
      type = TRI_LE_INDEX_OPERATOR;
    } else {
      type = TRI_GE_INDEX_OPERATOR;
    }
  } else {
    if (upper) {
      type = TRI_LT_INDEX_OPERATOR;
    } else {
      type = TRI_GT_INDEX_OPERATOR;
    }
  }

  auto builder = std::make_shared<VPackBuilder>();
  try {
    VPackArrayBuilder b(builder.get());
    if (parameters.isArray()) {
      // Everything else is to be ignored.
      // Copy content of array
      for (auto const& e : VPackArrayIterator(parameters)) {
        builder->add(e);
      }
    }
    builder->add(bound);
  } catch (...) {
    // Out of memory. Cannot build operator.
    return nullptr;
  }

  return TRI_CreateIndexOperator(type, nullptr, nullptr, builder, shaper, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create an index operator for the range information.
///        Will either be a nullptr if no range is used.
///        Or a LE, LT, GE, GT operator if only one bound is given
///        Or an AND operator if both bounds are given.
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* buildRangeOperator(VPackSlice const& lowerBound,
                                                bool lowerBoundInclusive,
                                                VPackSlice const& upperBound,
                                                bool upperBoundInclusive,
                                                VPackSlice const& parameters,
                                                VocShaper* shaper) {
  std::unique_ptr<TRI_index_operator_t> lowerOperator(buildBoundOperator(
      lowerBound, lowerBoundInclusive, false, parameters, shaper));

  if (lowerOperator == nullptr && !lowerBound.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::unique_ptr<TRI_index_operator_t> upperOperator(buildBoundOperator(
      upperBound, upperBoundInclusive, true, parameters, shaper));
  
  if (upperOperator == nullptr && !upperBound.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (lowerOperator == nullptr) {
    return upperOperator.release();
  }
  if (upperOperator == nullptr) {
    return lowerOperator.release();
  }

  // And combine both
  std::unique_ptr<TRI_index_operator_t> rangeOperator(TRI_CreateIndexOperator(
      TRI_AND_INDEX_OPERATOR, lowerOperator.get(), upperOperator.get(),
      std::make_shared<VPackBuilder>(), nullptr, 2));
  lowerOperator.release();
  upperOperator.release();
  return rangeOperator.release();
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

static int CompareKeyElement(TRI_shaped_json_t const* left,
                             TRI_index_element_t const* right,
                             size_t rightPosition, VocShaper* shaper) {
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);

  auto rightSubobjects = right->subObjects();

  return TRI_CompareShapeTypes(
      nullptr, nullptr, left, shaper, right->document()->getShapedJsonPtr(),
      &rightSubobjects[rightPosition], nullptr, shaper);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares elements, version with proper types
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement(TRI_index_element_t const* left,
                                 size_t leftPosition,
                                 TRI_index_element_t const* right,
                                 size_t rightPosition, VocShaper* shaper) {
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);

  auto leftSubobjects = left->subObjects();
  auto rightSubobjects = right->subObjects();

  return TRI_CompareShapeTypes(
      left->document()->getShapedJsonPtr(), &leftSubobjects[leftPosition],
      nullptr, shaper, right->document()->getShapedJsonPtr(),
      &rightSubobjects[rightPosition], nullptr, shaper);
}

static int FillLookupOperator(TRI_index_operator_t* slOperator,
                              TRI_document_collection_t* document) {
  if (slOperator == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  switch (slOperator->_type) {
    case TRI_AND_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* logicalOperator =
          (TRI_logical_index_operator_t*)slOperator;
      int res = FillLookupOperator(logicalOperator->_left, document);

      if (res == TRI_ERROR_NO_ERROR) {
        res = FillLookupOperator(logicalOperator->_right, document);
      }
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
      break;
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* relationOperator =
          (TRI_relation_index_operator_t*)slOperator;
      VPackSlice const params = relationOperator->_parameters->slice();
      relationOperator->_numFields = static_cast<size_t>(params.length());
      relationOperator->_fields = static_cast<TRI_shaped_json_t*>(TRI_Allocate(
          TRI_UNKNOWN_MEM_ZONE,
          sizeof(TRI_shaped_json_t) * relationOperator->_numFields, false));

      if (relationOperator->_fields != nullptr) {
        for (size_t j = 0; j < relationOperator->_numFields; ++j) {
          VPackSlice const element = params.at(j);

          // find out if the search value is a list or an array
          if ((element.isArray() || element.isObject()) &&
              slOperator->_type != TRI_EQ_INDEX_OPERATOR) {
            // non-equality operator used on list or array data type, this is
            // disallowed
            // because we need to shape these objects first. however, at this
            // place (index lookup)
            // we never want to create new shapes so we will have a problem if
            // we cannot find an
            // existing shape for the search value. in this case we would need
            // to raise an error
            // but then the query results would depend on the state of the
            // shaper and if it had
            // seen previous such objects

            // we still allow looking for list or array values using equality.
            // this is safe.
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator->_fields);
            relationOperator->_fields = nullptr;
            return TRI_ERROR_BAD_PARAMETER;
          }

          // now shape the search object (but never create any new shapes)
          TRI_shaped_json_t* shapedObject = TRI_ShapedJsonVelocyPack(
              document->getShaper(), element,
              false);  // ONLY IN INDEX, PROTECTED by RUNTIME

          if (shapedObject != nullptr) {
            // found existing shape
            relationOperator->_fields[j] =
                *shapedObject;  // shallow copy here is ok
            TRI_Free(TRI_UNKNOWN_MEM_ZONE,
                     shapedObject);  // don't require storage anymore
          } else {
            // shape not found
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator->_fields);
            relationOperator->_fields = nullptr;
            return TRI_RESULT_ELEMENT_NOT_FOUND;
          }
        }
      } else {
        relationOperator->_numFields = 0;  // out of memory?
      }
      break;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

size_t SkiplistIterator::size() const { return _intervals.size(); }

void SkiplistIterator::initCursor() {
  size_t const n = size();
  if (0 < n) {
    if (_reverse) {
      // start at last interval, right endpoint
      _currentInterval = n - 1;
      _cursor = _intervals.at(_currentInterval)._rightEndPoint;
    } else {
      // start at first interval, left endpoint
      _currentInterval = 0;
      _cursor = _intervals.at(_currentInterval)._leftEndPoint;
    }
  } else {
    _cursor = nullptr;
  }
}

bool SkiplistIterator::hasNext() const {
  if (_reverse) {
    return hasPrevIteration();
  }
  return hasNextIteration();
}

TRI_index_element_t* SkiplistIterator::next() {
  if (_reverse) {
    return prevIteration();
  }
  return nextIteration();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the skiplist and returns iterator
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Tests whether the LeftEndPoint is < than RightEndPoint (-1)
// Tests whether the LeftEndPoint is == to RightEndPoint (0)    [empty]
// Tests whether the LeftEndPoint is > than RightEndPoint (1)   [undefined]
// .............................................................................

bool SkiplistIterator::findHelperIntervalValid(
    SkiplistIteratorInterval const& interval) {
  Node* lNode = interval._leftEndPoint;

  if (lNode == nullptr) {
    return false;
  }
  // Note that the right end point can be nullptr to indicate the end of
  // the index.

  Node* rNode = interval._rightEndPoint;

  if (lNode == rNode) {
    return false;
  }

  if (lNode->nextNode() == rNode) {
    // Interval empty, nothing to do with it.
    return false;
  }

  if (nullptr != rNode && rNode->nextNode() == lNode) {
    // Interval empty, nothing to do with it.
    return false;
  }

  if (_index->_skiplistIndex->getNrUsed() == 0) {
    return false;
  }

  if (lNode == _index->_skiplistIndex->startNode() || nullptr == rNode) {
    // The index is not empty, the nodes are not neighbours, one of them
    // is at the boundary, so the interval is valid and not empty.
    return true;
  }

  int compareResult =
      _index->CmpElmElm(lNode->document(), rNode->document(),
                        arangodb::basics::SKIPLIST_CMP_TOTORDER);
  return (compareResult == -1);
  // Since we know that the nodes are not neighbours, we can guarantee
  // at least one document in the interval.
}

bool SkiplistIterator::findHelperIntervalIntersectionValid(
    SkiplistIteratorInterval const& lInterval,
    SkiplistIteratorInterval const& rInterval,
    SkiplistIteratorInterval& interval) {
  Node* lNode = lInterval._leftEndPoint;
  Node* rNode = rInterval._leftEndPoint;

  if (nullptr == lNode || nullptr == rNode) {
    // At least one left boundary is the end, intersection is empty.
    return false;
  }

  int compareResult;
  // Now find the larger of the two start nodes:
  if (lNode == _index->_skiplistIndex->startNode()) {
    // We take rNode, even if it is the start node as well.
    compareResult = -1;
  } else if (rNode == _index->_skiplistIndex->startNode()) {
    // We take lNode
    compareResult = 1;
  } else {
    compareResult = _index->CmpElmElm(lNode->document(), rNode->document(),
                                      arangodb::basics::SKIPLIST_CMP_TOTORDER);
  }

  if (compareResult < 1) {
    interval._leftEndPoint = rNode;
  } else {
    interval._leftEndPoint = lNode;
  }

  lNode = lInterval._rightEndPoint;
  rNode = rInterval._rightEndPoint;

  // Now find the smaller of the two end nodes:
  if (nullptr == lNode) {
    // We take rNode, even is this also the end node.
    compareResult = 1;
  } else if (nullptr == rNode) {
    // We take lNode.
    compareResult = -1;
  } else {
    compareResult = _index->CmpElmElm(lNode->document(), rNode->document(),
                                      arangodb::basics::SKIPLIST_CMP_TOTORDER);
  }

  if (compareResult < 1) {
    interval._rightEndPoint = lNode;
  } else {
    interval._rightEndPoint = rNode;
  }

  return findHelperIntervalValid(interval);
}

void SkiplistIterator::findHelper(
    TRI_index_operator_t const* indexOperator,
    std::vector<SkiplistIteratorInterval>& intervals) {
  TRI_skiplist_index_key_t values;
  std::vector<SkiplistIteratorInterval> leftResult;
  std::vector<SkiplistIteratorInterval> rightResult;
  SkiplistIteratorInterval interval;
  Node* temp;

  switch (indexOperator->_type) {
    case TRI_EQ_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* relationOperator =
        (TRI_relation_index_operator_t*)indexOperator;

      values._fields = relationOperator->_fields;
      values._numFields = relationOperator->_numFields;
      break;  // this is to silence a compiler warning
    }

    default: {
      // must not access relationOperator->xxx if the operator is not a
      // relational one otherwise we'll get invalid reads and the prog
      // might crash
    }
  }

  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* logicalOperator =
        (TRI_logical_index_operator_t*)indexOperator;
      findHelper(logicalOperator->_left, leftResult);
      findHelper(logicalOperator->_right, rightResult);

      size_t nl = leftResult.size();
      size_t nr = rightResult.size();
      for (size_t i = 0; i < nl; ++i) {
        for (size_t j = 0; j < nr; ++j) {
          auto tempLeftInterval = leftResult[i];
          auto tempRightInterval = rightResult[j];

          if (findHelperIntervalIntersectionValid(
                  tempLeftInterval, tempRightInterval, interval)) {
            intervals.emplace_back(interval);
          }
        }
      }
      return;
    }

    case TRI_EQ_INDEX_OPERATOR: {
      temp = _index->_skiplistIndex->leftKeyLookup(&values);
      TRI_ASSERT(nullptr != temp);
      interval._leftEndPoint = temp;

      bool const allAttributesCoveredByCondition =
          (values._numFields == _index->numPaths());

      if (_index->unique() && allAttributesCoveredByCondition) {
        // At most one hit:
        temp = temp->nextNode();
        if (nullptr != temp) {
          if (0 == _index->CmpKeyElm(&values, temp->document())) {
            interval._rightEndPoint = temp->nextNode();
            if (findHelperIntervalValid(interval)) {
              intervals.emplace_back(interval);
            }
          }
        }
      } else {
        temp = _index->_skiplistIndex->rightKeyLookup(&values);
        interval._rightEndPoint = temp->nextNode();
        if (findHelperIntervalValid(interval)) {
          intervals.emplace_back(interval);
        }
      }
      return;
    }

    case TRI_LE_INDEX_OPERATOR: {
      interval._leftEndPoint = _index->_skiplistIndex->startNode();
      temp = _index->_skiplistIndex->rightKeyLookup(&values);
      interval._rightEndPoint = temp->nextNode();

      if (findHelperIntervalValid(interval)) {
        intervals.emplace_back(interval);
      }
      return;
    }

    case TRI_LT_INDEX_OPERATOR: {
      interval._leftEndPoint = _index->_skiplistIndex->startNode();
      temp = _index->_skiplistIndex->leftKeyLookup(&values);
      interval._rightEndPoint = temp->nextNode();

      if (findHelperIntervalValid(interval)) {
        intervals.emplace_back(interval);
      }
      return;
    }

    case TRI_GE_INDEX_OPERATOR: {
      temp = _index->_skiplistIndex->leftKeyLookup(&values);
      interval._leftEndPoint = temp;
      interval._rightEndPoint = _index->_skiplistIndex->endNode();

      if (findHelperIntervalValid(interval)) {
        intervals.emplace_back(interval);
      }
      return;
    }

    case TRI_GT_INDEX_OPERATOR: {
      temp = _index->_skiplistIndex->rightKeyLookup(&values);
      interval._leftEndPoint = temp;
      interval._rightEndPoint = _index->_skiplistIndex->endNode();

      if (findHelperIntervalValid(interval)) {
        intervals.emplace_back(interval);
      }
      return;
    }

    default: { TRI_ASSERT(false); }

  }  // end of switch statement
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a previous document within an
/// interval or before it - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

bool SkiplistIterator::hasPrevIteration() const {
  // ...........................................................................
  // if we have more intervals than the one we are currently working
  // on then of course we have a previous doc, because intervals are nonempty.
  // ...........................................................................
  if (_currentInterval > 0) {
    return true;
  }

  Node const* leftNode = _index->_skiplistIndex->prevNode(_cursor);

  // Note that leftNode can be nullptr here!
  // ...........................................................................
  // If the leftNode == left end point AND there are no more intervals
  // then we have no next.
  // ...........................................................................
  return leftNode != _intervals.at(_currentInterval)._leftEndPoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a next document within an
/// interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

bool SkiplistIterator::hasNextIteration() const {
  if (_cursor == nullptr) {
    return false;
  }

  // ...........................................................................
  // if we have more intervals than the one we are currently working
  // on then of course we have a next doc, since intervals are nonempty.
  // ...........................................................................
  if (_intervals.size() - 1 > _currentInterval) {
    return true;
  }

  Node const* leftNode = _cursor->nextNode();

  // Note that leftNode can be nullptr here!
  // ...........................................................................
  // If the left == right end point AND there are no more intervals then we have
  // no next.
  // ...........................................................................
  return leftNode != _intervals.at(_currentInterval)._rightEndPoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Jumps backwards by 1 and returns the document
////////////////////////////////////////////////////////////////////////////////

TRI_index_element_t* SkiplistIterator::prevIteration() {
  if (_currentInterval >= _intervals.size()) {
    return nullptr;
  }
  SkiplistIteratorInterval& interval = _intervals.at(_currentInterval);

  // ...........................................................................
  // use the current cursor and move 1 backward
  // ...........................................................................

  Node* result = nullptr;

  result = _index->_skiplistIndex->prevNode(_cursor);

  if (result == interval._leftEndPoint) {
    if (_currentInterval == 0) {
      _cursor = nullptr;  // exhausted
      return nullptr;
    }
    --_currentInterval;
    interval = _intervals.at(_currentInterval);
    _cursor = interval._rightEndPoint;
    result = _index->_skiplistIndex->prevNode(_cursor);
  }
  _cursor = result;

  TRI_ASSERT(result != nullptr);
  return result->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Jumps forwards by jumpSize and returns the document
////////////////////////////////////////////////////////////////////////////////

TRI_index_element_t* SkiplistIterator::nextIteration() {
  if (_cursor == nullptr) {
    // In this case the iterator is exhausted or does not even have intervals.
    return nullptr;
  }

  if (_currentInterval >= _intervals.size()) {
    return nullptr;
  }
  SkiplistIteratorInterval& interval = _intervals.at(_currentInterval);

  while (true) {  // will be left by break
    _cursor = _cursor->nextNode();
    if (_cursor != interval._rightEndPoint) {
      if (_cursor == nullptr) {
        return nullptr;
      }
      break;  // we found a next one
    }
    if (_currentInterval == _intervals.size() - 1) {
      _cursor = nullptr;  // exhausted
      return nullptr;
    }
    ++_currentInterval;
    interval = _intervals.at(_currentInterval);
    _cursor = interval._leftEndPoint;
  }

  return _cursor->document();
}

TRI_doc_mptr_t* SkiplistIndexIterator::next() {
  while (_iterator == nullptr) {
    if (_currentOperator == _operators.size()) {
      // Sorry nothing found at all
      return nullptr;
    }
    // We restart the lookup
    _iterator = _index->lookup(_trx, _operators[_currentOperator], _reverse);
    if (_iterator == nullptr) {
      // This iterator was not created.
      _currentOperator++;
    }
  }
  TRI_ASSERT(_iterator != nullptr);
  TRI_index_element_t* res = _iterator->next();
  while (res == nullptr) {
    // Try the next iterator
    _currentOperator++;
    if (_currentOperator == _operators.size()) {
      // We are done
      return nullptr;
    }
    // Free the former iterator and get the next one
    delete _iterator;
    _iterator = _index->lookup(_trx, _operators[_currentOperator], _reverse);
    res = _iterator->next();
  }
  return res->document();
}

void SkiplistIndexIterator::reset() {
  delete _iterator;
  _iterator = nullptr;
  _currentOperator = 0;
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

  int res = fillElement(elements, doc);

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

    if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && !_unique) {
      // We ignore unique_constraint violated if we are not unique
      res = TRI_ERROR_NO_ERROR;
    }

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

  int res = fillElement(elements, doc);

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
/// @brief attempts to locate an entry in the skip list index
///
/// Note: this function will not destroy the passed slOperator before it returns
/// Warning: who ever calls this function is responsible for destroying
/// the TRI_index_operator_t* and the SkiplistIterator* results
////////////////////////////////////////////////////////////////////////////////

SkiplistIterator* SkiplistIndex::lookup(arangodb::Transaction* trx,
                                        TRI_index_operator_t* slOperator,
                                        bool reverse) const {
  TRI_ASSERT(slOperator != nullptr);

  // .........................................................................
  // fill the relation operators which may be embedded in the slOperator with
  // additional information. Recall the slOperator is what information was
  // received from a user for query the skiplist.
  // .........................................................................

  int res = FillLookupOperator(slOperator, _collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    return nullptr;
  }

  auto results = std::make_unique<SkiplistIterator>(this, reverse);

  results->findHelper(slOperator, results->_intervals);

  results->initCursor();

  // Finally initialize _cursor if the result is not empty:
  return results.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key with an element in a skip list, generic callback
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex::KeyElementComparator::operator()(
    TRI_skiplist_index_key_t const* leftKey,
    TRI_index_element_t const* rightElement) const {
  TRI_ASSERT(nullptr != leftKey);
  TRI_ASSERT(nullptr != rightElement);

  auto shaper =
      _idx->collection()->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  // Note that the key might contain fewer fields than there are indexed
  // attributes, therefore we only run the following loop to
  // leftKey->_numFields.
  for (size_t j = 0; j < leftKey->_numFields; j++) {
    int compareResult =
        CompareKeyElement(&leftKey->_fields[j], rightElement, j, shaper);
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

  auto shaper =
      _idx->_collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  for (size_t j = 0; j < _idx->numPaths(); j++) {
    int compareResult =
        CompareElementElement(leftElement, j, rightElement, j, shaper);

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
  int compareResult = strcmp(
      TRI_EXTRACT_MARKER_KEY(
          leftElement->document()),  // ONLY IN INDEX, PROTECTED by RUNTIME
      TRI_EXTRACT_MARKER_KEY(
          rightElement->document()));  // ONLY IN INDEX, PROTECTED by RUNTIME

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
      (!_sparse || (_sparse && attributesCovered == _fields.size()))) {
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
    double& estimatedCost) const {
  TRI_ASSERT(sortCondition != nullptr);

  if (!_sparse) {
    // only non-sparse indexes can be used for sorting
    if (!_useExpansion && sortCondition->isUnidirectional() &&
        sortCondition->isOnlyAttributeAccess()) {
      size_t const coveredAttributes =
          sortCondition->coveredAttributes(reference, _fields);

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
    arangodb::aql::Ast* ast, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  // Create the skiplistOperator for the IndexLookup
  if (node == nullptr) {
    // We have no condition, we just use sort
    auto builder = std::make_shared<VPackBuilder>();
    {
      VPackArrayBuilder b(builder.get());
      builder->add(VPackValue(VPackValueType::Null));
    }
    std::unique_ptr<TRI_index_operator_t> unboundOperator(
        TRI_CreateIndexOperator(TRI_GE_INDEX_OPERATOR, nullptr, nullptr,
                                builder, _shaper, 1));
    std::vector<TRI_index_operator_t*> searchValues({unboundOperator.get()});
    unboundOperator.release();

    TRI_IF_FAILURE("SkiplistIndex::noSortIterator") {
      // prevent a (false-positive) memleak here
      for (auto& it : searchValues) {
        delete it;
      }
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    return new SkiplistIndexIterator(trx, this, searchValues, reverse);
  }

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

  // initialize permutations
  std::vector<PermutationState> permutationStates;
  permutationStates.reserve(_fields.size());
  size_t maxPermutations = 1;

  size_t usedFields = 0;
  for (; usedFields < _fields.size(); ++usedFields) {
    // We are in the equality range, we only allow one == or IN node per
    // attribute
    auto it = found.find(usedFields);
    if (it == found.end() || it->second.size() != 1) {
      // We are either done,
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
      // This is an equalityCheck, we can continue with the next field
      permutationStates.emplace_back(
          PermutationState(comp->type, value, usedFields, 1));
      TRI_IF_FAILURE("SkiplistIndex::permutationEQ") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } else if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      if (isAttributeExpanded(usedFields)) {
        permutationStates.emplace_back(PermutationState(
            aql::NODE_TYPE_OPERATOR_BINARY_EQ, value, usedFields, 1));
        TRI_IF_FAILURE("SkiplistIndex::permutationArrayIN") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      } else {
        if (value->numMembers() == 0) {
          return nullptr;
        }
        permutationStates.emplace_back(PermutationState(
            comp->type, value, usedFields, value->numMembers()));
        TRI_IF_FAILURE("SkiplistIndex::permutationIN") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        maxPermutations *= value->numMembers();
      }
    } else {
      // This is a one-sided range
      break;
    }
  }

  // Now handle the next element, which might be a range
  bool includeLower = false;
  bool includeUpper = false;
  auto lower = std::make_shared<VPackBuilder>();
  auto upper = std::make_shared<VPackBuilder>();
  if (usedFields < _fields.size()) {
    auto it = found.find(usedFields);
    if (it != found.end()) {
      auto rangeConditions = it->second;
      TRI_ASSERT(rangeConditions.size() <= 2);

      for (auto& comp : rangeConditions) {
        TRI_ASSERT(comp->numMembers() == 2);
        arangodb::aql::AstNode const* access = nullptr;
        arangodb::aql::AstNode const* value = nullptr;
        bool isReverseOrder = getValueAccess(comp, access, value);

        auto setBorder = [&](bool isLower, bool includeBound) -> void {
          if (isLower == isReverseOrder) {
            // We set an upper bound
            TRI_ASSERT(upper->isEmpty());
            upper = value->toVelocyPackValue();
            includeUpper = includeBound;
          } else {
            // We set an lower bound
            TRI_ASSERT(lower->isEmpty());
            lower = value->toVelocyPackValue();
            includeLower = includeBound;
          }
        };
        // This is not an equalityCheck, set lower or upper
        switch (comp->type) {
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
            setBorder(false, false);
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
            setBorder(false, true);
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
            setBorder(true, false);
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
            setBorder(true, true);
            break;
          default:
            // unsupported right now. Should have been rejected by
            // supportsFilterCondition
            TRI_ASSERT(false);
            return nullptr;
        }
      }
    }
  }

  std::vector<TRI_index_operator_t*> searchValues;
  searchValues.reserve(maxPermutations);
  VPackSlice emptySlice;

  try {
    if (usedFields == 0) {
      // We have a range query based on the first _field
      std::unique_ptr<TRI_index_operator_t> op(
          buildRangeOperator(lower->slice(), includeLower, upper->slice(),
                             includeUpper, emptySlice, _shaper));

      if (op != nullptr) {
        searchValues.emplace_back(op.get());
        op.release();

        TRI_IF_FAILURE("SkiplistIndex::onlyRangeOperator") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      }
    } else {
      bool done = false;
      // create all permutations
      while (!done) {
        auto parameter = std::make_shared<VPackBuilder>();
        bool valid = true;

        try {
          VPackArrayBuilder b(parameter.get());
          for (size_t i = 0; i < usedFields; ++i) {
            TRI_ASSERT(i < permutationStates.size());
            auto& state = permutationStates[i];

            std::shared_ptr<VPackBuilder> valueBuilder =
                state.getValue()->toVelocyPackValue();
            VPackSlice const value = valueBuilder->slice();

            if (value.isNone()) {
              valid = false;
              break;
            }
            parameter->add(value);
          }
        } catch (...) {
          // Out of Memory
          return nullptr;
        }

        if (valid) {
          std::unique_ptr<TRI_index_operator_t> tmpOp(
              TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr, nullptr,
                                      parameter, _shaper, usedFields));
          // Note we create a new RangeOperator always.
          std::unique_ptr<TRI_index_operator_t> rangeOperator(
              buildRangeOperator(lower->slice(), includeLower, upper->slice(),
                                 includeUpper, parameter->slice(), _shaper));

          if (rangeOperator != nullptr) {
            std::unique_ptr<TRI_index_operator_t> combinedOp(
                TRI_CreateIndexOperator(
                    TRI_AND_INDEX_OPERATOR, tmpOp.get(), rangeOperator.get(),
                    std::make_shared<VPackBuilder>(), _shaper, 2));
            rangeOperator.release();
            tmpOp.release();
            searchValues.emplace_back(combinedOp.get());
            combinedOp.release();
            TRI_IF_FAILURE("SkiplistIndex::rangeOperatorNoTmp") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }
          } else {
            if (tmpOp != nullptr) {
              searchValues.emplace_back(tmpOp.get());
              tmpOp.release();
              TRI_IF_FAILURE("SkiplistIndex::rangeOperatorTmp") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }
            }
          }
        }

        size_t const np = permutationStates.size() - 1;
        size_t current = 0;
        // now permute
        while (true) {
          if (++permutationStates[np - current].current <
              permutationStates[np - current].n) {
            current = 0; // note: resetting the variable has no effect here
            // abort inner iteration
            break;
          }

          permutationStates[np - current].current = 0;

          if (++current >= usedFields) {
            done = true;
            break;
          }
          // next inner iteration
        }
      }
    }

    if (searchValues.empty()) {
      return nullptr;
    }

    if (reverse) {
      std::reverse(searchValues.begin(), searchValues.end());
    }

    TRI_IF_FAILURE("SkiplistIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  } catch (...) {
    // prevent memleak here
    for (auto& it : searchValues) {
      delete it;
    }
    throw;
  }

  TRI_IF_FAILURE("SkiplistIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return new SkiplistIndexIterator(trx, this, searchValues, reverse);
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
      // do not less duplicate or related operators pass
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
