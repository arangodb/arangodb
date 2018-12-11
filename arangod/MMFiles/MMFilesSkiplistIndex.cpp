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

#include "MMFilesSkiplistIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/FixedSizeAllocator.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "Indexes/SkiplistIndexAttributeMatcher.h"
#include "MMFiles/MMFilesCollection.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

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

/// @brief compares a key with an element, version with proper types
static int CompareKeyElement(void* userData, VPackSlice const* left,
                             MMFilesSkiplistIndexElement const* right,
                             size_t rightPosition) {
  MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);
  return arangodb::basics::VelocyPackHelper::compare(
      *left, right->slice(context, rightPosition), true);
}

/// @brief compares elements, version with proper types
static int CompareElementElement(void* userData,
                                 MMFilesSkiplistIndexElement const* left,
                                 size_t leftPosition,
                                 MMFilesSkiplistIndexElement const* right,
                                 size_t rightPosition) {
  MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);

  VPackSlice l = left->slice(context, leftPosition);
  VPackSlice r = right->slice(context, rightPosition);
  return arangodb::basics::VelocyPackHelper::compare(l, r, true);
}

bool MMFilesBaseSkiplistLookupBuilder::isEquality() const {
  return _isEquality;
}

VPackSlice const* MMFilesBaseSkiplistLookupBuilder::getLowerLookup() const {
  return &_lowerSlice;
}

bool MMFilesBaseSkiplistLookupBuilder::includeLower() const {
  return _includeLower;
}

VPackSlice const* MMFilesBaseSkiplistLookupBuilder::getUpperLookup() const {
  return &_upperSlice;
}

bool MMFilesBaseSkiplistLookupBuilder::includeUpper() const {
  return _includeUpper;
}

MMFilesSkiplistLookupBuilder::MMFilesSkiplistLookupBuilder(
    transaction::Methods* trx,
    std::vector<std::vector<arangodb::aql::AstNode const*>>& ops,
    arangodb::aql::Variable const* var, bool reverse)
    : MMFilesBaseSkiplistLookupBuilder(trx) {
  _lowerBuilder->openArray();
  if (ops.empty()) {
    // We only use this skiplist to sort. use empty array for lookup
    _lowerBuilder->close();
    _lowerSlice = _lowerBuilder->slice();
    _upperSlice = _lowerBuilder->slice();
    return;
  }

  auto const& last = ops.back();
  TRI_ASSERT(!last.empty());

  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>>
      paramPair;

  if (last[0]->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ &&
      last[0]->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    _isEquality = false;
    _upperBuilder->openArray();
    for (size_t i = 0; i < ops.size() - 1; ++i) {
      auto const& oplist = ops[i];
      TRI_ASSERT(oplist.size() == 1);
      auto const& op = oplist[0];
      TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
                 op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);
      TRI_ASSERT(op->numMembers() == 2);
      auto value = op->getMember(0);
      if (value->isAttributeAccessForVariable(paramPair) &&
          paramPair.first == var) {
        value = op->getMember(1);
        TRI_ASSERT(!(value->isAttributeAccessForVariable(paramPair) &&
                     paramPair.first == var));
      }
      value->toVelocyPackValue(*(_lowerBuilder.get()));
      value->toVelocyPackValue(*(_upperBuilder.get()));
    }

    TRI_IF_FAILURE("SkiplistIndex::permutationEQ") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("SkiplistIndex::permutationArrayIN") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    auto const& last = ops.back();
    for (auto const& op : last) {
      bool isReverseOrder = true;
      TRI_ASSERT(op->numMembers() == 2);

      auto value = op->getMember(0);
      if (value->isAttributeAccessForVariable(paramPair) &&
          paramPair.first == var) {
        value = op->getMember(1);
        TRI_ASSERT(!(value->isAttributeAccessForVariable(paramPair) &&
                     paramPair.first == var));
        isReverseOrder = false;
      }
      switch (op->type) {
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
          if (isReverseOrder) {
            _includeLower = false;
          } else {
            _includeUpper = false;
          }
          // intentionally falls through
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
          if (isReverseOrder) {
            value->toVelocyPackValue(*(_lowerBuilder.get()));
          } else {
            value->toVelocyPackValue(*(_upperBuilder.get()));
          }
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
          if (isReverseOrder) {
            _includeUpper = false;
          } else {
            _includeLower = false;
          }
          // intentionally falls through
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
          if (isReverseOrder) {
            value->toVelocyPackValue(*(_upperBuilder.get()));
          } else {
            value->toVelocyPackValue(*(_lowerBuilder.get()));
          }
          break;
        default:
          TRI_ASSERT(false);
      }
    }
    _lowerBuilder->close();
    _lowerSlice = _lowerBuilder->slice();

    _upperBuilder->close();
    _upperSlice = _upperBuilder->slice();
  } else {
    for (auto const& oplist : ops) {
      TRI_ASSERT(oplist.size() == 1);
      auto const& op = oplist[0];
      TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
                 op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);
      TRI_ASSERT(op->numMembers() == 2);
      auto value = op->getMember(0);
      if (value->isAttributeAccessForVariable(paramPair) &&
          paramPair.first == var) {
        value = op->getMember(1);
        TRI_ASSERT(!(value->isAttributeAccessForVariable(paramPair) &&
                     paramPair.first == var));
      }
      value->toVelocyPackValue(*(_lowerBuilder.get()));
    }

    TRI_IF_FAILURE("SkiplistIndex::permutationEQ") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("SkiplistIndex::permutationArrayIN") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _lowerBuilder->close();
    _lowerSlice = _lowerBuilder->slice();
    _upperSlice = _lowerBuilder->slice();
  }
}

bool MMFilesSkiplistLookupBuilder::next() {
  // The first search value is created during creation.
  // So next is always false.
  return false;
}

MMFilesSkiplistInLookupBuilder::MMFilesSkiplistInLookupBuilder(
    transaction::Methods* trx,
    std::vector<std::vector<arangodb::aql::AstNode const*>>& ops,
    arangodb::aql::Variable const* var, bool reverse)
    : MMFilesBaseSkiplistLookupBuilder(trx), _dataBuilder(trx), _done(false) {
  TRI_ASSERT(!ops.empty());  // We certainly do not need IN here
  transaction::BuilderLeaser tmp(trx);
  std::set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackSorted<true>>
      unique_set(
          (arangodb::basics::VelocyPackHelper::VPackSorted<true>(reverse)));
  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>>
      paramPair;

  _dataBuilder->clear();
  _dataBuilder->openArray();

  // The == and IN part
  for (size_t i = 0; i < ops.size() - 1; ++i) {
    auto const& oplist = ops[i];
    TRI_ASSERT(oplist.size() == 1);
    auto const& op = oplist[0];
    TRI_ASSERT(op->numMembers() == 2);
    auto value = op->getMember(0);
    bool valueLeft = true;
    if (value->isAttributeAccessForVariable(paramPair) &&
        paramPair.first == var) {
      valueLeft = false;
      value = op->getMember(1);
      TRI_ASSERT(!(value->isAttributeAccessForVariable(paramPair) &&
                   paramPair.first == var));
    }
    if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      if (valueLeft) {
        // Case: value IN x.a
        // This is identical to == for the index.
        value->toVelocyPackValue(*(_dataBuilder.get()));
      } else {
        // Case: x.a IN value
        TRI_ASSERT(value->numMembers() > 0);
        tmp->clear();
        unique_set.clear();
        value->toVelocyPackValue(*(tmp.get()));
        for (VPackSlice it : VPackArrayIterator(tmp->slice())) {
          unique_set.emplace(it);
        }
        TRI_IF_FAILURE("SkiplistIndex::permutationIN") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        _inPositions.emplace_back(i, 0, unique_set.size());
        _dataBuilder->openArray();
        for (auto const& it : unique_set) {
          _dataBuilder->add(it);
        }
        _dataBuilder->close();
      }
    } else {
      TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ);
      value->toVelocyPackValue(*(_dataBuilder.get()));
    }
  }
  auto const& last = ops.back();
  arangodb::aql::AstNode const* lower = nullptr;
  arangodb::aql::AstNode const* upper = nullptr;

  _isEquality = false;

  for (auto const& op : last) {
    bool isReverseOrder = true;
    TRI_ASSERT(op->numMembers() == 2);

    auto value = op->getMember(0);
    if (value->isAttributeAccessForVariable(paramPair) &&
        paramPair.first == var) {
      value = op->getMember(1);
      TRI_ASSERT(!(value->isAttributeAccessForVariable(paramPair) &&
                   paramPair.first == var));
      isReverseOrder = false;
    }

    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
        if (isReverseOrder) {
          _includeLower = false;
        } else {
          _includeUpper = false;
        }
        // intentionally falls through
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
        if (isReverseOrder) {
          TRI_ASSERT(lower == nullptr);
          lower = value;
        } else {
          TRI_ASSERT(upper == nullptr);
          upper = value;
        }
        break;
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
        if (isReverseOrder) {
          _includeUpper = false;
        } else {
          _includeLower = false;
        }
        // intentionally falls through
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
        if (isReverseOrder) {
          TRI_ASSERT(upper == nullptr);
          upper = value;
        } else {
          TRI_ASSERT(lower == nullptr);
          lower = value;
        }
        break;
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
        TRI_ASSERT(upper == nullptr);
        TRI_ASSERT(lower == nullptr);
        TRI_ASSERT(value->numMembers() > 0);
        tmp->clear();
        unique_set.clear();
        value->toVelocyPackValue(*(tmp.get()));
        for (auto const& it : VPackArrayIterator(tmp->slice())) {
          unique_set.emplace(it);
        }
        TRI_IF_FAILURE("Index::permutationIN") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        _inPositions.emplace_back(ops.size() - 1, 0, unique_set.size());
        _dataBuilder->openArray();
        for (auto const& it : unique_set) {
          _dataBuilder->add(it);
        }
        _dataBuilder->close();
        _isEquality = true;
        _dataBuilder->close();

        buildSearchValues();
        return;
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
        TRI_ASSERT(upper == nullptr);
        TRI_ASSERT(lower == nullptr);
        value->toVelocyPackValue(*(_dataBuilder.get()));
        _isEquality = true;
        _dataBuilder->close();

        buildSearchValues();
        return;
      default:
        TRI_ASSERT(false);
    }
  }
  _dataBuilder->openArray();
  if (lower == nullptr) {
    _dataBuilder->add(arangodb::velocypack::Slice::nullSlice());
  } else {
    lower->toVelocyPackValue(*(_dataBuilder.get()));
  }

  if (upper == nullptr) {
    _dataBuilder->add(arangodb::velocypack::Slice::nullSlice());
  } else {
    upper->toVelocyPackValue(*(_dataBuilder.get()));
  }
  _dataBuilder->close();
  _dataBuilder->close();

  buildSearchValues();
}

bool MMFilesSkiplistInLookupBuilder::next() {
  if (_done || !forwardInPosition()) {
    return false;
  }
  buildSearchValues();
  return true;
}

bool MMFilesSkiplistInLookupBuilder::forwardInPosition() {
  std::list<PosStruct>::reverse_iterator it = _inPositions.rbegin();
  while (it != _inPositions.rend()) {
    it->current++;
    TRI_ASSERT(it->_max > 0);
    if (it->current < it->_max) {
      return true;
      // Okay we increased this, next search value;
    }
    it->current = 0;
    ++it;
  }
  _done = true;
  // If we get here all positions are reset to 0.
  // We are done, no further combination
  return false;
}

void MMFilesSkiplistInLookupBuilder::buildSearchValues() {
  auto inPos = _inPositions.begin();
  _lowerBuilder->clear();
  _lowerBuilder->openArray();

  VPackSlice data = _dataBuilder->slice();
  if (!_isEquality) {
    _upperBuilder->clear();
    _upperBuilder->openArray();

    size_t const n = data.length();

    for (size_t i = 0; i < n - 1; ++i) {
      if (inPos != _inPositions.end() && i == inPos->field) {
        VPackSlice s = data.at(i).at(inPos->current);
        _lowerBuilder->add(s);
        _upperBuilder->add(s);
        inPos++;
      } else {
        VPackSlice s = data.at(i);
        _lowerBuilder->add(s);
        _upperBuilder->add(s);
      }
    }

    VPackSlice bounds = data.at(n - 1);
    TRI_ASSERT(bounds.isArray());
    TRI_ASSERT(bounds.length() == 2);
    VPackSlice b = bounds.at(0);
    if (!b.isNull()) {
      _lowerBuilder->add(b);
    }
    _lowerBuilder->close();
    _lowerSlice = _lowerBuilder->slice();

    b = bounds.at(1);
    if (!b.isNull()) {
      _upperBuilder->add(b);
    }

    _upperBuilder->close();
    _upperSlice = _upperBuilder->slice();
  } else {
    size_t const n = data.length();

    for (size_t i = 0; i < n; ++i) {
      if (inPos != _inPositions.end() && i == inPos->field) {
        _lowerBuilder->add(data.at(i).at(inPos->current));
        inPos++;
      } else {
        _lowerBuilder->add(data.at(i));
      }
    }
    _lowerBuilder->close();
    _lowerSlice = _lowerBuilder->slice();
    _upperSlice = _lowerBuilder->slice();
  }
}

MMFilesSkiplistIterator::MMFilesSkiplistIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mdr, arangodb::MMFilesSkiplistIndex const* index,
    TRI_Skiplist const* skiplist, size_t numPaths,
    std::function<int(void*, MMFilesSkiplistIndexElement const*,
                      MMFilesSkiplistIndexElement const*,
                      MMFilesSkiplistCmpType)> const& CmpElmElm,
    bool reverse, MMFilesBaseSkiplistLookupBuilder* builder)
    : IndexIterator(collection, trx),
      _skiplistIndex(skiplist),
      _context(trx, collection, mdr, index->fields().size()),
      _numPaths(numPaths),
      _reverse(reverse),
      _cursor(nullptr),
      _currentInterval(0),
      _builder(builder),
      _CmpElmElm(CmpElmElm) {
  TRI_ASSERT(_builder != nullptr);
  initNextInterval();  // Initializes the cursor
  TRI_ASSERT((_intervals.empty() && _cursor == nullptr) ||
             (!_intervals.empty() && _cursor != nullptr));
}

/// @brief Checks if the interval is valid. It is declared invalid if
///        one border is nullptr or the right is lower than left.
bool MMFilesSkiplistIterator::intervalValid(void* userData, Node* left,
                                            Node* right) const {
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
  if (_CmpElmElm(userData, left->document(), right->document(),
                 arangodb::SKIPLIST_CMP_TOTORDER) > 0) {
    return false;
  }
  return true;
}

/// @brief Reset the cursor
void MMFilesSkiplistIterator::reset() {
  // If _intervals is empty at this point
  // the cursor does not contain any
  // document at all. Reset is pointless
  if (!_intervals.empty()) {
    // We reset to the first interval and reset the cursor
    _currentInterval = 0;
    if (_reverse) {
      _cursor = _intervals[0].second;
    } else {
      _cursor = _intervals[0].first;
    }
  }
}

bool MMFilesSkiplistIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  while (limit > 0) {
    if (_cursor == nullptr) {
      // We are exhausted already, sorry
      return false;
    }
    TRI_ASSERT(_currentInterval < _intervals.size());
    auto const& interval = _intervals[_currentInterval];
    Node* tmp = _cursor;
    if (_reverse) {
      if (_cursor == interval.first) {
        forwardCursor();
      } else {
        _cursor = _cursor->prevNode();
      }
    } else {
      if (_cursor == interval.second) {
        forwardCursor();
      } else {
        _cursor = _cursor->nextNode();
      }
    }
    TRI_ASSERT(tmp != nullptr);
    TRI_ASSERT(tmp->document() != nullptr);
      
    cb(tmp->document()->localDocumentId());
    limit--;
  }
  return true;
}

bool MMFilesSkiplistIterator::nextDocument(DocumentCallback const& cb, size_t limit) {
  _documentIds.clear();
  _documentIds.reserve(limit);

  bool done = false;
  while (limit > 0) {
    if (_cursor == nullptr) {
      // We are exhausted already, sorry
      done = true;
      break;
    }
    TRI_ASSERT(_currentInterval < _intervals.size());
    auto const& interval = _intervals[_currentInterval];
    Node* tmp = _cursor;
    if (_reverse) {
      if (_cursor == interval.first) {
        forwardCursor();
      } else {
        _cursor = _cursor->prevNode();
      }
    } else {
      if (_cursor == interval.second) {
        forwardCursor();
      } else {
        _cursor = _cursor->nextNode();
      }
    }
    TRI_ASSERT(tmp != nullptr);
    TRI_ASSERT(tmp->document() != nullptr);
      
    _documentIds.emplace_back(std::make_pair(tmp->document()->localDocumentId(), nullptr));
    limit--;
  }
  
  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  physical->readDocumentWithCallback(_trx, _documentIds, cb);
  return !done;
}

void MMFilesSkiplistIterator::forwardCursor() {
  _currentInterval++;
  if (_currentInterval < _intervals.size()) {
    auto const& interval = _intervals[_currentInterval];
    if (_reverse) {
      _cursor = interval.second;
    } else {
      _cursor = interval.first;
    }
    return;
  }
  _cursor = nullptr;
  if (_builder->next()) {
    initNextInterval();
  }
}

void MMFilesSkiplistIterator::initNextInterval() {
  // We will always point the cursor to the resulting interval if any.
  // We do not take responsibility for the Nodes!
  Node* rightBorder = nullptr;
  Node* leftBorder = nullptr;

  while (true) {
    if (_builder->isEquality()) {
      rightBorder =
          _skiplistIndex->rightKeyLookup(&_context, _builder->getLowerLookup());
      if (rightBorder == _skiplistIndex->startNode()) {
        // No matching elements. Next interval
        if (!_builder->next()) {
          // No next interval. We are done.
          return;
        }
        // Builder moved forward. Try again.
        continue;
      }
      leftBorder =
          _skiplistIndex->leftKeyLookup(&_context, _builder->getLowerLookup());
      leftBorder = leftBorder->nextNode();
      // NOTE: rightBorder < leftBorder => no Match.
      // Will be checked by interval valid
    } else {
      if (_builder->includeLower()) {
        leftBorder = _skiplistIndex->leftKeyLookup(&_context,
                                                   _builder->getLowerLookup());
        // leftKeyLookup guarantees that we find the element before search.
      } else {
        leftBorder = _skiplistIndex->rightKeyLookup(&_context,
                                                    _builder->getLowerLookup());
        // leftBorder is identical or smaller than search
      }
      // This is the first element not to be returned, but the next one
      // Also save for the startNode, it should never be contained in the index.
      leftBorder = leftBorder->nextNode();

      if (_builder->includeUpper()) {
        rightBorder = _skiplistIndex->rightKeyLookup(
            &_context, _builder->getUpperLookup());
      } else {
        rightBorder = _skiplistIndex->leftKeyLookup(&_context,
                                                    _builder->getUpperLookup());
      }
      if (rightBorder == _skiplistIndex->startNode()) {
        // No match make interval invalid
        rightBorder = nullptr;
      }
      // else rightBorder is correct
    }
    if (!intervalValid(&_context, leftBorder, rightBorder)) {
      // No matching elements. Next interval
      if (!_builder->next()) {
        // No next interval. We are done.
        return;
      }
      // Builder moved forward. Try again.
      continue;
    }
    TRI_ASSERT(_currentInterval == _intervals.size());
    _intervals.emplace_back(leftBorder, rightBorder);
    if (_reverse) {
      _cursor = rightBorder;
    } else {
      _cursor = leftBorder;
    }
    // Next valid interal initialized. Return;
    return;
  }
}

/// @brief create the skiplist index
MMFilesSkiplistIndex::MMFilesSkiplistIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
)
    : MMFilesPathBasedIndex(iid, collection, info, sizeof(LocalDocumentId), true),
      CmpElmElm(this),
      CmpKeyElm(this),
      _skiplistIndex(nullptr) {
  _skiplistIndex =
      new TRI_Skiplist(CmpElmElm, CmpKeyElm,
                       [this](MMFilesSkiplistIndexElement* element) {
                         _allocator->deallocate(element);
                       },
                       _unique, _useExpansion);
}

/// @brief destroy the skiplist index
MMFilesSkiplistIndex::~MMFilesSkiplistIndex() { delete _skiplistIndex; }

size_t MMFilesSkiplistIndex::memory() const {
  return _skiplistIndex->memoryUsage() +
         static_cast<size_t>(_skiplistIndex->getNrUsed()) *
             MMFilesSkiplistIndexElement::baseMemoryUsage(_paths.size());
}

/// @brief return a VelocyPack representation of the index figures
void MMFilesSkiplistIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  MMFilesPathBasedIndex::toVelocyPackFigures(builder);
  _skiplistIndex->appendToVelocyPack(builder);
}

/// @brief inserts a document into a skiplist index
Result MMFilesSkiplistIndex::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  std::vector<MMFilesSkiplistIndexElement*> elements;
  Result res;

  int r;

  try {
    r = fillElement<MMFilesSkiplistIndexElement>(elements, documentId, doc);
  } catch (basics::Exception const& ex) {
    r = ex.code();
  } catch (std::bad_alloc const&) {
    r = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    r = TRI_ERROR_INTERNAL;
  }

  if (r != TRI_ERROR_NO_ERROR) {
    for (auto& element : elements) {
      // free all elements to prevent leak
      _allocator->deallocate(element);
    }
    return addErrorMsg(res, r);
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, numPaths());

  // insert into the index. the memory for the element will be owned or freed
  // by the index
  size_t const count = elements.size();

  size_t badIndex = 0;
  for (size_t i = 0; i < count; ++i) {
    r = _skiplistIndex->insert(&context, elements[i]);

    if (r != TRI_ERROR_NO_ERROR) {
      badIndex = i;

      // Note: this element is freed already
      for (size_t j = i; j < count; ++j) {
        _allocator->deallocate(elements[j]);
      }
      for (size_t j = 0; j < i; ++j) {
        _skiplistIndex->remove(&context, elements[j]);
        // No need to free elements[j] skiplist has taken over already
      }

      if (r == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && !_unique) {
        // We ignore unique_constraint violated if we are not unique
        r = TRI_ERROR_NO_ERROR;
      }

      break;
    }
  }

  if (r == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    elements.clear();

    // need to rebuild elements, find conflicting key to return error,
    // and then free elements again
    int innerRes = TRI_ERROR_NO_ERROR;
    try {
      innerRes = fillElement<MMFilesSkiplistIndexElement>(elements, documentId, doc);
    } catch (basics::Exception const& ex) {
      innerRes = ex.code();
    } catch (std::bad_alloc const&) {
      innerRes = TRI_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      innerRes = TRI_ERROR_INTERNAL;
    }

    auto guard = scopeGuard([this, &elements] {
      for (auto& element : elements) {
        // free all elements to prevent leak
        _allocator->deallocate(element);
      }
    });

    if (innerRes != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, innerRes);
    }

    auto found = _skiplistIndex->rightLookup(&context, elements[badIndex]);
    TRI_ASSERT(found);
    LocalDocumentId rev(found->document()->localDocumentId());
    std::string existingId;

    _collection.getPhysical()->readDocumentWithCallback(
      &trx,
      rev,
      [&existingId](LocalDocumentId const&, velocypack::Slice doc)->void {
      existingId = doc.get(StaticStrings::KeyString).copyString();
    });

    if (mode == OperationMode::internal) {
      return res.reset(r, std::move(existingId));
    }

    return addErrorMsg(res, r, existingId);
  }

  return addErrorMsg(res, r);
}

/// @brief removes a document from a skiplist index
Result MMFilesSkiplistIndex::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  std::vector<MMFilesSkiplistIndexElement*> elements;
  Result res;
  int r;

  try {
    r = fillElement<MMFilesSkiplistIndexElement>(elements, documentId, doc);
  } catch (basics::Exception const& ex) {
    r = ex.code();
  } catch (std::bad_alloc const&) {
    r = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    r = TRI_ERROR_INTERNAL;
  }

  if (r != TRI_ERROR_NO_ERROR) {
    for (auto& element : elements) {
      // free all elements to prevent leak
      _allocator->deallocate(element);
    }
    return addErrorMsg(res, r);
  }

  ManagedDocumentResult result;
  MMFilesIndexLookupContext context(&trx, &_collection, &result, numPaths());

  // attempt the removal for skiplist indexes
  // ownership for the index element is transferred to the index
  size_t const count = elements.size();

  for (size_t i = 0; i < count; ++i) {
    int result = _skiplistIndex->remove(&context, elements[i]);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (result != TRI_ERROR_NO_ERROR) {
      r = result;
    }

    _allocator->deallocate(elements[i]);
  }

  return addErrorMsg(res, r);
}

void MMFilesSkiplistIndex::unload() {
  _skiplistIndex->truncate(true);
}

/// @brief Checks if the interval is valid. It is declared invalid if
///        one border is nullptr or the right is lower than left.
bool MMFilesSkiplistIndex::intervalValid(void* userData, Node* left,
                                         Node* right) const {
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
  if (CmpElmElm(userData, left->document(), right->document(),
                arangodb::SKIPLIST_CMP_TOTORDER) > 0) {
    return false;
  }
  return true;
}

/// @brief compares a key with an element in a skip list, generic callback
int MMFilesSkiplistIndex::KeyElementComparator::operator()(
    void* userData, VPackSlice const* leftKey,
    MMFilesSkiplistIndexElement const* rightElement) const {
  TRI_ASSERT(nullptr != leftKey);
  TRI_ASSERT(nullptr != rightElement);

  // Note that the key might contain fewer fields than there are indexed
  // attributes, therefore we only run the following loop to
  // leftKey->_numFields.
  TRI_ASSERT(leftKey->isArray());
  size_t numFields = leftKey->length();
  for (size_t j = 0; j < numFields; j++) {
    VPackSlice field = leftKey->at(j);
    int compareResult = CompareKeyElement(userData, &field, rightElement, j);
    if (compareResult != 0) {
      return compareResult;
    }
  }

  return 0;
}

/// @brief compares two elements in a skip list, this is the generic callback
int MMFilesSkiplistIndex::ElementElementComparator::operator()(
    void* userData, MMFilesSkiplistIndexElement const* leftElement,
    MMFilesSkiplistIndexElement const* rightElement,
    MMFilesSkiplistCmpType cmptype) const {
  TRI_ASSERT(nullptr != leftElement);
  TRI_ASSERT(nullptr != rightElement);

  // ..........................................................................
  // The document could be the same -- so no further comparison is required.
  // ..........................................................................

  if (leftElement == rightElement ||
      (!_idx->_skiplistIndex->isArray() &&
       leftElement->localDocumentId() == rightElement->localDocumentId())) {
    return 0;
  }

  for (size_t j = 0; j < _idx->numPaths(); j++) {
    int compareResult =
        CompareElementElement(userData, leftElement, j, rightElement, j);

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

  if (arangodb::SKIPLIST_CMP_PREORDER == cmptype) {
    return 0;
  }

  // We break this tie in the key comparison by looking at the key:
  if (leftElement->localDocumentId() < rightElement->localDocumentId()) {
    return -1;
  }
  if (leftElement->localDocumentId() > rightElement->localDocumentId()) {
    return 1;
  }
  return 0;
}

bool MMFilesSkiplistIndex::accessFitsIndex(arangodb::aql::AstNode const* access, arangodb::aql::AstNode const* other,
                                           arangodb::aql::AstNode const* op, arangodb::aql::Variable const* reference,
                                           std::vector<std::vector<arangodb::aql::AstNode const*>>& found,
                                           std::unordered_set<std::string>& nonNullAttributes) const {
  if (!this->canUseConditionPart(access, other, op, reference,
                                 nonNullAttributes, true)) {
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
      found[i].emplace_back(op);
      
      TRI_IF_FAILURE("SkiplistIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      
      return true;
    }
  }
  
  return false;
}

bool MMFilesSkiplistIndex::findMatchingConditions(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::vector<std::vector<arangodb::aql::AstNode const*>>& mapping,
    bool& usesIn) const {
  std::unordered_set<std::string> nonNullAttributes;
  usesIn = false;

  for (size_t i = 0; i < node->numMembers(); ++i) {
    auto op = node->getMember(i);

    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE: {
        TRI_ASSERT(op->numMembers() == 2);
        accessFitsIndex(op->getMember(0), op->getMember(1),
                        op, reference, mapping, nonNullAttributes);
        accessFitsIndex(op->getMember(1), op->getMember(0),
                        op, reference, mapping, nonNullAttributes);
        break;
      }
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN: {
        auto m = op->getMember(1);
        if (accessFitsIndex(op->getMember(0), m, op, reference, mapping,
                            nonNullAttributes)) {
          if (m->numMembers() == 0) {
            // We want to do an IN [].
            // No results
            // Even if we cannot use the index.
            return false;
          }
        }
        break;
      }

      default: {
        TRI_ASSERT(false);
        break;
      }
    }
  }

  for (size_t i = 0; i < mapping.size(); ++i) {
    auto const& conditions = mapping[i];
    if (conditions.empty()) {
      // We do not have any condition for this field.
      // Remove it and everything afterwards.
      mapping.resize(i);
      TRI_ASSERT(i == mapping.size());
      break;
    }
    TRI_ASSERT(conditions.size() <= 2);
    auto const& first = conditions[0];
    switch (first->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
        if (first->getMember(1)->isArray()) {
          usesIn = true;
        }
        // intentionally falls through
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
        TRI_ASSERT(conditions.size() == 1);
        break;

      default: {
        // All conditions after this cannot be used.
        // shrink and break outer for loop
        mapping.resize(i + 1);
        TRI_ASSERT(i + 1 == mapping.size());
        return true;
      }
    }
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (auto const& it : mapping) {
    TRI_ASSERT(!it.empty());
  }
#endif

  return true;
}

IndexIterator* MMFilesSkiplistIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  std::vector<std::vector<arangodb::aql::AstNode const*>> mapping;
  bool usesIn = false;
  if (node != nullptr) {
    mapping.resize(_fields.size());  // We use the default constructor. Mapping
                                     // will have _fields many entries.
    TRI_ASSERT(mapping.size() == _fields.size());
    if (!findMatchingConditions(node, reference, mapping, usesIn)) {
      return new EmptyIndexIterator(&_collection, trx);
    }
  } else {
    TRI_IF_FAILURE("SkiplistIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("SkiplistIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (usesIn) {
    auto builder = std::make_unique<MMFilesSkiplistInLookupBuilder>(
        trx, mapping, reference, !opts.ascending);

    return new MMFilesSkiplistIterator(
      &_collection,
      trx,
      mdr,
      this,
      _skiplistIndex,
      numPaths(),
      CmpElmElm,
      !opts.ascending,
      builder.release()
    );
  }

  auto builder = std::make_unique<MMFilesSkiplistLookupBuilder>(
      trx, mapping, reference, !opts.ascending);

  return new MMFilesSkiplistIterator(
    &_collection,
    trx,
    mdr,
    this,
    _skiplistIndex,
    numPaths(),
    CmpElmElm,
    !opts.ascending,
    builder.release()
  );
}

bool MMFilesSkiplistIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  return SkiplistIndexAttributeMatcher::supportsFilterCondition(allIndexes, this, node, reference, itemsInIndex,
                                                                estimatedItems, estimatedCost);
}

bool MMFilesSkiplistIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost, size_t& coveredAttributes) const {
  return SkiplistIndexAttributeMatcher::supportsSortCondition(this, sortCondition, reference,
                                                        itemsInIndex, estimatedCost, coveredAttributes);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* MMFilesSkiplistIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  return SkiplistIndexAttributeMatcher::specializeCondition(this, node, reference);
}
