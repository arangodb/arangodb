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
#include "Aql/AqlValueMaterializer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "ExecutorExpressionContext.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "V8/v8-globals.h"

#include <velocypack/Iterator.h>

#include <memory>
#include <utility>

// Set this to true to activate devel logging
#define INTERNAL_LOG_IDX LOG_DEVEL_IF(false)

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// resolve constant attribute accesses
static void resolveFCallConstAttributes(Ast* ast, AstNode* fcall) {
  TRI_ASSERT(fcall->type == NODE_TYPE_FCALL);
  TRI_ASSERT(fcall->numMembers() == 1);
  AstNode* array = fcall->getMemberUnchecked(0);
  for (size_t x = 0; x < array->numMembers(); x++) {
    AstNode* child = array->getMemberUnchecked(x);
    if (child->type == NODE_TYPE_ATTRIBUTE_ACCESS && child->isConstant()) {
      child = const_cast<AstNode*>(ast->resolveConstAttributeAccess(child));
      array->changeMember(x, child);
    }
  }
}

template <bool checkUniqueness>
IndexIterator::DocumentCallback getCallback(DocumentProducingFunctionContext& context,
                                            transaction::Methods::IndexHandle const& index,
                                            IndexNode::IndexValuesVars const& outNonMaterializedIndVars,
                                            IndexNode::IndexValuesRegisters const& outNonMaterializedIndRegs) {
  return [&context, &index, &outNonMaterializedIndVars, &outNonMaterializedIndRegs](LocalDocumentId const& token,
                                                                                    VPackSlice slice) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    auto indexId = index->id();
    TRI_ASSERT(indexId == outNonMaterializedIndRegs.first &&
               indexId == outNonMaterializedIndVars.first);
    if (ADB_UNLIKELY(indexId != outNonMaterializedIndRegs.first ||
                     indexId != outNonMaterializedIndVars.first)) {
      return false;
    }

    if (context.hasFilter()) {
      struct filterContext {
        IndexNode::IndexValuesVars const& outNonMaterializedIndVars;
        velocypack::Slice slice;
      };
      filterContext fc{outNonMaterializedIndVars, slice};

      auto getValue = [](void const* ctx, Variable const* var, bool doCopy) {
        TRI_ASSERT(ctx && var);
        auto const& fc = *reinterpret_cast<filterContext const*>(ctx);
        auto const it = fc.outNonMaterializedIndVars.second.find(var);
        TRI_ASSERT(fc.outNonMaterializedIndVars.second.cend() != it);
        if (ADB_UNLIKELY(fc.outNonMaterializedIndVars.second.cend() == it)) {
          return AqlValue();
        }
        velocypack::Slice s;
        // hash/skiplist/edge
        if (fc.slice.isArray()) {
          TRI_ASSERT(it->second < fc.slice.length());
          if (ADB_UNLIKELY(it->second >= fc.slice.length())) {
            return AqlValue();
          }
          s = fc.slice.at(it->second);
        } else {  // primary
          s = fc.slice;
        }
        if (doCopy) {
          return AqlValue(AqlValueHintCopy(s.start()));
        }
        return AqlValue(AqlValueHintDocumentNoCopy(s.start()));
      };

      if (!context.checkFilter(getValue, &fc)) {
        context.incrFiltered();
        return false;
      }
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    // move a document id
    AqlValue v(AqlValueHintUInt(token.id()));
    AqlValueGuard guard{v, true};
    TRI_ASSERT(!output.isFull());
    output.moveValueInto(registerId, input, guard);

    // hash/skiplist/edge
    if (slice.isArray()) {
      for (auto const& indReg : outNonMaterializedIndRegs.second) {
        TRI_ASSERT(indReg.first < slice.length());
        if (ADB_UNLIKELY(indReg.first >= slice.length())) {
          return false;
        }
        auto s = slice.at(indReg.first);
        AqlValue v(s);
        AqlValueGuard guard{v, true};
        TRI_ASSERT(!output.isFull());
        output.moveValueInto(indReg.second, input, guard);
      }
    } else {  // primary
      auto indReg = outNonMaterializedIndRegs.second.cbegin();
      TRI_ASSERT(indReg != outNonMaterializedIndRegs.second.cend());
      if (ADB_UNLIKELY(indReg == outNonMaterializedIndRegs.second.cend())) {
        return false;
      }
      AqlValue v(slice);
      AqlValueGuard guard{v, true};
      TRI_ASSERT(!output.isFull());
      output.moveValueInto(indReg->second, input, guard);
    }

    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
}
}  // namespace

IndexExecutorInfos::IndexExecutorInfos(
    RegisterId outputRegister, QueryContext& query,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    Expression* filter, std::vector<std::string> const& projections,
    std::vector<size_t> const& coveringIndexAttributePositions, 
    std::vector<std::unique_ptr<NonConstExpression>>&& nonConstExpression,
    std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs,
    bool hasV8Expression, bool count, AstNode const* condition,
    std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast,
    IndexIteratorOptions options, IndexNode::IndexValuesVars const& outNonMaterializedIndVars,
    IndexNode::IndexValuesRegisters&& outNonMaterializedIndRegs)
    : _indexes(std::move(indexes)),
      _condition(condition),
      _ast(ast),
      _options(options),
      _query(query),
      _collection(collection),
      _outVariable(outVariable),
      _filter(filter),
      _projections(projections),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _expInVars(std::move(expInVars)),
      _expInRegs(std::move(expInRegs)),
      _nonConstExpression(std::move(nonConstExpression)),
      _outputRegisterId(outputRegister),
      _outNonMaterializedIndVars(outNonMaterializedIndVars),
      _outNonMaterializedIndRegs(std::move(outNonMaterializedIndRegs)),
      _hasMultipleExpansions(false),
      _produceResult(produceResult),
      _hasV8Expression(hasV8Expression),
      _count(count) {
  if (_condition != nullptr) {
    // fix const attribute accesses, e.g. { "a": 1 }.a
    for (size_t i = 0; i < _condition->numMembers(); ++i) {
      auto andCond = _condition->getMemberUnchecked(i);
      for (size_t j = 0; j < andCond->numMembers(); ++j) {
        auto leaf = andCond->getMemberUnchecked(j);

        // geo index condition i.e. GEO_CONTAINS, GEO_INTERSECTS
        if (leaf->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(_ast, leaf);
          continue;  //
        } else if (leaf->numMembers() != 2) {
          continue;  // Otherwise we only support binary conditions
        }

        TRI_ASSERT(leaf->numMembers() == 2);
        AstNode* lhs = leaf->getMemberUnchecked(0);
        AstNode* rhs = leaf->getMemberUnchecked(1);
        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->isConstant()) {
          lhs = const_cast<AstNode*>(_ast->resolveConstAttributeAccess(lhs));
          leaf->changeMember(0, lhs);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->isConstant()) {
          rhs = const_cast<AstNode*>(_ast->resolveConstAttributeAccess(rhs));
          leaf->changeMember(1, rhs);
        }
        // geo index condition i.e. `GEO_DISTANCE(x, y) <= d`
        if (lhs->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(_ast, lhs);
        }
      }
    }
  }

  // count how many attributes in the index are expanded (array index)
  // if more than a single attribute, we always need to deduplicate the
  // result later on
  for (auto const& idx : getIndexes()) {
    size_t expansions = 0;
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

Collection const* IndexExecutorInfos::getCollection() const {
  return _collection;
}

Variable const* IndexExecutorInfos::getOutVariable() const {
  return _outVariable;
}

std::vector<std::string> const& IndexExecutorInfos::getProjections() const noexcept {
  return _projections;
}

QueryContext& IndexExecutorInfos::query() noexcept {
  return _query;
}

Expression* IndexExecutorInfos::getFilter() const noexcept { return _filter; }

std::vector<size_t> const& IndexExecutorInfos::getCoveringIndexAttributePositions() const noexcept {
  return _coveringIndexAttributePositions;
}

bool IndexExecutorInfos::getProduceResult() const noexcept {
  return _produceResult;
}

std::vector<transaction::Methods::IndexHandle> const& IndexExecutorInfos::getIndexes() const
    noexcept {
  return _indexes;
}

AstNode const* IndexExecutorInfos::getCondition() const noexcept {
  return _condition;
}

bool IndexExecutorInfos::getV8Expression() const noexcept {
  return _hasV8Expression;
}

RegisterId IndexExecutorInfos::getOutputRegisterId() const noexcept {
  return _outputRegisterId;
}

std::vector<std::unique_ptr<NonConstExpression>> const& IndexExecutorInfos::getNonConstExpressions() const
    noexcept {
  return _nonConstExpression;
}

bool IndexExecutorInfos::hasMultipleExpansions() const noexcept {
  return _hasMultipleExpansions;
}

bool IndexExecutorInfos::getCount() const noexcept { return _count; }

IndexIteratorOptions IndexExecutorInfos::getOptions() const { return _options; }

bool IndexExecutorInfos::isAscending() const noexcept {
  return _options.ascending;
}

Ast* IndexExecutorInfos::getAst() const noexcept { return _ast; }

std::vector<Variable const*> const& IndexExecutorInfos::getExpInVars() const noexcept {
  return _expInVars;
}

std::vector<RegisterId> const& IndexExecutorInfos::getExpInRegs() const noexcept {
  return _expInRegs;
}

void IndexExecutorInfos::setHasMultipleExpansions(bool flag) {
  _hasMultipleExpansions = flag;
}

bool IndexExecutorInfos::hasNonConstParts() const {
  return !_nonConstExpression.empty();
}

IndexExecutor::CursorReader::CursorReader(transaction::Methods& trx,
                                          IndexExecutorInfos const& infos,
                                          AstNode const* condition,
                                          transaction::Methods::IndexHandle const& index,
                                          DocumentProducingFunctionContext& context,
                                          bool checkUniqueness)
    : _trx(trx),
      _infos(infos),
      _condition(condition),
      _index(index),
      _cursor(_trx.indexScanForCondition(
          index, condition, infos.getOutVariable(), infos.getOptions())),
      _context(context),
      _type(infos.getCount() ? Type::Count :
                infos.isLateMaterialized()
                    ? Type::LateMaterialized
                    : !infos.getProduceResult()
                          ? Type::NoResult
                          : _cursor->hasCovering() &&  // if change see IndexNode::canApplyLateDocumentMaterializationRule()
                                    !infos.getCoveringIndexAttributePositions().empty()
                                ? Type::Covering
                                : Type::Document) {
  switch (_type) {
    case Type::NoResult: {
      _documentNonProducer = checkUniqueness ? getNullCallback<true>(context)
                                             : getNullCallback<false>(context);
      break;
    }
    case Type::LateMaterialized:
      _documentProducer =
          checkUniqueness
              ? ::getCallback<true>(context, _index, _infos.getOutNonMaterializedIndVars(), _infos.getOutNonMaterializedIndRegs())
              : ::getCallback<false>(context, _index, _infos.getOutNonMaterializedIndVars(), _infos.getOutNonMaterializedIndRegs());
      break;
    case Type::Count:
      break;
    default:
      _documentProducer = checkUniqueness
                              ? buildDocumentCallback<true, false>(context)
                              : buildDocumentCallback<false, false>(context);
      break;
  }
  _documentSkipper = checkUniqueness ? buildDocumentCallback<true, true>(context)
                                     : buildDocumentCallback<false, true>(context);
}

IndexExecutor::CursorReader::CursorReader(CursorReader&& other) noexcept
    : _trx(other._trx),
      _infos(other._infos),
      _condition(other._condition),
      _index(other._index),
      _cursor(std::move(other._cursor)),
      _context(other._context),
      _type(other._type),
      _documentNonProducer(std::move(other._documentNonProducer)),
      _documentProducer(std::move(other._documentProducer)),
      _documentSkipper(std::move(other._documentSkipper)) {}

bool IndexExecutor::CursorReader::hasMore() const {
  return _cursor->hasMore();
}

bool IndexExecutor::CursorReader::readIndex(OutputAqlItemRow& output) {
  // this is called every time we want to read the index.
  // For the primary key index, this only reads the index once, and never
  // again (although there might be multiple calls to this function).
  // For the edge, hash or skiplists indexes, initIndexes creates an iterator
  // and read*Index just reads from the iterator until it is done.
  // Then initIndexes is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents.
  TRI_ASSERT(_cursor->hasMore());
  TRI_IF_FAILURE("IndexBlock::readIndex") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  switch (_type) {
    case Type::NoResult:
      TRI_ASSERT(_documentNonProducer != nullptr);
      return _cursor->next(_documentNonProducer, output.numRowsLeft());
    case Type::Covering:
    case Type::LateMaterialized:
      TRI_ASSERT(_documentProducer != nullptr);
      return _cursor->nextCovering(_documentProducer, output.numRowsLeft());
    case Type::Document:
      TRI_ASSERT(_documentProducer != nullptr);
      return _cursor->nextDocument(_documentProducer, output.numRowsLeft());
    case Type::Count: {
      uint64_t counter = 0;
      _cursor->skipAll(counter);
      InputAqlItemRow const& input = _context.getInputRow();
      RegisterId registerId = _context.getOutputRegister();
      TRI_ASSERT(!output.isFull());
      AqlValue v((AqlValueHintUInt(counter)));
      AqlValueGuard guard{v, true};
      output.moveValueInto(registerId, input, guard);
      TRI_ASSERT(output.produced());
      output.advanceRow();
      return false;
    }
  }
  // The switch above is covering all values and this code
  // cannot be reached
  ADB_UNREACHABLE;
  return false;
}

size_t IndexExecutor::CursorReader::skipIndex(size_t toSkip) {
  TRI_ASSERT(_type != Type::Count);
  
  if (!hasMore()) {
    return 0;
  }

  uint64_t skipped = 0;
  if (_infos.getFilter() != nullptr) {
    switch (_type) {
      case Type::Covering:
      case Type::LateMaterialized:
        TRI_ASSERT(_documentSkipper != nullptr);
        _cursor->nextCovering(_documentSkipper, toSkip);
        break;
      case Type::NoResult:
      case Type::Document:
      case Type::Count:
        TRI_ASSERT(_documentSkipper != nullptr);
        _cursor->nextDocument(_documentSkipper, toSkip);
        break;
    }
    skipped = _context.getAndResetNumScanned() - _context.getAndResetNumFiltered();
  } else {
    _cursor->skip(toSkip, skipped);
  }

  TRI_ASSERT(skipped <= toSkip);
  TRI_ASSERT(skipped == toSkip || !hasMore());

  return static_cast<size_t>(skipped);
}

bool IndexExecutor::CursorReader::isCovering() const {
  return _type == Type::Covering;
}

void IndexExecutor::CursorReader::reset() {
  if (_condition == nullptr || !_infos.hasNonConstParts()) {
    // Const case
    _cursor->reset();
    return;
  }

  if (_cursor->canRearm()) {
    bool didRearm =
        _cursor->rearm(_condition, _infos.getOutVariable(), _infos.getOptions());
    if (!didRearm) {
      // iterator does not support the condition
      // It will not create any results
      _cursor = std::make_unique<EmptyIndexIterator>(_cursor->collection(), &_trx);
    }
  } else {
    // We need to build a fresh search and cannot go the rearm shortcut
    _cursor = _trx.indexScanForCondition(_index, _condition,
                                         _infos.getOutVariable(),
                                         _infos.getOptions());
  }
}

IndexExecutor::IndexExecutor(Fetcher& fetcher, Infos& infos)
    : _trx(infos.query().newTrxContext()),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _state(ExecutorState::HASMORE),
      _documentProducingFunctionContext(
      _input, nullptr, infos.getOutputRegisterId(), infos.getProduceResult(),
      infos.query(), _trx, infos.getFilter(), infos.getProjections(),
      infos.getCoveringIndexAttributePositions(), false,
      infos.getIndexes().size() > 1 || infos.hasMultipleExpansions()),
      _infos(infos),
      _currentIndex(_infos.getIndexes().size()),
      _skipped(0) {
  TRI_ASSERT(!_infos.getIndexes().empty());
  // Creation of a cursor will trigger search.
  // As we want to create them lazily we only
  // reserve here.
  _cursors.reserve(_infos.getIndexes().size());
}

IndexExecutor::~IndexExecutor() = default;

void IndexExecutor::initializeCursor() {
  _state = ExecutorState::HASMORE;
  _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
  _documentProducingFunctionContext.reset();
  _currentIndex = _infos.getIndexes().size();
  // should not be in a half-skipped state
  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
}

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
          _infos.query().exitV8Context();
        }
      };

      _infos.query().enterV8Context();
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

  auto& query = _infos.query();

  for (size_t posInExpressions = 0;
       posInExpressions < _infos.getNonConstExpressions().size(); ++posInExpressions) {
    NonConstExpression* toReplace =
        _infos.getNonConstExpressions()[posInExpressions].get();
    auto exp = toReplace->expression.get();

    auto& regex = _documentProducingFunctionContext.aqlFunctionsInternalCache();

    ExecutorExpressionContext ctx(_trx, query, regex,
                                  input, _infos.getExpInVars(),
                                  _infos.getExpInRegs());

    bool mustDestroy;
    AqlValue a = exp->execute(&ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(&_trx);
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
      _cursors.emplace_back(_trx, _infos, conditionNode, _infos.getIndexes()[infoIndex],
                            _documentProducingFunctionContext, needsUniquenessCheck());
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

IndexExecutor::CursorReader& IndexExecutor::getCursor() {
  TRI_ASSERT(_currentIndex < _cursors.size());
  return _cursors[_currentIndex];
}

bool IndexExecutor::needsUniquenessCheck() const noexcept {
  return _infos.getIndexes().size() > 1 || _infos.hasMultipleExpansions();
}

[[nodiscard]] auto IndexExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (_infos.getCount()) {
    // when we are counting, we will always return a single row
    return std::max<size_t>(input.countShadowRows(), 1);
  }
  // Otherwise we do not know.
  return call.getLimit();
}

auto IndexExecutor::produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("IndexExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  IndexStats stats{};

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumFiltered() == 0);
  _documentProducingFunctionContext.setOutputRow(&output);

  AqlCall clientCall = output.getClientCall();
  INTERNAL_LOG_IDX << "IndexExecutor::produceRows " << clientCall;

  /*
   * Logic of this executor is as follows:
   *  - peek a data row
   *  - read the indexes for this data row until its done
   *  - continue
   */

  while (!output.isFull()) {
    INTERNAL_LOG_IDX << "IndexExecutor::produceRows output.numRowsLeft() == "
                  << output.numRowsLeft();
    if (!_input.isInitialized()) {
      std::tie(_state, _input) = inputRange.peekDataRow();
      INTERNAL_LOG_IDX
          << "IndexExecutor::produceRows input not initialized, peek next row: " << _state
          << " " << std::boolalpha << _input.isInitialized();

      if (_input.isInitialized()) {
        INTERNAL_LOG_IDX << "IndexExecutor::produceRows initIndexes";
        initIndexes(_input);
        if (!advanceCursor()) {
          INTERNAL_LOG_IDX << "IndexExecutor::produceRows failed to advanceCursor "
                           "after init";
          std::ignore = inputRange.nextDataRow();
          _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
          // just to validate that after continue we get into retry mode
          TRI_ASSERT(!_input);
          continue;
        }
      } else {
        break;
      }
    }

    TRI_ASSERT(_input.isInitialized());
    // Short Loop over the output block here for performance!
    while (!output.isFull()) {
      INTERNAL_LOG_IDX << "IndexExecutor::produceRows::innerLoop hasMore = " << std::boolalpha
                    << getCursor().hasMore() << " " << output.numRowsLeft();

      if (!getCursor().hasMore() && !advanceCursor()) {
        INTERNAL_LOG_IDX << "IndexExecutor::produceRows::innerLoop cursor does "
                         "not have more and advancing failed";
        std::ignore = inputRange.nextDataRow();
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        break;
      }

      TRI_ASSERT(getCursor().hasMore());

      // Read the next elements from the index
      bool more = getCursor().readIndex(output);
      TRI_ASSERT(more == getCursor().hasMore());

      INTERNAL_LOG_IDX
          << "IndexExecutor::produceRows::innerLoop output.numRowsWritten() == "
          << output.numRowsWritten();
      // NOTE: more => output.isFull() does not hold, if we do uniqness checks.
      // The index iterator does still count skipped rows for limit.
      // Nevertheless loop here, the cursor has more so we will retigger
      // read more.
      // Loop here, either we have filled the output
      // Or the cursor is done, so we need to advance
    }

    stats.incrScanned(_documentProducingFunctionContext.getAndResetNumScanned());
    stats.incrFiltered(_documentProducingFunctionContext.getAndResetNumFiltered());
  }

  AqlCall upstreamCall;

  INTERNAL_LOG_IDX << "IndexExecutor::produceRows reporting state " << returnState();
  return {returnState(), stats, upstreamCall};
}

auto IndexExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  // This code does not work correctly with multiple indexes, as it does not
  // check for duplicates. Currently, no plan is generated where that can
  // happen, because with multiple indexes, the FILTER is not removed and thus
  // skipSome is not called on the IndexExecutor.
  TRI_ASSERT(_infos.getIndexes().size() <= 1);
  TRI_IF_FAILURE("IndexExecutor::skipRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange " << clientCall;

  IndexStats stats{};

  while (clientCall.needSkipMore()) {
    INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange skipped " << _skipped << " "
                  << clientCall.getOffset();
    // get an input row first, if necessary
    if (!_input.isInitialized()) {
      std::tie(_state, _input) = inputRange.peekDataRow();
      INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange input not initialized, "
                       "peek next row: "
                    << _state << " " << std::boolalpha << _input.isInitialized();

      if (_input.isInitialized()) {
        INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange initIndexes";
        initIndexes(_input);
        if (!advanceCursor()) {
          INTERNAL_LOG_IDX
              << "IndexExecutor::skipRowsRange failed to advanceCursor "
                 "after init";
          std::ignore = inputRange.nextDataRow();
          _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
          // just to validate that after continue we get into retry mode
          TRI_ASSERT(!_input);
          continue;
        }
      } else {
        break;
      }
    }

    if (!getCursor().hasMore() && !advanceCursor()) {
      INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange cursor does not "
                       "have more and advancing failed";
      std::ignore = inputRange.nextDataRow();
      _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
      continue;
    }

    auto toSkip = clientCall.getOffset();
    if (toSkip == 0) {
      TRI_ASSERT(clientCall.needsFullCount());
      toSkip = ExecutionBlock::SkipAllSize();
    }
    TRI_ASSERT(toSkip > 0);
    INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange skipIndex(" << toSkip << ")";
    size_t skippedNow = getCursor().skipIndex(toSkip);
    INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange skipIndex(...) == " << skippedNow;

    stats.incrScanned(skippedNow);
    _skipped += skippedNow;
    clientCall.didSkip(skippedNow);
  }

  size_t skipped = _skipped;
  _skipped = 0;

  AqlCall upstreamCall;

  INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange returning " << returnState()
                << " " << skipped << " " << upstreamCall;
  return {returnState(), stats, skipped, upstreamCall};
}

auto IndexExecutor::returnState() const noexcept -> ExecutorState {
  if (_input.isInitialized()) {
    // We are still working.
    // TODO: Potential optimization: We can ask if the cursor has more, or there
    // are other cursors.
    return ExecutorState::HASMORE;
  }
  return _state;
}
