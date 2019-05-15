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

#include <lib/Logger/LogMacros.h>

#include <memory>
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
static inline DocumentProducingFunctionContext createContext(InputAqlItemRow const& inputRow,
                                                             IndexExecutorInfos& infos) {
  return DocumentProducingFunctionContext(
      inputRow, nullptr, infos.getOutputRegisterId(), infos.getProduceResult(),
      infos.getProjections(), infos.getTrxPtr(),
      infos.getCoveringIndexAttributePositions(), false, infos.getUseRawDocumentPointers(),
      infos.getIndexes().size() > 1 || infos.hasMultipleExpansions());
}
}  // namespace

IndexExecutorInfos::IndexExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep, ExecutionEngine* engine,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    std::vector<std::string> const& projections, transaction::Methods* trxPtr,
    std::vector<size_t> const& coveringIndexAttributePositions, bool useRawDocumentPointers,
    std::vector<std::unique_ptr<NonConstExpression>>&& nonConstExpression,
    std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs,
    bool hasV8Expression, AstNode const* condition,
    std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast,
    IndexIteratorOptions options)
    : ExecutorInfos(make_shared_unordered_set(),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _indexes(std::move(indexes)),
      _condition(condition),
      _ast(ast),
      _hasMultipleExpansions(false),
      _options(options),
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
      _hasV8Expression(hasV8Expression) {
  if (_condition != nullptr) {
    // fix const attribute accesses, e.g. { "a": 1 }.a
    for (size_t i = 0; i < _condition->numMembers(); ++i) {
      auto andCond = _condition->getMemberUnchecked(i);
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
  for (auto const& it : getIndexes()) {
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
}

IndexExecutor::CursorReader::CursorReader(IndexExecutorInfos const& infos,
                                          AstNode const* condition,
                                          transaction::Methods::IndexHandle const& index,
                                          DocumentProducingFunctionContext& context,
                                          bool checkUniqueness)
    : _infos(infos),
      _condition(condition),
      _index(index),
      _cursor(std::make_unique<OperationCursor>(infos.getTrxPtr()->indexScanForCondition(
          index, condition, infos.getOutVariable(), infos.getOptions()))),
      _type(!infos.getProduceResult()
                ? Type::NoResult
                : _cursor->hasCovering() &&
                          !infos.getCoveringIndexAttributePositions().empty()
                      ? Type::Covering
                      : Type::Document),
      _callback() {
  if (checkUniqueness) {
    if (_type == Type::NoResult) {
      _callback.noProduce = getNullCallback<true>(context);
    } else {
      _callback.produce = buildCallback<true>(context);
    }
  } else {
    if (_type == Type::NoResult) {
      _callback.noProduce = getNullCallback<false>(context);
    } else {
      _callback.produce = buildCallback<false>(context);
    }
  }
}

IndexExecutor::CursorReader::CursorReader(CursorReader&& other) noexcept
    : _infos(other._infos),
      _condition(other._condition),
      _index(other._index),
      _cursor(std::move(other._cursor)),
      _type(other._type),
      _callback() {
  if (other._type == Type::NoResult) {
    _callback.noProduce = other._callback.noProduce;
  } else {
    _callback.produce = other._callback.produce;
  }
}

bool IndexExecutor::CursorReader::hasMore() const {
  if (_cursor != nullptr && _cursor->hasMore()) {
    return true;
  }
  return false;
}

bool IndexExecutor::CursorReader::readIndex(OutputAqlItemRow& output) {
  // this is called every time we want to read the index.
  // For the primary key index, this only reads the index once, and never
  // again (although there might be multiple calls to this function).
  // For the edge, hash or skiplists indexes, initIndexes creates an iterator
  // and read*Index just reads from the iterator until it is done.
  // Then initIndexes is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents.

  TRI_ASSERT(_cursor != nullptr);
  TRI_ASSERT(_cursor->hasMore());
  TRI_IF_FAILURE("IndexBlock::readIndex") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  switch (_type) {
    case Type::NoResult:
      TRI_ASSERT(_callback.noProduce != nullptr);
      return _cursor->next(_callback.noProduce, output.numRowsLeft());
    case Type::Covering:
      TRI_ASSERT(_callback.produce != nullptr);
      return _cursor->nextCovering(_callback.produce, output.numRowsLeft());
    case Type::Document:
      TRI_ASSERT(_callback.produce != nullptr);
      return _cursor->nextDocument(_callback.produce, output.numRowsLeft());
  }
  // The switch above is covering all values and this code
  // cannot be reached
  TRI_ASSERT(false);
  return false;
}

void IndexExecutor::CursorReader::reset() {
  if (_condition == nullptr || !_infos.hasNonConstParts()) {
    // Const case
    _cursor->reset();
    return;
  }
  IndexIterator* iterator = _cursor->indexIterator();
  if (iterator != nullptr && iterator->canRearm()) {
    bool didRearm =
        iterator->rearm(_condition, _infos.getOutVariable(), _infos.getOptions());
    if (!didRearm) {
      // iterator does not support the condition
      // It will not create any results
      _cursor->rearm(std::make_unique<EmptyIndexIterator>(iterator->collection(),
                                                          _infos.getTrxPtr()));
    }
    _cursor->reset();
    return;
  }
  // We need to build a fresh search and cannot go the rearm shortcut
  _cursor->rearm(_infos.getTrxPtr()->indexScanForCondition(_index, _condition,
                                                           _infos.getOutVariable(),
                                                           _infos.getOptions()));
}

IndexExecutor::IndexExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _documentProducingFunctionContext(::createContext(_input, _infos)),
      _state(ExecutionState::HASMORE),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _currentIndex(_infos.getIndexes().size()),
      _skipped(0) {
  TRI_ASSERT(!_infos.getIndexes().empty());
  // Creation of a cursor will trigger search.
  // As we want to create them lazily we only
  // reserve here.
  _cursors.reserve(_infos.getIndexes().size());
};

void IndexExecutor::initializeCursor() {
  _state = ExecutionState::HASMORE;
  _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
  _documentProducingFunctionContext.reset();
  _currentIndex = _infos.getIndexes().size();
  // should not be in a half-skipped state
  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
}

size_t IndexExecutor::CursorReader::skipIndex(size_t toSkip) {
  if (!hasMore()) {
    return 0;
  }

  uint64_t skipped = 0;
  _cursor->skip(toSkip, skipped);

  TRI_ASSERT(skipped <= toSkip);
  TRI_ASSERT(skipped == toSkip || !hasMore());

  return static_cast<size_t>(skipped);
}

IndexExecutor::~IndexExecutor() = default;

void IndexExecutor::initIndexes(InputAqlItemRow& input) {
  // We start with a different context. Return documents found in the previous
  // context again.
  _documentProducingFunctionContext.reset();
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

  _currentIndex = _infos.getIndexes().size();
}

void IndexExecutor::executeExpressions(InputAqlItemRow& input) {
  TRI_ASSERT(_infos.getCondition() != nullptr);
  TRI_ASSERT(!_infos.getNonConstExpressions().empty());

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  auto ast = _infos.getAst();
  auto* condition = const_cast<AstNode*>(_infos.getCondition());
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

bool IndexExecutor::advanceCursor() {
  // This function will lazily create cursors.
  // It will also settle the asc / desc ordering
  // once all cursors are created.
  // Furthermore it will adjust lastIndex and covering in the context
  const size_t numTotal = _infos.getIndexes().size();
  if (_currentIndex >= numTotal) {
    // We are past the last index, start from the beginning
    _currentIndex = 0;
  } else {
    _currentIndex++;
  }
  while (_currentIndex < numTotal) {
    TRI_ASSERT(_currentIndex <= _cursors.size());
    if (_currentIndex == _cursors.size() && _currentIndex < numTotal) {
      // First access to this index. Let's create it.
      size_t infoIndex =
          _infos.isAscending() ? _currentIndex : numTotal - _currentIndex - 1;
      AstNode const* conditionNode = nullptr;
      if (_infos.getCondition() != nullptr) {
        TRI_ASSERT(_infos.getIndexes().size() == _infos.getCondition()->numMembers());
        TRI_ASSERT(_infos.getCondition()->numMembers() > infoIndex);
        conditionNode = _infos.getCondition()->getMember(infoIndex);
      }
      _cursors.emplace_back(
          CursorReader{_infos, conditionNode, _infos.getIndexes()[infoIndex],
                       _documentProducingFunctionContext, needsUniquenessCheck()});
    } else {
      // Next index exists, need a reset.
      getCursor().reset();
    }
    // We have a cursor now.
    TRI_ASSERT(_currentIndex < _cursors.size());
    // Check if this cursor has more (some might now already)
    if (getCursor().hasMore()) {
      // The current cursor has data.
      _documentProducingFunctionContext.setAllowCoveringIndexOptimization(
          getCursor().isCovering());
      if (!_infos.hasMultipleExpansions()) {
        // If we have multiple expansions
        // We unfortunately need to insert found documents
        // in every index
        _documentProducingFunctionContext.setIsLastIndex(_currentIndex == numTotal - 1);
      }
      TRI_ASSERT(getCursor().hasMore());
      return true;
    }
    _currentIndex++;
  }

  // If we get here we were unable to
  TRI_ASSERT(_currentIndex >= numTotal);
  return false;
}

std::pair<ExecutionState, IndexStats> IndexExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("IndexExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  IndexStats stats{};

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  _documentProducingFunctionContext.setOutputRow(&output);

  while (true) {
    if (!_input) {
      if (_state == ExecutionState::DONE) {
        return {_state, stats};
      }

      std::tie(_state, _input) = _fetcher.fetchRow();

      if (!_input) {
        TRI_ASSERT(_state == ExecutionState::WAITING || _state == ExecutionState::DONE);
        return {_state, stats};
      }
      TRI_ASSERT(_state != ExecutionState::WAITING);
      TRI_ASSERT(_input);
      initIndexes(_input);
      if (!advanceCursor()) {
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        // just to validate that after continue we get into retry mode
        TRI_ASSERT(!_input);
        continue;
      }
    }
    TRI_ASSERT(_input.isInitialized());

    // Short Loop over the output block here for performance!
    while (!output.isFull()) {
      if (!getCursor().hasMore()) {
        if (!advanceCursor()) {
          // End of this cursor. Either return or try outer loop again.
          _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
          break;
        }
      }
      auto& cursor = getCursor();
      TRI_ASSERT(cursor.hasMore());

      // Read the next elements from the index
      bool more = cursor.readIndex(output);
      TRI_ASSERT(more == cursor.hasMore());
      // NOTE: more => output.isFull() does not hold, if we do uniqness checks.
      // The index iterator does still count skipped rows for limit.
      // Nevertheless loop here, the cursor has more so we will retigger
      // read more.
      // Loop here, either we have filled the output
      // Or the cursor is done, so we need to advance
    }
    stats.incrScanned(_documentProducingFunctionContext.getAndResetNumScanned());
    if (output.isFull()) {
      if (_state == ExecutionState::DONE && !_input) {
        return {ExecutionState::DONE, stats};
      }
      return {ExecutionState::HASMORE, stats};
    }
  }
}

std::tuple<ExecutionState, IndexExecutor::Stats, size_t> IndexExecutor::skipRows(size_t toSkip) {
  // This code does not work correctly with multiple indexes, as it does not
  // check for duplicates. Currently, no plan is generated where that can
  // happen, because with multiple indexes, the FILTER is not removed and thus
  // skipSome is not called on the IndexExecutor.
  TRI_ASSERT(_infos.getIndexes().size() <= 1);
  TRI_IF_FAILURE("IndexExecutor::skipRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  IndexStats stats{};

  while (_skipped < toSkip) {
    // get an input row first, if necessary
    if (!_input) {
      if (_state == ExecutionState::DONE) {
        size_t skipped = _skipped;

        _skipped = 0;

        return std::make_tuple(_state, stats, skipped); // tupple, cannot use initializer list due to build failure
      }

      std::tie(_state, _input) = _fetcher.fetchRow();

      if (_state == ExecutionState::WAITING) {
        return std::make_tuple(_state, stats, 0); // tupple, cannot use initializer list due to build failure
      }

      if (!_input) {
        TRI_ASSERT(_state == ExecutionState::DONE);
        size_t skipped = _skipped;

        _skipped = 0;

        return std::make_tuple(_state, stats, skipped); // tupple, cannot use initializer list due to build failure
      }

      initIndexes(_input);

      if (!advanceCursor()) {
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        // just to validate that after continue we get into retry mode
        TRI_ASSERT(!_input);
        continue;
      }
    }

    if (!getCursor().hasMore()) {
      if (!advanceCursor()) {
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        break;
      }
    }

    size_t skippedNow = getCursor().skipIndex(toSkip - _skipped);
    stats.incrScanned(skippedNow);
    _skipped += skippedNow;
  }

  size_t skipped = _skipped;

  _skipped = 0;

  if (_state == ExecutionState::DONE && !_input) {
    return std::make_tuple(ExecutionState::DONE, stats, skipped); // tupple, cannot use initializer list due to build failure
  }

  return std::make_tuple(ExecutionState::HASMORE, stats, skipped); // tupple, cannot use initializer list due to build failure
}
