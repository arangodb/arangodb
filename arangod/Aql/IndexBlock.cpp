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
/// @author Jan Steemann
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

IndexBlock::IndexBlock(ExecutionEngine* engine, IndexNode const* en)
    : ExecutionBlock(engine, en),
      _collection(en->collection()),
      _currentIndex(0),
      _indexes(en->getIndexes()),
      _cursor(nullptr),
      _cursors(_indexes.size()),
      _condition(en->_condition->root()),
      _hasV8Expression(false),
      _indexesExhausted(false),
      _isLastIndex(false),
      _returned(0),
      _collector(&_engine->_itemBlockManager) {
  _mmdr.reset(new ManagedDocumentResult);

  if (_condition != nullptr) {
    // fix const attribute accesses, e.g. { "a": 1 }.a
    for (size_t i = 0; i < _condition->numMembers(); ++i) {
      auto andCond = _condition->getMemberUnchecked(i);
      for (size_t j = 0; j < andCond->numMembers(); ++j) {
        auto leaf = andCond->getMemberUnchecked(j);

        if (leaf->numMembers() != 2) {
          continue;
        }
        // We only support binary conditions
        TRI_ASSERT(leaf->numMembers() == 2);
        AstNode* lhs = leaf->getMember(0);
        AstNode* rhs = leaf->getMember(1);

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->isConstant()) {
          lhs = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(lhs));
          leaf->changeMember(0, lhs);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->isConstant()) {
          rhs = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(rhs));
          leaf->changeMember(1, rhs);
        }
      }
    }
  }
}

IndexBlock::~IndexBlock() { cleanupNonConstExpressions(); }

/// @brief adds a UNIQUE() to a dynamic IN condition
arangodb::aql::AstNode* IndexBlock::makeUnique(
    arangodb::aql::AstNode* node) const {
  if (node->type != arangodb::aql::NODE_TYPE_ARRAY || node->numMembers() >= 2) {
    // an non-array or an array with more than 1 member
    auto en = static_cast<IndexNode const*>(getPlanNode());
    auto ast = en->_plan->getAst();
    auto array = ast->createNodeArray();
    array->addMember(node);
    auto trx = transaction();
    bool isSorted = false;
    bool isSparse = false;
    auto unused =
        trx->getIndexFeatures(_indexes[_currentIndex], isSorted, isSparse);
    if (isSparse) {
      // the index is sorted. we need to use SORTED_UNIQUE to get the
      // result back in index order
      return ast->createNodeFunctionCall("SORTED_UNIQUE", array);
    }
    // a regular UNIQUE will do
    return ast->createNodeFunctionCall("UNIQUE", array);
  }

  // presumably an array with no or a single member
  return node;
}

void IndexBlock::executeExpressions() {
  DEBUG_BEGIN_BLOCK();
  TRI_ASSERT(_condition != nullptr);
  TRI_ASSERT(!_nonConstExpressions.empty());

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:

  AqlItemBlock* cur = _buffer.front();
  auto en = static_cast<IndexNode const*>(getPlanNode());
  auto ast = en->_plan->getAst();
  for (size_t posInExpressions = 0;
       posInExpressions < _nonConstExpressions.size(); ++posInExpressions) {
    auto& toReplace = _nonConstExpressions[posInExpressions];
    auto exp = toReplace->expression;

    bool mustDestroy;
    AqlValue a = exp->execute(_trx, cur, _pos, _inVars[posInExpressions],
                              _inRegs[posInExpressions], mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AstNode* evaluatedNode = nullptr;

    AqlValueMaterializer materializer(_trx);
    VPackSlice slice = materializer.slice(a, false);
    evaluatedNode = ast->nodeFromVPack(slice, true);

    _condition->getMember(toReplace->orMember)
        ->getMember(toReplace->andMember)
        ->changeMember(toReplace->operatorMember, evaluatedNode);
  }
  DEBUG_END_BLOCK();
}

int IndexBlock::initialize() {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initialize();

  auto en = static_cast<IndexNode const*>(getPlanNode());
  auto ast = en->_plan->getAst();

  // instantiate expressions:
  auto instantiateExpression = [&](size_t i, size_t j, size_t k,
                                   AstNode* a) -> void {
    // all new AstNodes are registered with the Ast in the Query
    auto e = std::make_unique<Expression>(ast, a);

    TRI_IF_FAILURE("IndexBlock::initialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasV8Expression |= e->isV8();

    std::unordered_set<Variable const*> inVars;
    e->variables(inVars);

    auto nce = std::make_unique<NonConstExpression>(i, j, k, e.get());
    e.release();
    _nonConstExpressions.push_back(nce.get());
    nce.release();

    // Prepare _inVars and _inRegs:
    _inVars.emplace_back();
    std::vector<Variable const*>& inVarsCur = _inVars.back();
    _inRegs.emplace_back();
    std::vector<RegisterId>& inRegsCur = _inRegs.back();

    for (auto const& v : inVars) {
      inVarsCur.emplace_back(v);
      auto it = en->getRegisterPlan()->varInfo.find(v->id);
      TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
      TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
      inRegsCur.emplace_back(it->second.registerId);
    }
  };

  if (_condition == nullptr) {
    // This Node has no condition. Iterate over the complete index.
    return TRI_ERROR_NO_ERROR;
  }

  auto outVariable = en->outVariable();

  for (size_t i = 0; i < _condition->numMembers(); ++i) {
    auto andCond = _condition->getMemberUnchecked(i);
    for (size_t j = 0; j < andCond->numMembers(); ++j) {
      auto leaf = andCond->getMemberUnchecked(j);

      if (leaf->numMembers() != 2) {
        continue;
      }

      // We only support binary conditions
      TRI_ASSERT(leaf->numMembers() == 2);
      AstNode* lhs = leaf->getMember(0);
      AstNode* rhs = leaf->getMember(1);

      if (lhs->isAttributeAccessForVariable(outVariable, false)) {
        // Index is responsible for the left side, check if right side has to be
        // evaluated
        if (!rhs->isConstant()) {
          if (leaf->type == NODE_TYPE_OPERATOR_BINARY_IN) {
            rhs = makeUnique(rhs);
          }
          instantiateExpression(i, j, 1, rhs);
          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      } else {
        // Index is responsible for the right side, check if left side has to be
        // evaluated
        if (!lhs->isConstant()) {
          instantiateExpression(i, j, 0, lhs);
          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
    }
  }

  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

// init the ranges for reading, this should be called once per new incoming
// block!
//
// This is either called every time we get a new incoming block.
// If all the bounds are constant, then in the case of hash, primary or edges
// indexes it does nothing. In the case of a skiplist index, it creates a
// skiplistIterator which is used by readIndex. If at least one bound is
// variable, then this this also evaluates the IndexOrCondition required to
// determine the values of the bounds.
//
// It is guaranteed that
//   _buffer   is not empty, in particular _buffer.front() is defined
//   _pos      points to a position in _buffer.front()
// Therefore, we can use the register values in _buffer.front() in row
// _pos to evaluate the variable bounds.

bool IndexBlock::initIndexes() {
  DEBUG_BEGIN_BLOCK();
  // We start with a different context. Return documents found in the previous
  // context again.
  _alreadyReturned.clear();
  // Find out about the actual values for the bounds in the variable bound case:

  if (!_nonConstExpressions.empty()) {
    TRI_ASSERT(_condition != nullptr);

    if (_hasV8Expression) {
      bool const isRunningInCluster =
          arangodb::ServerState::instance()->isRunningInCluster();

      // must have a V8 context here to protect Expression::execute()
      auto engine = _engine;
      arangodb::basics::ScopeGuard guard{
          [&engine]() -> void { engine->getQuery()->enterContext(); },
          [&]() -> void {
            if (isRunningInCluster) {
              // must invalidate the expression now as we might be called from
              // different threads
              for (auto const& e : _nonConstExpressions) {
                e->expression->invalidate();
              }

              engine->getQuery()->exitContext();
            }
          }};

      ISOLATE;
      v8::HandleScope scope(isolate);  // do not delete this!

      executeExpressions();
      TRI_IF_FAILURE("IndexBlock::executeV8") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } else {
      // no V8 context required!

      Functions::InitializeThreadContext();
      try {
        executeExpressions();
        TRI_IF_FAILURE("IndexBlock::executeExpression") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        Functions::DestroyThreadContext();
      } catch (...) {
        Functions::DestroyThreadContext();
        throw;
      }
    }
  }
  IndexNode const* node = static_cast<IndexNode const*>(getPlanNode());
  if (node->_reverse) {
    _currentIndex = _indexes.size() - 1;
  } else {
    _currentIndex = 0;
  }

  createCursor();
  if (_cursor->failed()) {
    THROW_ARANGO_EXCEPTION(_cursor->code);
  }

  while (!_cursor->hasMore()) {
    if (node->_reverse) {
      --_currentIndex;
    } else {
      ++_currentIndex;
    }
    if (_currentIndex < _indexes.size()) {
      // This check will work as long as _indexes.size() < MAX_SIZE_T
      createCursor();
      if (_cursor->failed()) {
        THROW_ARANGO_EXCEPTION(_cursor->code);
      }
    } else {
      _cursor = nullptr;
      _indexesExhausted = true;
      // We were not able to initialize any index with this condition
      return false;
    }
  }
  _indexesExhausted = false;
  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief create an OperationCursor object
void IndexBlock::createCursor() {
  DEBUG_BEGIN_BLOCK();
  _cursor = orderCursor(_currentIndex);
  DEBUG_END_BLOCK();
}

/// @brief Forwards _iterator to the next available index
void IndexBlock::startNextCursor() {
  DEBUG_BEGIN_BLOCK();

  IndexNode const* node = static_cast<IndexNode const*>(getPlanNode());
  if (node->_reverse) {
    --_currentIndex;
    _isLastIndex = (_currentIndex == 0);
  } else {
    ++_currentIndex;
    _isLastIndex = (_currentIndex == _indexes.size() - 1);
  }
  if (_currentIndex < _indexes.size()) {
    // This check will work as long as _indexes.size() < MAX_SIZE_T
    createCursor();
  } else {
    _cursor = nullptr;
  }
  DEBUG_END_BLOCK();
}

// this is called every time we just skip in the index
bool IndexBlock::skipIndex(size_t atMost) {
  DEBUG_BEGIN_BLOCK();

  if (_cursor == nullptr || _indexesExhausted) {
    // All indexes exhausted
    return false;
  }

  while (_cursor != nullptr) {
    if (!_cursor->hasMore()) {
      startNextCursor();
      continue;
    }

    if (_returned == atMost) {
      // We have skipped enough, do not check if we have more
      return true;
    }

    TRI_IF_FAILURE("IndexBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (_cursor->skip(atMost - _returned, _returned)) {
      // We have skipped enough.
      // And this index could return more.
      // We are good.
      return true;
    }
  }
  return false;
}

// this is called every time we need to fetch data from the indexes
bool IndexBlock::readIndex(
    size_t atMost,
    IndexIterator::TokenCallback const& callback) {
  DEBUG_BEGIN_BLOCK();
  // this is called every time we want to read the index.
  // For the primary key index, this only reads the index once, and never
  // again (although there might be multiple calls to this function).
  // For the edge, hash or skiplists indexes, initIndexes creates an iterator
  // and read*Index just reads from the iterator until it is done.
  // Then initIndexes is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents.

  if (_cursor == nullptr || _indexesExhausted) {
    // All indexes exhausted
    return false;
  }
    
  while (_cursor != nullptr) {
    if (!_cursor->hasMore()) {
      startNextCursor();
      continue;
    }

    TRI_ASSERT(atMost >= _returned);

    if (_returned == atMost) {
      // We have returned enough, do not check if we have more
      return true;
    }

    TRI_IF_FAILURE("IndexBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(atMost >= _returned);
  
    if (_cursor->next(callback, atMost - _returned)) {
      // We have returned enough.
      // And this index could return more.
      // We are good.
      return true;
    }
  }
  // if we get here the indexes are exhausted.
  return false;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int IndexBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  _collector.clear();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _alreadyReturned.clear();
  _returned = 0;
  _pos = 0;

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSome
AqlItemBlock* IndexBlock::getSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();
  if (_done) {
    traceGetSomeEnd(nullptr);
    return _collector.steal();
  }

  TRI_ASSERT(atMost > 0);
  size_t curRegs;

  std::unique_ptr<AqlItemBlock> res;

  IndexIterator::TokenCallback callback;
  if (_indexes.size() > 1) {
    // Activate uniqueness checks
    callback = [&](DocumentIdentifierToken const& token) {
      TRI_ASSERT(res.get() != nullptr);
      if (!_isLastIndex) {
        // insert & check for duplicates in one go
        if (!_alreadyReturned.emplace(token).second) {
          // Document already in list. Skip this
          return;
        }
      } else {
        // only check for duplicates
        if (_alreadyReturned.find(token) != _alreadyReturned.end()) {
          // Document found, skip
          return;
        }
      }
      if (_cursor->collection()->readDocument(_trx, token, *_mmdr)) {
        res->setValue(_returned,
                      static_cast<arangodb::aql::RegisterId>(curRegs),
                      _mmdr->createAqlValue());

        if (_returned > 0) {
          // re-use already copied AqlValues
          res->copyValuesFromFirstRow(_returned,
                                      static_cast<RegisterId>(curRegs));
        }
        ++_returned;
      }
      // What happens in else case?
      // Index has stored a document the primary store does not know
    };
  } else {
    // No uniqueness checks
    callback = [&](DocumentIdentifierToken const& token) {
      TRI_ASSERT(res.get() != nullptr);
      if (_cursor->collection()->readDocument(_trx, token, *_mmdr)) {
        res->setValue(_returned,
                      static_cast<arangodb::aql::RegisterId>(curRegs),
                      _mmdr->createAqlValue());

        if (_returned > 0) {
          // re-use already copied AqlValues
          res->copyValuesFromFirstRow(_returned,
                                      static_cast<RegisterId>(curRegs));
        }
        ++_returned;
      }
      // What happens in else case?
      // Index has stored a document the primary store does not know
    };
  }

  size_t found = 0;
  do {
    _returned = 0;
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch) || (!initIndexes())) {
        _done = true;
        break;
      }
      TRI_ASSERT(!_indexesExhausted);
    }
    if (_indexesExhausted) {
      AqlItemBlock* cur = _buffer.front();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
      if (_buffer.empty()) {
        if (!ExecutionBlock::getBlock(DefaultBatchSize(), DefaultBatchSize())) {
          _done = true;
          break;
        }
      }

      if (!initIndexes()) {
        _done = true;
        break;
      }
      TRI_ASSERT(!_indexesExhausted);
    }

    // We only get here with non-exhausted indexes.
    // At least one of them is prepared and ready to read.
    TRI_ASSERT(!_indexesExhausted);
    AqlItemBlock* cur = _buffer.front();
    curRegs = cur->getNrRegs();
   
    TRI_ASSERT(atMost >= found);

    res.reset(requestBlock(
        atMost - found,
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    // Read the next (atMost - j) many elements from the indexes
    _indexesExhausted = !readIndex(atMost - found, callback);
    if (_returned > 0) {
      if (_returned < atMost - found) {
        // We have prepared too many entries, we could only fill less.
        // Shrink the block
        res->shrink(_returned, false);
      }
      _collector.add(std::move(res));
    } else {
      // No results. Kill the registers
      res.reset();
    }
    TRI_ASSERT(res.get() == nullptr);

    // Update statistics
    _engine->_stats.scannedIndex += _returned;
    found += _returned;
    _returned = 0;
  } while (found < atMost);

  TRI_ASSERT(found == _collector.totalSize());

  if (found == 0) {
    // We have not found anything at all.
    // Return a nullptr.
    return _collector.steal();
  }

  res.reset(_collector.steal());

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  traceGetSomeEnd(res.get());

  return res.release();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skipSome
size_t IndexBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  if (_done) {
    return 0;
  }

  _returned = 0;

  while (_returned < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch) || (!initIndexes())) {
        _done = true;
        break;
      }
      TRI_ASSERT(!_indexesExhausted);
      _pos = 0;  // this is in the first block
    }
    if (_indexesExhausted) {
      AqlItemBlock* cur = _buffer.front();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
      if (_buffer.empty()) {
        if (!ExecutionBlock::getBlock(DefaultBatchSize(), DefaultBatchSize())) {
          _done = true;
          break;
        }
        _pos = 0;  // this is in the first block
      }

      if (!initIndexes()) {
        _done = true;
        break;
      }
      TRI_ASSERT(!_indexesExhausted);
    }

    // We only get here with non-exhausted indexes.
    // At least one of them is prepared and ready to read.
    TRI_ASSERT(!_indexesExhausted);
    _indexesExhausted = !skipIndex(atMost);
  }

  return _returned;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief frees the memory for all non-constant expressions
void IndexBlock::cleanupNonConstExpressions() {
  for (auto& it : _nonConstExpressions) {
    delete it;
  }
  _nonConstExpressions.clear();
}

/// @brief order a cursor for the index at the specified position
arangodb::OperationCursor* IndexBlock::orderCursor(size_t currentIndex) {
  AstNode const* conditionNode = nullptr;
  if (_condition != nullptr) {
    TRI_ASSERT(_indexes.size() == _condition->numMembers());
    TRI_ASSERT(_condition->numMembers() > currentIndex);

    conditionNode = _condition->getMember(currentIndex);
  }

  TRI_ASSERT(_indexes.size() > currentIndex);

  // TODO: if we have _nonConstExpressions, we should also reuse the
  // cursors, but in this case we have to adjust the iterator's search condition
  // from _condition
  if (!_nonConstExpressions.empty() || _cursors[currentIndex] == nullptr) {
    // yet no cursor for index, so create it
    IndexNode const* node = static_cast<IndexNode const*>(getPlanNode());
    _cursors[currentIndex].reset(_trx->indexScanForCondition(
        _indexes[currentIndex], conditionNode, node->outVariable(), _mmdr.get(),
        UINT64_MAX, transaction::Methods::defaultBatchSize(), node->_reverse));
  } else {
    // cursor for index already exists, reset and reuse it
    _cursors[currentIndex]->reset();
  }

  return _cursors[currentIndex].get();
}
