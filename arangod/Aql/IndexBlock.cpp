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
#include "Aql/BaseExpressionContext.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
  /// resolve constant attribute accesses
  static void resolveFCallConstAttributes(AstNode *fcall) {
    TRI_ASSERT(fcall->type == NODE_TYPE_FCALL);
    TRI_ASSERT(fcall->numMembers() == 1);
    AstNode* array = fcall->getMemberUnchecked(0);
    for (size_t x = 0; x < array->numMembers(); x++) {
      AstNode* child = array->getMemberUnchecked(x);
      if (child->type == NODE_TYPE_ATTRIBUTE_ACCESS && child->isConstant()) {
        child = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(child));
        array->changeMember(x, child);
      }
    }
  }
}

IndexBlock::IndexBlock(ExecutionEngine* engine, IndexNode const* en)
    : ExecutionBlock(engine, en),
      DocumentProducingBlock(en, _trx),
      _collection(en->collection()),
      _currentIndex(0),
      _indexes(en->getIndexes()),
      _cursor(nullptr),
      _cursors(_indexes.size()),
      _condition(en->_condition->root()),
      _hasV8Expression(false),
      _indexesExhausted(false),
      _isLastIndex(false),
      _hasMultipleExpansions(false),
      _returned(0),
      _copyFromRow(0),
      _resultInFlight(nullptr) {
  _mmdr.reset(new ManagedDocumentResult);

  TRI_ASSERT(!_indexes.empty());

  if (_condition != nullptr) {
    // fix const attribute accesses, e.g. { "a": 1 }.a
    for (size_t i = 0; i < _condition->numMembers(); ++i) {
      auto andCond = _condition->getMemberUnchecked(i);
      for (size_t j = 0; j < andCond->numMembers(); ++j) {
        auto leaf = andCond->getMemberUnchecked(j);

        // geo index condition i.e. GEO_CONTAINS, GEO_INTERSECTS
        if (leaf->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(leaf);
          continue; //
        } else if (leaf->numMembers() != 2) {
          continue; // Otherwise we only support binary conditions
        }

        TRI_ASSERT(leaf->numMembers() == 2);
        AstNode* lhs = leaf->getMemberUnchecked(0);
        AstNode* rhs = leaf->getMemberUnchecked(1);
        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->isConstant()) {
          lhs = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(lhs));
          leaf->changeMember(0, lhs);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->isConstant()) {
          rhs = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(rhs));
          leaf->changeMember(1, rhs);
        }
        // geo index condition i.e. `GEO_DISTANCE(x, y) <= d`
        if (lhs->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(lhs);
        }
      }
    }
  }

  // count how many attributes in the index are expanded (array index)
  // if more than a single attribute, we always need to deduplicate the
  // result later on
  for (auto const& it : _indexes) {
    size_t expansions = 0;
    auto idx = it.getIndex();
    auto const& fields = idx->fields();
    for (size_t i = 0; i < fields.size(); ++i) {
      if (idx->isAttributeExpanded(i)) {
        ++expansions;
        if (expansions > 1 || i > 0) {
          _hasMultipleExpansions = true;
          break;
        }
      }
    }
  }

  // build the _documentProducer callback for extracting
  // documents from the index
  buildCallback();

  initializeOnce();
}

IndexBlock::~IndexBlock() { cleanupNonConstExpressions(); }

/// @brief adds a UNIQUE() to a dynamic IN condition
arangodb::aql::AstNode* IndexBlock::makeUnique(
    arangodb::aql::AstNode* node) const {
  if (node->type != arangodb::aql::NODE_TYPE_ARRAY || node->numMembers() >= 2) {
    // an non-array or an array with more than 1 member
    auto en = ExecutionNode::castTo<IndexNode const*>(getPlanNode());
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
      return ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("SORTED_UNIQUE"),
                                         array);
    }
    // a regular UNIQUE will do
    return ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("UNIQUE"), array);
  }

  // presumably an array with no or a single member
  return node;
}

void IndexBlock::executeExpressions() {
  TRI_ASSERT(_condition != nullptr);
  TRI_ASSERT(!_nonConstExpressions.empty());

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:

  AqlItemBlock* cur = _buffer.front();
  auto en = ExecutionNode::castTo<IndexNode const*>(getPlanNode());
  auto ast = en->_plan->getAst();
  AstNode* condition = const_cast<AstNode*>(_condition);

  // modify the existing node in place
  TEMPORARILY_UNLOCK_NODE(condition);
  
  Query* query = _engine->getQuery();

  for (size_t posInExpressions = 0;
       posInExpressions < _nonConstExpressions.size(); ++posInExpressions) {
    NonConstExpression* toReplace = _nonConstExpressions[posInExpressions].get();
    auto exp = toReplace->expression.get();

    bool mustDestroy;
    BaseExpressionContext ctx(query, _pos, cur, _inVars[posInExpressions],
                              _inRegs[posInExpressions]);
    AqlValue a = exp->execute(_trx, &ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(_trx);
    VPackSlice slice = materializer.slice(a, false);
    AstNode* evaluatedNode = ast->nodeFromVPack(slice, true);

    AstNode* tmp = condition;
    for (size_t x = 0; x < toReplace->indexPath.size(); x++) {
      size_t idx = toReplace->indexPath[x];
      AstNode* old = tmp->getMember(idx);
      // modify the node in place
      TEMPORARILY_UNLOCK_NODE(tmp);
      if (x + 1 < toReplace->indexPath.size()) {
        AstNode* cpy = old; 
        tmp->changeMember(idx, cpy);
        tmp = cpy;
      } else {
        // insert the actual expression value
        tmp->changeMember(idx, evaluatedNode);
      }
    }
  }
}

void IndexBlock::initializeOnce() {
  auto en = ExecutionNode::castTo<IndexNode const*>(getPlanNode());
  auto ast = en->_plan->getAst();
      
  _trx->pinData(_collection->id());

  // instantiate expressions:
  auto instantiateExpression = [&](AstNode* a,
                                   std::vector<size_t>&& idxs) -> void {
    // all new AstNodes are registered with the Ast in the Query
    auto e = std::make_unique<Expression>(en->_plan, ast, a);

    TRI_IF_FAILURE("IndexBlock::initialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasV8Expression |= e->willUseV8();

    std::unordered_set<Variable const*> inVars;
    e->variables(inVars);

    _nonConstExpressions.emplace_back(std::make_unique<NonConstExpression>(std::move(e), std::move(idxs)));

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
    // this node has no condition. Iterate over the complete index.
    return;
  }

  auto outVariable = en->outVariable();
  std::function<bool(AstNode const*)> hasOutVariableAccess =
    [&](AstNode const* node) -> bool {
    if (node->isAttributeAccessForVariable(outVariable, true)) {
      return true;
    }

    bool accessedInSubtree = false;
    for (size_t i = 0; i < node->numMembers() && !accessedInSubtree; i++) {
      accessedInSubtree = hasOutVariableAccess(node->getMemberUnchecked(i));
    }

    return accessedInSubtree;
  };

  auto instFCallArgExpressions = [&](AstNode* fcall,
                                     std::vector<size_t>&& indexPath) {
    TRI_ASSERT(1 == fcall->numMembers());
    indexPath.emplace_back(0); // for the arguments array
    AstNode* array = fcall->getMemberUnchecked(0);
    for (size_t k = 0; k < array->numMembers(); k++) {
      AstNode* child = array->getMemberUnchecked(k);
      if (!child->isConstant() && !hasOutVariableAccess(child)) {
        std::vector<size_t> idx = indexPath;
        idx.emplace_back(k);
        instantiateExpression(child, std::move(idx));

        TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      }
    }
  };

  // conditions can be of the form (a [<|<=|>|=>] b) && ...
  // in case of a geo spatial index a might take the form
  // of a GEO_* function. We might need to evaluate fcall arguments
  for (size_t i = 0; i < _condition->numMembers(); ++i) {
    auto andCond = _condition->getMemberUnchecked(i);
    for (size_t j = 0; j < andCond->numMembers(); ++j) {
      auto leaf = andCond->getMemberUnchecked(j);

      // FCALL at this level is most likely a geo index
      if (leaf->type == NODE_TYPE_FCALL) {
        instFCallArgExpressions(leaf, {i, j});
        continue;
      } else if (leaf->numMembers() != 2) {
        continue;
      }

      // We only support binary conditions
      TRI_ASSERT(leaf->numMembers() == 2);
      AstNode* lhs = leaf->getMember(0);
      AstNode* rhs = leaf->getMember(1);

      if (lhs->isAttributeAccessForVariable(outVariable, false)) {
        // Index is responsible for the left side, check if right side
        // has to be evaluated
        if (!rhs->isConstant()) {
          if (leaf->type == NODE_TYPE_OPERATOR_BINARY_IN) {
            rhs = makeUnique(rhs);
          }
          instantiateExpression(rhs, {i, j, 1});
          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      } else {
        // Index is responsible for the right side, check if left side
        // has to be evaluated

        if (lhs->type == NODE_TYPE_FCALL && !en->options().evaluateFCalls) {
          // most likely a geo index condition
          instFCallArgExpressions(lhs, {i, j, 0});
        } else if (!lhs->isConstant()) {
          instantiateExpression(lhs, {i, j, 0});
          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
    }
  }
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
  // We start with a different context. Return documents found in the previous
  // context again.
  _alreadyReturned.clear();
  // Find out about the actual values for the bounds in the variable bound case:

  if (!_nonConstExpressions.empty()) {
    TRI_ASSERT(_condition != nullptr);

    if (_hasV8Expression) {
      // must have a V8 context here to protect Expression::execute()
      auto cleanup = [this]() {
        if (arangodb::ServerState::instance()->isRunningInCluster()) {
          // must invalidate the expression now as we might be called from
          // different threads
          for (auto const& e : _nonConstExpressions) {
            e->expression->invalidate();
          }

          _engine->getQuery()->exitContext();
        }
      };

      _engine->getQuery()->enterContext();
      TRI_DEFER(cleanup());

      ISOLATE;
      v8::HandleScope scope(isolate);  // do not delete this!

      executeExpressions();
      TRI_IF_FAILURE("IndexBlock::executeV8") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } else {
      // no V8 context required!
      executeExpressions();
      TRI_IF_FAILURE("IndexBlock::executeExpression") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
  }
  IndexNode const* node = ExecutionNode::castTo<IndexNode const*>(getPlanNode());
  if (!node->options().ascending) {
    _currentIndex = _indexes.size() - 1;
  } else {
    _currentIndex = 0;
  }

  createCursor();
  if (_cursor->fail()) {
    THROW_ARANGO_EXCEPTION(_cursor->code);
  }

  while (!_cursor->hasMore()) {
    if (!node->options().ascending) {
      --_currentIndex;
    } else {
      ++_currentIndex;
    }
    if (_currentIndex < _indexes.size()) {
      // This check will work as long as _indexes.size() < MAX_SIZE_T
      createCursor();
      if (_cursor->fail()) {
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
}

/// @brief create an OperationCursor object
void IndexBlock::createCursor() {
  _cursor = orderCursor(_currentIndex);
}

/// @brief Forwards _iterator to the next available index
void IndexBlock::startNextCursor() {
  IndexNode const* node = ExecutionNode::castTo<IndexNode const*>(getPlanNode());
  if (!node->options().ascending) {
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
}

// this is called every time we just skip in the index
bool IndexBlock::skipIndex(size_t atMost) {
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

    uint64_t returned = static_cast<uint64_t>(_returned);
    _cursor->skip(atMost - returned, returned);
    _engine->_stats.scannedIndex += returned;
    _returned = static_cast<size_t>(returned);

    return true;
  }
  return false;
}

// this is called every time we need to fetch data from the indexes
bool IndexBlock::readIndex(size_t atMost,
                           IndexIterator::DocumentCallback const& callback) {
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

    bool res;
    if (!produceResult()) {
      // optimization: iterate over index (e.g. for filtering), but do not fetch the
      // actual documents
      res = _cursor->next([&callback](LocalDocumentId const& id) {
        callback(id, VPackSlice::nullSlice());
      }, atMost - _returned);
    } else {
      // check if the *current* cursor supports covering index queries or not
      // if we can optimize or not must be stored in our instance, so the
      // DocumentProducingBlock can access the flag
      _allowCoveringIndexOptimization = _cursor->hasCovering();

      if (_allowCoveringIndexOptimization &&
          !ExecutionNode::castTo<IndexNode const*>(_exeNode)->coveringIndexAttributePositions().empty()) {
        // index covers all projections
        res = _cursor->nextCovering(callback, atMost - _returned);
      } else {
        // we need the documents later on. fetch entire documents
       res = _cursor->nextDocument(callback, atMost - _returned);
      }
    }

    if (res) {
      // We have returned enough.
      // And this index could return more.
      // We are good.
      return true;
    }
  }
  // if we get here the indexes are exhausted.
  return false;
}

std::pair<ExecutionState, Result> IndexBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _alreadyReturned.clear();
  _returned = 0;
  _pos = 0;
  _currentIndex = 0;
  _resultInFlight.reset();
  _copyFromRow = 0;

  return res;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> IndexBlock::getSome(
    size_t atMost) {
  traceGetSomeBegin(atMost);
  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  TRI_ASSERT(atMost > 0);
  size_t const nrInRegs = getNrInputRegisters();
  if (_resultInFlight == nullptr) {
    // We handed sth out last call and need to reset now.
    TRI_ASSERT(_returned == 0);
    TRI_ASSERT(_copyFromRow == 0);
    _resultInFlight.reset(requestBlock(atMost, getNrOutputRegisters()));
  }

  // The following callbacks write one index lookup result into res at
  // position _returned:

  IndexIterator::DocumentCallback callback;

  if (_indexes.size() > 1 || _hasMultipleExpansions) {
    // Activate uniqueness checks
    callback = [this, nrInRegs](LocalDocumentId const& token, VPackSlice slice) {
      TRI_ASSERT(_resultInFlight != nullptr);
      if (!_isLastIndex) {
        // insert & check for duplicates in one go
        if (!_alreadyReturned.insert(token.id()).second) {
          // Document already in list. Skip this
          return;
        }
      } else {
        // only check for duplicates
        if (_alreadyReturned.find(token.id()) != _alreadyReturned.end()) {
          // Document found, skip
          return;
        }
      }

      _documentProducer(_resultInFlight.get(), slice, nrInRegs, _returned, _copyFromRow);
    };
  } else {
    // No uniqueness checks
    callback = [this, nrInRegs](LocalDocumentId const&, VPackSlice slice) {
      TRI_ASSERT(_resultInFlight != nullptr);
      _documentProducer(_resultInFlight.get(), slice, nrInRegs, _returned, _copyFromRow);
    };
  }

  do {
    if (_buffer.empty()) {
      if (_upstreamState == ExecutionState::DONE) {
        _done = true;
        break;
      }

      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      ExecutionState state;
      bool blockAppended;
      std::tie(state, blockAppended) = ExecutionBlock::getBlock(toFetch);
      if (state == ExecutionState::WAITING) {
        TRI_ASSERT(!blockAppended);
        traceGetSomeEnd(_resultInFlight.get(), ExecutionState::WAITING);
        return {ExecutionState::WAITING, nullptr};
      }
      if (!blockAppended || !initIndexes()) {
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
        if (_upstreamState == ExecutionState::DONE) {
          _done = true;
          break;
        }
        ExecutionState state;
        bool blockAppended;
        std::tie(state, blockAppended) =
            ExecutionBlock::getBlock(DefaultBatchSize());
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!blockAppended);
          traceGetSomeEnd(_resultInFlight.get(), ExecutionState::WAITING);
          return {ExecutionState::WAITING, nullptr};
        }
        if (!blockAppended) {
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
    TRI_ASSERT(nrInRegs == cur->getNrRegs());

    TRI_ASSERT(nrInRegs <= _resultInFlight->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, _resultInFlight.get(), _pos, _returned);
    _copyFromRow = _returned;

    // Read the next elements from the indexes
    auto saveReturned = _returned;
    _indexesExhausted = !readIndex(atMost, callback);
    if (_returned == saveReturned) {
      // No results. Kill the registers:
      for (arangodb::aql::RegisterId i = 0; i < nrInRegs; ++i) {
        _resultInFlight->destroyValue(_returned, i);
      }
    } else {
      // Update statistics
      _engine->_stats.scannedIndex += _returned - saveReturned;
    }

  } while (_returned < atMost);

  // Now there are three cases:
  //   (1) The AqlItemBlock is empty (no result for any input or index)
  //   (2) The AqlItemBlock is half-full (0 < _returned < atMost)
  //   (3) The AqlItemBlock is full (_returned == atMost)
  if (_returned == 0) {
    TRI_ASSERT(_copyFromRow == 0);
    AqlItemBlock* dummy = _resultInFlight.release();
    returnBlock(dummy);
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(_resultInFlight.get(), getHasMoreState());
    return {getHasMoreState(), nullptr};
  }
  if (_returned < atMost) {
    _resultInFlight->shrink(_returned);
  }

  _returned = 0;
  _copyFromRow = 0;
  // Clear out registers no longer needed later:
  clearRegisters(_resultInFlight.get());
  traceGetSomeEnd(_resultInFlight.get(), getHasMoreState());

  return {getHasMoreState(), std::move(_resultInFlight)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> IndexBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  _returned = 0;

  while (_returned < atMost) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      ExecutionState state;
      bool blockAppended;
      std::tie(state, blockAppended) = ExecutionBlock::getBlock(toFetch);
      if (state == ExecutionState::WAITING) {
        TRI_ASSERT(!blockAppended);
        traceSkipSomeEnd(0, ExecutionState::WAITING);
        return {ExecutionState::WAITING, 0};
      }
      if (!blockAppended || !initIndexes()) {
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
        ExecutionState state;
        bool blockAppended;
        std::tie(state, blockAppended) = ExecutionBlock::getBlock(DefaultBatchSize());
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!blockAppended);
          traceSkipSomeEnd(0, ExecutionState::WAITING);
          return {ExecutionState::WAITING, 0};
        }
        if (!blockAppended) {
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

  size_t returned = _returned;
  _returned = 0;
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(returned, state);
  return {state, returned};
}

/// @brief frees the memory for all non-constant expressions
void IndexBlock::cleanupNonConstExpressions() {
  _nonConstExpressions.clear();
}

/// @brief order a cursor for the index at the specified position
arangodb::OperationCursor* IndexBlock::orderCursor(size_t currentIndex) {
  TRI_ASSERT(_indexes.size() > currentIndex);

  // TODO: if we have _nonConstExpressions, we should also reuse the
  // cursors, but in this case we have to adjust the iterator's search condition
  // from _condition
  if (!_nonConstExpressions.empty() || _cursors[currentIndex] == nullptr) {
    AstNode const* conditionNode = nullptr;
    if (_condition != nullptr) {
      TRI_ASSERT(_indexes.size() == _condition->numMembers());
      TRI_ASSERT(_condition->numMembers() > currentIndex);

      conditionNode = _condition->getMember(currentIndex);
    }

    // yet no cursor for index, so create it
    IndexNode const* node = ExecutionNode::castTo<IndexNode const*>(getPlanNode());
    _cursors[currentIndex].reset(_trx->indexScanForCondition(
        _indexes[currentIndex], conditionNode, node->outVariable(), _mmdr.get(),
        node->_options));
  } else {
    // cursor for index already exists, reset and reuse it
    _cursors[currentIndex]->reset();
  }

  return _cursors[currentIndex].get();
}
