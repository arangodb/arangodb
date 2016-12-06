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
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Functions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

IndexBlock::IndexBlock(ExecutionEngine* engine, IndexNode const* en)
    : ExecutionBlock(engine, en),
      _collection(en->collection()),
      _posInDocs(0),
      _currentIndex(0),
      _indexes(en->getIndexes()),
      _cursor(nullptr),
      _cursors(_indexes.size()),
      _condition(en->_condition->root()),
      _hasV8Expression(false) {
    
  _mmdr.reset(new ManagedDocumentResult(transaction()));
}

IndexBlock::~IndexBlock() {
  cleanupNonConstExpressions();
}

/// @brief adds a UNIQUE() to a dynamic IN condition
arangodb::aql::AstNode* IndexBlock::makeUnique(
    arangodb::aql::AstNode* node) const {
  if (node->type != arangodb::aql::NODE_TYPE_ARRAY ||
      node->numMembers() >= 2) {
    // an non-array or an array with more than 1 member
    auto en = static_cast<IndexNode const*>(getPlanNode());
    auto ast = en->_plan->getAst();
    auto array = ast->createNodeArray();
    array->addMember(node);
    auto trx = transaction();
    bool isSorted = false;
    bool isSparse = false;
    auto unused = trx->getIndexFeatures(_indexes[_currentIndex], isSorted, isSparse);
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
  auto instantiateExpression =
      [&](size_t i, size_t j, size_t k, AstNode* a) -> void {
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

      // We only support binary conditions
      TRI_ASSERT(leaf->numMembers() == 2);
      auto lhs = leaf->getMember(0);
      auto rhs = leaf->getMember(1);

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
      // We were not able to initialize any index with this condition
      return false;
    }
  }
  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief create an OperationCursor object
void IndexBlock::createCursor() {
  DEBUG_BEGIN_BLOCK();
  _cursor = orderCursor(_currentIndex);
  _result.clear();
  DEBUG_END_BLOCK();
}

/// @brief Forwards _iterator to the next available index
void IndexBlock::startNextCursor() {
  DEBUG_BEGIN_BLOCK();

  IndexNode const* node = static_cast<IndexNode const*>(getPlanNode());
  if (node->_reverse) {
    --_currentIndex;
  } else {
    ++_currentIndex;
  }
  if (_currentIndex < _indexes.size()) {
    // This check will work as long as _indexes.size() < MAX_SIZE_T
    createCursor();
  } else {
    _cursor = nullptr;
  }
  DEBUG_END_BLOCK();
}

// this is called every time everything in _documents has been passed on

bool IndexBlock::readIndex(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  // this is called every time we want more in _documents.
  // For the primary key index, this only reads the index once, and never
  // again (although there might be multiple calls to this function).
  // For the edge, hash or skiplists indexes, initIndexes creates an iterator
  // and read*Index just reads from the iterator until it is done.
  // Then initIndexes is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents.

  if (_documents.empty()) {
    TRI_IF_FAILURE("IndexBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _documents.reserve(atMost);
  } else {
    _documents.clear();
  }

  if (_cursor == nullptr) {
    // All indexes exhausted
    return false;
  }

  size_t lastIndexNr = _indexes.size() - 1;
  bool isReverse = (static_cast<IndexNode const*>(getPlanNode()))->_reverse;
  bool isLastIndex = (_currentIndex == lastIndexNr && !isReverse) ||
                     (_currentIndex == 0 && isReverse);
  bool const hasMultipleIndexes = (_indexes.size() > 1); 

  while (_cursor != nullptr) {
    if (!_cursor->hasMore()) {
      startNextCursor();
      continue;
    }

    LogicalCollection* collection = _cursor->collection();
    _cursor->getMoreMptr(_result, atMost);

    size_t length = _result.size();

    if (length == 0) {
      startNextCursor();
      continue;
    }

    _engine->_stats.scannedIndex += length;

    TRI_IF_FAILURE("IndexBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
     

    if (hasMultipleIndexes) {
      for (auto const& element : _result) {
        TRI_voc_rid_t revisionId = element.revisionId();
        if (collection->readRevision(_trx, *_mmdr, revisionId)) {
          uint8_t const* vpack = _mmdr->vpack(); //back();
          // uniqueness checks
          if (!isLastIndex) {
            // insert & check for duplicates in one go
            if (_alreadyReturned.emplace(revisionId).second) {
              _documents.emplace_back(vpack);
            }
          } else {
            // only check for duplicates
            if (_alreadyReturned.find(revisionId) == _alreadyReturned.end()) {
              _documents.emplace_back(vpack);
            }
          }
        }
      }
    } else {
      for (auto const& element : _result) {
        TRI_voc_rid_t revisionId = element.revisionId();
        if (collection->readRevision(_trx, *_mmdr, revisionId)) {
          uint8_t const* vpack = _mmdr->vpack(); //back();
          _documents.emplace_back(vpack);
        }
      } 
    }
    // Leave the loop here, we can only exhaust one cursor at a time, otherwise slices are lost
    if (!_documents.empty()) {
      break;
    }
  }
  _posInDocs = 0;
  return (!_documents.empty());

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int IndexBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  _alreadyReturned.clear();
  _pos = 0;
  _posInDocs = 0;

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
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> res;

  do {
    // repeatedly try to get more stuff from upstream
    // note that the value of the variable we have to loop over
    // can contain zero entries, in which case we have to
    // try again!

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch) || (!initIndexes())) {
        _done = true;
        traceGetSomeEnd(nullptr);
        return nullptr;
      }
      _pos = 0;  // this is in the first block

      // This is a new item, so let's read the index (it is already
      // initialized).
      readIndex(atMost);
    } else if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,

      if (!readIndex(atMost)) {  // no more output from this version of the
                                 // index
        AqlItemBlock* cur = _buffer.front();
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          delete cur;
          _pos = 0;
        }
        if (_buffer.empty()) {
          if (!ExecutionBlock::getBlock(DefaultBatchSize(), DefaultBatchSize())) {
            _done = true;
            traceGetSomeEnd(nullptr);
            return nullptr;
          }
          _pos = 0;  // this is in the first block
        }

        if (!initIndexes()) {
          _done = true;
          traceGetSomeEnd(nullptr);
          return nullptr;
        }
        readIndex(atMost);
      }
    }

    // If we get here, we do have _buffer.front() and _pos points into it
    AqlItemBlock* cur = _buffer.front();
    size_t const curRegs = cur->getNrRegs();

    size_t available = _documents.size() - _posInDocs;
    size_t toSend = (std::min)(atMost, available);

    if (toSend > 0) {
      // automatically freed should we throw
      res.reset(
        new AqlItemBlock(_engine->getQuery()->resourceMonitor(),
        toSend,
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()])
      );

      TRI_ASSERT(curRegs <= res->getNrRegs());

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(cur, res.get(), _pos);

      for (size_t j = 0; j < toSend; j++) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        auto doc = _documents[_posInDocs++];
        TRI_ASSERT(!doc.isExternal());
        // doc points directly into the data files
        res->setValue(j, static_cast<arangodb::aql::RegisterId>(curRegs), 
                      AqlValue(doc.begin(), AqlValueFromManagedDocument()));
        // No harm done, if the setValue throws!
        
        if (j > 0) {
          // re-use already copied AqlValues
          res->copyValuesFromFirstRow(j, static_cast<RegisterId>(curRegs));
        }
      }
    }

  } while (res.get() == nullptr);

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

  size_t skipped = 0;

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch) || (!initIndexes())) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block

      // This is a new item, so let's read the index if bounds are variable:
      readIndex(atMost);
    }

    size_t available = _documents.size() - _posInDocs;
    size_t toSkip = (std::min)(atMost - skipped, available);

    _posInDocs += toSkip;
    skipped += toSkip;

    // Advance read position:
    if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,
      if (!readIndex(atMost)) {
        // If we get here, we do have _buffer.front() and _pos points into it
        AqlItemBlock* cur = _buffer.front();

        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          delete cur;
          _pos = 0;
        }

        // let's read the index if bounds are variable:
        if (!_buffer.empty()) {
          if (!initIndexes()) {
            _done = true;
            return skipped;
          }
          readIndex(atMost);
        }
      }

      // If _buffer is empty, then we will fetch a new block in the next round
      // and then read the index.
    }
  }

  return skipped;

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
      _indexes[currentIndex], 
      conditionNode,
      node->outVariable(), 
      _mmdr.get(),
      UINT64_MAX, 
      Transaction::defaultBatchSize(),
      node->_reverse
    ));
  } else {
    // cursor for index already exists, reset and reuse it
    _cursors[currentIndex]->reset();
  }

  return _cursors[currentIndex].get();
}
