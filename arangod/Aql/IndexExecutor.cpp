////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "IndexExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Cluster/ServerState.h"
#include "ExecutorExpressionContext.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"
#include "VocBase/ManagedDocumentResult.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// resolve constant attribute accesses
static void resolveFCallConstAttributes(AstNode* fcall) {
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
}  // namespace

IndexExecutorInfos::IndexExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear, ExecutionEngine* engine,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    std::vector<std::string> const& projections, transaction::Methods* trxPtr,
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool allowCoveringIndexOptimization, bool useRawDocumentPointers,
    std::vector<std::unique_ptr<NonConstExpression>>&& nonConstExpression,
    std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs,
    bool hasV8Expression, AstNode const* condition,
    std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast,
    IndexIteratorOptions options)
    : ExecutorInfos(make_shared_unordered_set(),
                    make_shared_unordered_set({outputRegister}), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _indexes(indexes),
      //_invars,
      _condition(condition),
      _allowCoveringIndexOptimization(false),
      _ast(ast),
      // in_regs,
      _hasMultipleExpansions(false),
      _isLastIndex(false),
      _options(options),
      // currentIndex
      _cursor(nullptr),

      _indexesExhausted(false),
      _done(false),
      _cursors(indexes.size()),

      _outputRegisterId(outputRegister),
      _engine(engine),
      _collection(collection),
      _outVariable(outVariable),
      _projections(projections),
      _trxPtr(trxPtr),
      _expInVars(std::move(expInVars)),
      _expInRegs(std::move(expInRegs)),

      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _useRawDocumentPointers(useRawDocumentPointers),
      _nonConstExpression(std::move(nonConstExpression)),
      _produceResult(produceResult),
      _returned(0),
      _hasV8Expression(hasV8Expression) {}

IndexExecutor::IndexExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _state(ExecutionState::HASMORE),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _initDone(false) {
  _mmdr.reset(new ManagedDocumentResult);

  TRI_ASSERT(!_infos.getIndexes().empty());

  if (_infos.getCondition() != nullptr) {
    // fix const attribute accesses, e.g. { "a": 1 }.a
    for (size_t i = 0; i < _infos.getCondition()->numMembers(); ++i) {
      auto andCond = _infos.getCondition()->getMemberUnchecked(i);
      for (size_t j = 0; j < andCond->numMembers(); ++j) {
        auto leaf = andCond->getMemberUnchecked(j);

        // geo index condition i.e. GEO_CONTAINS, GEO_INTERSECTS
        if (leaf->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(leaf);
          continue;  //
        } else if (leaf->numMembers() != 2) {
          continue;  // Otherwise we only support binary conditions
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
  for (auto const& it : _infos.getIndexes()) {
    size_t expansions = 0;
    auto idx = it.getIndex();
    auto const& fields = idx->fields();
    for (size_t i = 0; i < fields.size(); ++i) {
      if (idx->isAttributeExpanded(i)) {
        ++expansions;
        if (expansions > 1 || i > 0) {
          infos.setHasMultipleExpansions(true);
          ;
          break;
        }
      }
    }
  }

  this->setProducingFunction(
      buildCallback(_documentProducer, _infos.getOutVariable(),
                    _infos.getProduceResult(), _infos.getProjections(),
                    _infos.getTrxPtr(), _infos.getCoveringIndexAttributePositions(),
                    true, _infos.getUseRawDocumentPointers()));  // remove true flag later, it is always true in any case (?)
};

IndexExecutor::~IndexExecutor() = default;

/// @brief order a cursor for the index at the specified position
arangodb::OperationCursor* IndexExecutor::orderCursor(size_t currentIndex) {
  TRI_ASSERT(_infos.getIndexes().size() > currentIndex);
  LOG_DEVEL << "ORDERING A CURSOR";

  // TODO: if we have _nonConstExpressions, we should also reuse the
  // cursors, but in this case we have to adjust the iterator's search condition
  // from _condition
  // if (!_infos.getNonConstExpressions().empty() || _infos.checkCursor(currentIndex)) {
  if (!_infos.getNonConstExpressions().empty() || _infos.getCursor(currentIndex) == nullptr) {
    AstNode const* conditionNode = nullptr;
    if (_infos.getCondition() != nullptr) {
      TRI_ASSERT(_infos.getIndexes().size() == _infos.getCondition()->numMembers());
      TRI_ASSERT(_infos.getCondition()->numMembers() > currentIndex);

      conditionNode = _infos.getCondition()->getMember(currentIndex);
    }

    // yet no cursor for index, so create it
    // IndexNode const* node = ExecutionNode::castTo<IndexNode const*>(getPlanNode()); // TODO remove me
    LOG_DEVEL << "new cursor?";
    _infos.resetCursor(currentIndex, _infos.getTrxPtr()->indexScanForCondition(
                                         _infos.getIndexes()[currentIndex],
                                         conditionNode, _infos.getOutVariable(),
                                         _mmdr.get(), _infos.getOptions()));
  } else {
    // cursor for index already exists, reset and reuse it
    LOG_DEVEL << "Reusing current index cursor";
    _infos.resetCursor(currentIndex);
  }

  return _infos.getCursor(currentIndex);
}

void IndexExecutor::createCursor() {
  _infos.setCursor(orderCursor(_infos.getCurrentIndex()));
}

/// @brief Forwards _iterator to the next available index
void IndexExecutor::startNextCursor() {
  if (!_infos.isAscending()) {
    _infos.decrCurrentIndex();
    _infos.setLastIndex(_infos.getCurrentIndex() == 0);
  } else {
    _infos.incrCurrentIndex();
    _infos.setLastIndex(_infos.getCurrentIndex() == _infos.getIndexes().size() - 1);
  }
  if (_infos.getCurrentIndex() < _infos.getIndexes().size()) {
    // This check will work as long as _indexes.size() < MAX_SIZE_T
    createCursor();
  } else {
    _infos.setCursor(nullptr);
  }
}

// this is called every time we need to fetch data from the indexes
bool IndexExecutor::readIndex(size_t atMost, IndexIterator::DocumentCallback const& callback) {
  // this is called every time we want to read the index.
  // For the primary key index, this only reads the index once, and never
  // again (although there might be multiple calls to this function).
  // For the edge, hash or skiplists indexes, initIndexes creates an iterator
  // and read*Index just reads from the iterator until it is done.
  // Then initIndexes is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents.

  if (_infos.getCursor() == nullptr || _infos.getIndexesExhausted()) {
    // All indexes exhausted
    return false;
  }

  LOG_DEVEL << "Returned is : " << _infos.getReturned();

  while (_infos.getCursor() != nullptr) {
    LOG_DEVEL << "iterating inside cursor";
    if (!_infos.getCursor()->hasMore()) {
      startNextCursor();
      continue;
    }

    TRI_ASSERT(atMost >= _infos.getReturned());

     if (_infos.getReturned() == atMost) {
       // We have returned enough, do not check if we have more
       return true;
     }

    TRI_IF_FAILURE("IndexBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(atMost >= _infos.getReturned());

    bool res;
    if (!_infos.getProduceResult()) {
      // optimization: iterate over index (e.g. for filtering), but do not fetch
      // the actual documents
      res = _infos.getCursor()->next(
          [&callback](LocalDocumentId const& id) {
            callback(id, VPackSlice::nullSlice());
          },
          atMost);
    } else {
      // check if the *current* cursor supports covering index queries or not
      // if we can optimize or not must be stored in our instance, so the
      // DocumentProducingBlock can access the flag

      TRI_ASSERT(_infos.getCursor() != nullptr);
      _infos.setAllowCoveringIndexOptimization(_infos.getCursor()->hasCovering());

      if (_infos.getAllowCoveringIndexOptimization() &&
          !_infos.getCoveringIndexAttributePositions().empty()) {
        // index covers all projections
        res = _infos.getCursor()->nextCovering(callback, atMost);
      } else {
        // we need the documents later on. fetch entire documents
        res = _infos.getCursor()->nextDocument(callback, atMost);
      }
    }

    if (res) {
      LOG_DEVEL << "We have more";
      // We have returned enough.
      // And this index could return more.
      // We are good.
      return true;
    }
  }
  // if we get here the indexes are exhausted.
  LOG_DEVEL << "We do not have more";
  return false;
}

// TODO why copy of input row?
bool IndexExecutor::initIndexes(InputAqlItemRow& input) {
  // We start with a different context. Return documents found in the previous
  // context again.
  _alreadyReturned.clear();
  // Find out about the actual values for the bounds in the variable bound case:

  if (!_infos.getNonConstExpressions().empty()) {
    TRI_ASSERT(_infos.getCondition() != nullptr);

    if (_infos.getV8Expression()) {
      // must have a V8 context here to protect Expression::execute()
      auto cleanup = [this]() {
        if (arangodb::ServerState::instance()->isRunningInCluster()) {
          // must invalidate the expression now as we might be called from
          // different threads
          for (auto const& e : _infos.getNonConstExpressions()) {
            e->expression->invalidate();
          }

          _infos.getEngine()->getQuery()->exitContext();
        }
      };

      _infos.getEngine()->getQuery()->enterContext();
      TRI_DEFER(cleanup());

      ISOLATE;
      v8::HandleScope scope(isolate);  // do not delete this!

      executeExpressions(input);
      TRI_IF_FAILURE("IndexBlock::executeV8") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } else {
      // no V8 context required!
      executeExpressions(input);
      TRI_IF_FAILURE("IndexBlock::executeExpression") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
  }
  if (!_infos.isAscending()) {
    _infos.setCurrentIndex(_infos.getIndexes().size() - 1);
  } else {
    _infos.setCurrentIndex(0);
  }

  createCursor();
  if (_infos.getCursor()->fail()) {
    THROW_ARANGO_EXCEPTION(_infos.getCursor()->code);
  }

  TRI_ASSERT(_infos.getCursor() != nullptr);
  while (!_infos.getCursor()->hasMore()) {
    if (_infos.isAscending()) {
      _infos.decrCurrentIndex();
    } else {
      _infos.incrCurrentIndex();
    }
    if (_infos.getCurrentIndex() < _infos.getIndexes().size()) {
      // This check will work as long as _indexes.size() < MAX_SIZE_T
      createCursor();
      if (_infos.getCursor()->fail()) {
        THROW_ARANGO_EXCEPTION(_infos.getCursor()->code);
      }
    } else {
      _infos.setCursor(nullptr);
      _infos.setIndexesExhausted(true);
      // We were not able to initialize any index with this condition
      return false;
    }
  }
  _infos.setIndexesExhausted(false);
  return true;
}

void IndexExecutor::executeExpressions(InputAqlItemRow& input) {
  TRI_ASSERT(_infos.getCondition() != nullptr);
  TRI_ASSERT(!_infos.getNonConstExpressions().empty());

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  auto ast = _infos.getAst();
  AstNode* condition = const_cast<AstNode*>(_infos.getCondition());

  // modify the existing node in place
  TEMPORARILY_UNLOCK_NODE(condition);

  Query* query = _infos.getEngine()->getQuery();

  for (size_t posInExpressions = 0;
       posInExpressions < _infos.getNonConstExpressions().size(); ++posInExpressions) {
    NonConstExpression* toReplace =
        _infos.getNonConstExpressions()[posInExpressions].get();
    auto exp = toReplace->expression.get();

    ExecutorExpressionContext ctx(query, input, _infos.getExpInVars(),
                                  _infos.getExpInRegs());

    bool mustDestroy;
    AqlValue a = exp->execute(_infos.getTrxPtr(), &ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(_infos.getTrxPtr());
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

std::pair<ExecutionState, IndexStats> IndexExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("IndexExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  IndexStats stats{};

  while (true) {
    if (_infos.getIndexesExhausted() || !_initDone) {
      LOG_DEVEL << "fetched a new row";
      std::tie(_state, _input) = _fetcher.fetchRow();

      if (_state == ExecutionState::WAITING) {
        return {_state, stats};
      }

      if (!_input) {
        TRI_ASSERT(_state == ExecutionState::DONE);
        return {_state, stats};
      }

      if (!initIndexes(_input)) {
        _infos.setDone(true);
        break;
      }

      _initDone = true;
    } else {
      LOG_DEVEL << "no need to fetch more";
    }

    if (_infos.getDone() && _state == ExecutionState::DONE) {
      return {ExecutionState::DONE, stats};
    }

    if (_state == ExecutionState::WAITING) {
      return {_state, stats};
    }

    if (!_input) {
      TRI_ASSERT(_state == ExecutionState::DONE);
      return {_state, stats};
    }
    LOG_DEVEL << "INPUT NEEDS TO BE INITIALIZED HERE";
    TRI_ASSERT(_input.isInitialized());

    // TODO  init indizes, right position?
    // initIndexes(input);
   /* if (!initIndexes(input)) {
      _infos.setDone(true);
      break;
    }*/

    if (_infos.getDone()) {
      return {_state, stats};
    }

    // LOGIC HERE
    IndexIterator::DocumentCallback callback;
    // size_t const nrInRegs = _infos.numberOfInputRegisters(); // TODO REMOVE ME

    if (_infos.getIndexes().size() > 1 || _infos.hasMultipleExpansions()) {
      // Activate uniqueness checks
      callback = [this, &output](LocalDocumentId const& token, VPackSlice slice) {
        if (!_infos.isLastIndex()) {
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

        _documentProducer(this->_input, output, slice, _infos.getOutputRegisterId());
      };
    } else {
      // No uniqueness checks
      callback = [this, &output](LocalDocumentId const&, VPackSlice slice) {
        _documentProducer(this->_input, output, slice, _infos.getOutputRegisterId());
      };
    }

    if (_infos.getIndexesExhausted()) {
      if (!initIndexes(_input)) {
        // not found any index, so we are done
        _infos.setDone(true);
        break;
      }
      TRI_ASSERT(!_infos.getIndexesExhausted());
    }

    // We only get here with non-exhausted indexes.
    // At least one of them is prepared and ready to read.
    TRI_ASSERT(!_infos.getIndexesExhausted());

    // Read the next elements from the indexes
    auto saveReturned = _infos.getReturned();
    LOG_DEVEL << "Reading index";
    _infos.setIndexesExhausted(!readIndex(1, callback));
    LOG_DEVEL << "index is exhausted: " << _infos.getIndexesExhausted();
    if (_infos.getReturned() != saveReturned) {
      // Update statistics
      stats.incrScanned(_infos.getReturned() - saveReturned);
    }

    if (_infos.getIndexesExhausted()) {
      _infos.setDone(true);
      return {ExecutionState::DONE, stats};
    }

    return {ExecutionState::HASMORE, stats};
  }
}
