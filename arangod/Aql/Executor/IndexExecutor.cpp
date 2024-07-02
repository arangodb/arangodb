////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/Expression.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/NonConstExpression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Projections.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Logger/LogMacros.h"
#include "Transaction/Helpers.h"
#ifdef USE_V8
#include "V8/v8-globals.h"
#endif

#include <velocypack/Iterator.h>

#include <memory>
#include <span>
#include <utility>

// Set this to true to activate devel logging
#define INTERNAL_LOG_IDX LOG_DEVEL_IF(false)

using namespace arangodb;
using namespace arangodb::aql;

namespace {

bool hasMultipleExpansions(
    std::span<transaction::Methods::IndexHandle const> indexes) noexcept {
  // count how many attributes in the index are expanded (array index).
  // if more than a single attribute is expanded, we always need to
  // deduplicate the result later on.
  // if we have an expanded attribute that occurs later in the index fields
  // definition, e.g. ["name", "status[*]"], we also need to deduplicate
  // later on.
  for (auto const& idx : indexes) {
    auto const& fields = idx->fields();
    for (size_t i = 1; i < fields.size(); ++i) {
      if (idx->isAttributeExpanded(i)) {
        return true;
      }
    }
  }

  return false;
}

/// resolve constant attribute accesses
void resolveFCallConstAttributes(Ast* ast, AstNode* fcall) {
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

template<bool checkUniqueness>
IndexIterator::CoveringCallback getCallback(
    DocumentProducingCallbackVariant::WithLateMaterialization,
    DocumentProducingFunctionContext& context,
    transaction::Methods::IndexHandle const& index) {
  auto impl = [&context]<typename TokenType>(
                  TokenType&& token, IndexIteratorCoveringData& covering) {
    constexpr bool isLocalDocumentId =
        std::is_same_v<LocalDocumentId, std::decay_t<TokenType>>;
    // can't be a static_assert as this implementation is still possible
    // just can't be used right now as for restriction in late materialization
    // rule
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if constexpr (checkUniqueness && !isLocalDocumentId) {
      TRI_ASSERT(false);
    }
#endif
    if constexpr (checkUniqueness && isLocalDocumentId) {
      TRI_ASSERT(token.isSet());
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    if (context.hasFilter() && !context.checkFilter(&covering)) {
      context.incrFiltered();
      return false;
    }
    context.incrLookups();

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    TRI_ASSERT(!output.isFull());
    // move a document id
    if constexpr (isLocalDocumentId) {
      TRI_ASSERT(token.isSet());
      AqlValue v(AqlValueHintUInt(token.id()));
      AqlValueGuard guard{v, false};
      output.moveValueInto(registerId, input, &guard);
    } else {
      AqlValueGuard guard{token, true};
      output.moveValueInto(registerId, input, &guard);
    }

    // write projections into individual output registers
    if (!context.getProjectionsForRegisters().empty()) {
      TRI_ASSERT(context.getProjectionsForRegisters().usesCoveringIndex());
      context.getProjectionsForRegisters().produceFromIndex(
          context.getBuilder(), covering, context.getTrxPtr(),
          [&](Variable const* variable, velocypack::Slice slice) {
            if (slice.isNone()) {
              slice = VPackSlice::nullSlice();
            }
            RegisterId registerId = context.registerForVariable(variable->id);
            TRI_ASSERT(registerId != RegisterId::maxRegisterId);
            output.moveValueInto(registerId, input, slice);
          });
    }

    TRI_ASSERT(output.produced());
    output.advanceRow();
    return true;
  };
  return impl;
}

}  // namespace

IndexExecutorInfos::IndexExecutorInfos(
    IndexNode::Strategy strategy, RegisterId outputRegister,
    QueryContext& query, Collection const* collection,
    Variable const* outVariable, Expression* filter,
    aql::Projections projections, aql::Projections filterProjections,
    std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs,
    NonConstExpressionContainer&& nonConstExpressions,
    ReadOwnWrites readOwnWrites, AstNode const* condition,
    bool oneIndexCondition,
    std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast,
    IndexIteratorOptions options,
    IndexNode::IndexFilterCoveringVars filterCoveringVars)
    : _strategy(strategy),
      _indexes(std::move(indexes)),
      _condition(condition),
      _ast(ast),
      _options(options),
      _query(query),
      _collection(collection),
      _outVariable(outVariable),
      _filter(filter),
      _projections(std::move(projections)),
      _filterProjections(std::move(filterProjections)),
      _filterVarsToRegs(std::move(filterVarsToRegs)),
      _filterCoveringVars(std::move(filterCoveringVars)),
      _nonConstExpressions(std::move(nonConstExpressions)),
      _outputRegisterId(outputRegister),
      _hasMultipleExpansions(::hasMultipleExpansions(_indexes)),
      _oneIndexCondition(oneIndexCondition),
      _readOwnWrites(readOwnWrites) {
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
}

IndexNode::Strategy IndexExecutorInfos::strategy() const noexcept {
  return _strategy;
}

Collection const* IndexExecutorInfos::getCollection() const {
  return _collection;
}

Variable const* IndexExecutorInfos::getOutVariable() const {
  return _outVariable;
}

arangodb::aql::Projections const& IndexExecutorInfos::getProjections()
    const noexcept {
  return _projections;
}

arangodb::aql::Projections const& IndexExecutorInfos::getFilterProjections()
    const noexcept {
  return _filterProjections;
}

ResourceMonitor& IndexExecutorInfos::getResourceMonitor() noexcept {
  return _query.resourceMonitor();
}

QueryContext& IndexExecutorInfos::query() noexcept { return _query; }

Expression* IndexExecutorInfos::getFilter() const noexcept { return _filter; }

std::vector<transaction::Methods::IndexHandle> const&
IndexExecutorInfos::getIndexes() const noexcept {
  return _indexes;
}

AstNode const* IndexExecutorInfos::getCondition() const noexcept {
  return _condition;
}

bool IndexExecutorInfos::getV8Expression() const noexcept {
  return _nonConstExpressions._hasV8Expression;
}

RegisterId IndexExecutorInfos::getOutputRegisterId() const noexcept {
  return _outputRegisterId;
}

std::vector<std::unique_ptr<NonConstExpression>> const&
IndexExecutorInfos::getNonConstExpressions() const noexcept {
  return _nonConstExpressions._expressions;
}

bool IndexExecutorInfos::hasMultipleExpansions() const noexcept {
  return _hasMultipleExpansions;
}

IndexIteratorOptions IndexExecutorInfos::getOptions() const { return _options; }

bool IndexExecutorInfos::isAscending() const noexcept {
  return _options.ascending;
}

Ast* IndexExecutorInfos::getAst() const noexcept { return _ast; }

std::vector<std::pair<VariableId, RegisterId>> const&
IndexExecutorInfos::getVarsToRegister() const noexcept {
  return _nonConstExpressions._varToRegisterMapping;
}

std::vector<std::pair<VariableId, RegisterId>> const&
IndexExecutorInfos::getFilterVarsToRegister() const noexcept {
  return _filterVarsToRegs;
}

bool IndexExecutorInfos::hasNonConstParts() const {
  return !_nonConstExpressions._expressions.empty();
}

void IndexExecutor::CursorStats::incrCursorsCreated(
    std::uint64_t value) noexcept {
  cursorsCreated += value;
}

void IndexExecutor::CursorStats::incrCursorsRearmed(
    std::uint64_t value) noexcept {
  cursorsRearmed += value;
}

void IndexExecutor::CursorStats::incrCacheHits(std::uint64_t value) noexcept {
  cacheHits += value;
}

void IndexExecutor::CursorStats::incrCacheMisses(std::uint64_t value) noexcept {
  cacheMisses += value;
}

std::uint64_t IndexExecutor::CursorStats::getAndResetCursorsCreated() noexcept {
  return std::exchange(cursorsCreated, 0);
}

std::uint64_t IndexExecutor::CursorStats::getAndResetCursorsRearmed() noexcept {
  return std::exchange(cursorsRearmed, 0);
}

std::uint64_t IndexExecutor::CursorStats::getAndResetCacheHits() noexcept {
  return std::exchange(cacheHits, 0);
}

std::uint64_t IndexExecutor::CursorStats::getAndResetCacheMisses() noexcept {
  return std::exchange(cacheMisses, 0);
}

IndexExecutor::CursorReader::CursorReader(
    transaction::Methods& trx, IndexExecutorInfos& infos,
    AstNode const* condition, transaction::Methods::IndexHandle const& index,
    DocumentProducingFunctionContext& context, CursorStats& cursorStats,
    bool checkUniqueness)
    : _trx(trx),
      _infos(infos),
      _condition(condition),
      _index(index),
      _cursor(_trx.indexScanForCondition(
          _infos.query().resourceMonitor(), index, condition,
          infos.getOutVariable(), infos.getOptions(), infos.canReadOwnWrites(),
          transaction::Methods::kNoMutableConditionIdx)),
      _context(context),
      _cursorStats(cursorStats),
      _strategy(infos.strategy()),
      _checkUniqueness(checkUniqueness) {
  TRI_ASSERT(
      (_strategy != IndexNode::Strategy::kCoveringFilterOnly &&
       _strategy != IndexNode::Strategy::kCoveringFilterScanOnly) ||
      (infos.getFilter() != nullptr && !infos.getFilterProjections().empty()));

  // for the initial cursor created in the initializer list
  _cursorStats.incrCursorsCreated();

  switch (_strategy) {
    case IndexNode::Strategy::kNoResult: {
      _documentNonProducer = checkUniqueness
                                 ? getNullCallback<true, false>(context)
                                 : getNullCallback<false, false>(context);
      break;
    }
    case IndexNode::Strategy::kCovering: {
      _coveringProducer =
          checkUniqueness
              ? ::getCallback<true, false>(DocumentProducingCallbackVariant::
                                               WithProjectionsCoveredByIndex{},
                                           context)
              : ::getCallback<false, false>(DocumentProducingCallbackVariant::
                                                WithProjectionsCoveredByIndex{},
                                            context);
      break;
    }
    case IndexNode::Strategy::kCoveringFilterScanOnly: {
      _coveringProducer =
          checkUniqueness
              ? ::getCallback<true, false, /*produceResult*/ false>(
                    DocumentProducingCallbackVariant::
                        WithFilterCoveredByIndex{},
                    context)
              : ::getCallback<false, false, /*produceResult*/ false>(
                    DocumentProducingCallbackVariant::
                        WithFilterCoveredByIndex{},
                    context);
      break;
    }
    case IndexNode::Strategy::kCoveringFilterOnly: {
      _coveringProducer =
          checkUniqueness ? ::getCallback<true, false, /*produceResult*/ true>(
                                DocumentProducingCallbackVariant::
                                    WithFilterCoveredByIndex{},
                                context)
                          : ::getCallback<false, false, /*produceResult*/ true>(
                                DocumentProducingCallbackVariant::
                                    WithFilterCoveredByIndex{},
                                context);
      break;
    }
    case IndexNode::Strategy::kLateMaterialized:
      _coveringProducer =
          checkUniqueness
              ? ::getCallback<true>(
                    DocumentProducingCallbackVariant::WithLateMaterialization{},
                    context, _index)
              : ::getCallback<false>(
                    DocumentProducingCallbackVariant::WithLateMaterialization{},
                    context, _index);
      break;
    case IndexNode::Strategy::kCount:
      break;
    default:
      _documentProducer = checkUniqueness
                              ? buildDocumentCallback<true, false>(context)
                              : buildDocumentCallback<false, false>(context);
      break;
  }

  if (_coveringProducer) {
    if (_strategy == IndexNode::Strategy::kCovering) {
      _coveringSkipper =
          checkUniqueness
              ? ::getCallback<true, true>(DocumentProducingCallbackVariant::
                                              WithProjectionsCoveredByIndex{},
                                          context)
              : ::getCallback<false, true>(DocumentProducingCallbackVariant::
                                               WithProjectionsCoveredByIndex{},
                                           context);
    } else if (_strategy == IndexNode::Strategy::kCoveringFilterScanOnly ||
               _strategy == IndexNode::Strategy::kCoveringFilterOnly) {
      _coveringSkipper =
          checkUniqueness ? ::getCallback<true, true, /*produceResult*/ false>(
                                DocumentProducingCallbackVariant::
                                    WithFilterCoveredByIndex{},
                                context)
                          : ::getCallback<false, true, /*produceResult*/ false>(
                                DocumentProducingCallbackVariant::
                                    WithFilterCoveredByIndex{},
                                context);
    } else if (_strategy == IndexNode::Strategy::kLateMaterialized) {
      if (_infos.getFilter() != nullptr) {
        _coveringSkipper =
            checkUniqueness
                ? ::getCallback<true, true, /*produceResult*/ false>(
                      DocumentProducingCallbackVariant::
                          WithFilterCoveredByIndex{},
                      context)
                : ::getCallback<false, true, /*produceResult*/ false>(
                      DocumentProducingCallbackVariant::
                          WithFilterCoveredByIndex{},
                      context);
      } else {
        _coveringSkipper =
            checkUniqueness
                ? ::getCallback<true, true>(DocumentProducingCallbackVariant::
                                                WithProjectionsCoveredByIndex{},
                                            context)
                : ::getCallback<false, true>(
                      DocumentProducingCallbackVariant::
                          WithProjectionsCoveredByIndex{},
                      context);
      }
    }
  } else {
    _documentSkipper = checkUniqueness
                           ? buildDocumentCallback<true, true>(context)
                           : buildDocumentCallback<false, true>(context);
  }
}

bool IndexExecutor::CursorReader::hasMore() const { return _cursor->hasMore(); }

bool IndexExecutor::CursorReader::readIndex(
    OutputAqlItemRow& output,
    DocumentProducingFunctionContext const& documentProducingFunctionContext) {
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

  // validate that output has a valid AqlItemBlock
  TRI_ASSERT(output.isInitialized());
  // validate that the output pointer in documentProducingFunctionContext is the
  // same as in output
  TRI_ASSERT(&output == &documentProducingFunctionContext.getOutputRow());

  // update cache statistics from cursor when we exit this method
  auto statsUpdater = scopeGuard([this]() noexcept {
    auto [ch, cm] = _cursor->getAndResetCacheStats();
    _cursorStats.incrCacheHits(ch);
    _cursorStats.incrCacheMisses(cm);
  });

  switch (_strategy) {
    case IndexNode::Strategy::kNoResult:
      TRI_ASSERT(_documentNonProducer != nullptr);
      return _cursor->next(_documentNonProducer, output.numRowsLeft());
    case IndexNode::Strategy::kCovering:
    case IndexNode::Strategy::kCoveringFilterScanOnly:
    case IndexNode::Strategy::kCoveringFilterOnly:
    case IndexNode::Strategy::kLateMaterialized:
      TRI_ASSERT(_coveringProducer != nullptr);
      return _cursor->nextCovering(_coveringProducer, output.numRowsLeft());
    case IndexNode::Strategy::kDocument:
      TRI_ASSERT(_documentProducer != nullptr);
      return _cursor->nextDocument(_documentProducer, output.numRowsLeft());
    case IndexNode::Strategy::kCount: {
      uint64_t counter = 0;
      if (_checkUniqueness) {
        _cursor->all([&](LocalDocumentId token) -> bool {
          if (_context.checkUniqueness(token)) {
            counter++;
          }
          return true;
        });
      } else {
        _cursor->skipAll(counter);
      }

      InputAqlItemRow const& input = _context.getInputRow();
      RegisterId registerId = _context.getOutputRegister();
      TRI_ASSERT(!output.isFull());
      AqlValue v((AqlValueHintUInt(counter)));
      AqlValueGuard guard{v, true};
      output.moveValueInto(registerId, input, &guard);
      TRI_ASSERT(output.produced());
      output.advanceRow();
      return false;
    }
  }
  // The switch above is covering all values and this code
  // cannot be reached
  ADB_UNREACHABLE;
}

size_t IndexExecutor::CursorReader::skipIndex(size_t toSkip) {
  TRI_ASSERT(_strategy != IndexNode::Strategy::kCount);

  if (!hasMore()) {
    return 0;
  }

  uint64_t skipped = 0;
  if (_infos.getFilter() != nullptr || _checkUniqueness) {
    while (hasMore() && (skipped < toSkip)) {
      switch (_strategy) {
        case IndexNode::Strategy::kLateMaterialized:
          _context.setAllowCoveringIndexOptimization(true);
          [[fallthrough]];

        case IndexNode::Strategy::kCovering:
        case IndexNode::Strategy::kCoveringFilterScanOnly:
        case IndexNode::Strategy::kCoveringFilterOnly:
          TRI_ASSERT(_coveringSkipper != nullptr);
          _cursor->nextCovering(_coveringSkipper, toSkip - skipped);
          break;
        case IndexNode::Strategy::kNoResult:
        case IndexNode::Strategy::kDocument:
        case IndexNode::Strategy::kCount:
          TRI_ASSERT(_documentSkipper != nullptr);
          _cursor->nextDocument(_documentSkipper, toSkip - skipped);
          break;
      }
      skipped +=
          _context.getAndResetNumScanned() - _context.getAndResetNumFiltered();
    }
  } else {
    _cursor->skip(toSkip, skipped);
  }

  TRI_ASSERT(skipped <= toSkip);
  TRI_ASSERT(skipped == toSkip || !hasMore());

  return static_cast<size_t>(skipped);
}

bool IndexExecutor::CursorReader::isCovering() const {
  return _strategy == IndexNode::Strategy::kCovering ||
         _strategy == IndexNode::Strategy::kCoveringFilterScanOnly ||
         _strategy == IndexNode::Strategy::kCoveringFilterOnly;
}

void IndexExecutor::CursorReader::reset() {
  TRI_ASSERT(_cursor != nullptr);

  if (_condition == nullptr || !_infos.hasNonConstParts()) {
    // Const case
    _cursor->reset();
    return;
  }

  // update cache statistics from cursor
  auto [ch, cm] = _cursor->getAndResetCacheStats();
  _cursorStats.incrCacheHits(ch);
  _cursorStats.incrCacheMisses(cm);

  if (_cursor->canRearm()) {
    _cursorStats.incrCursorsRearmed();
    bool didRearm = _cursor->rearm(_condition, _infos.getOutVariable(),
                                   _infos.getOptions());
    if (!didRearm) {
      // iterator does not support the condition
      // It will not create any results
      _cursor =
          std::make_unique<EmptyIndexIterator>(_cursor->collection(), &_trx);
    }
  } else {
    // We need to build a fresh search and cannot go the rearm shortcut
    _cursorStats.incrCursorsCreated();
    _cursor = _trx.indexScanForCondition(
        _infos.query().resourceMonitor(), _index, _condition,
        _infos.getOutVariable(), _infos.getOptions(), _infos.canReadOwnWrites(),
        transaction::Methods::kNoMutableConditionIdx);
  }
}

IndexExecutor::IndexExecutor(Fetcher& fetcher, Infos& infos)
    : _trx(infos.query().newTrxContext()),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _state(ExecutorState::HASMORE),
      _documentProducingFunctionContext(_trx, _input, infos),
      _infos(infos),
      _ast(_infos.query()),
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

void IndexExecutor::initIndexes(InputAqlItemRow const& input) {
  // We start with a different context. Return documents found in the previous
  // context again.
  _documentProducingFunctionContext.reset();
  // Find out about the actual values for the bounds in the variable bound case:

  if (!_infos.getNonConstExpressions().empty()) {
    TRI_ASSERT(_infos.getCondition() != nullptr);

    if (_infos.getV8Expression()) {
#ifdef USE_v8
      // must have a V8 context here to protect Expression::execute()
      auto cleanup = [this]() {
        if (arangodb::ServerState::instance()->isRunningInCluster()) {
          _infos.query().exitV8Executor();
        }
      };

      _infos.query().enterV8Executor();
      auto sg = arangodb::scopeGuard([&]() noexcept { cleanup(); });

      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      v8::HandleScope scope(isolate);  // do not delete this!

      executeExpressions(input);
      TRI_IF_FAILURE("IndexBlock::executeV8") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
#else
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "unexpected v8 function call in IndexExecutor");
#endif
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

void IndexExecutor::executeExpressions(InputAqlItemRow const& input) {
  TRI_ASSERT(_infos.getCondition() != nullptr);
  TRI_ASSERT(!_infos.getNonConstExpressions().empty());

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  auto* condition = const_cast<AstNode*>(_infos.getCondition());
  // modify the existing node in place
  TEMPORARILY_UNLOCK_NODE(condition);

  if (_expressionContext == nullptr) {
    _expressionContext = std::make_unique<ExecutorExpressionContext>(
        _trx, _infos.query(),
        _documentProducingFunctionContext.aqlFunctionsInternalCache(), input,
        _infos.getVarsToRegister());
  } else {
    _expressionContext->adjustInputRow(input);
  }

  _ast.clearMost();

  for (size_t posInExpressions = 0;
       posInExpressions < _infos.getNonConstExpressions().size();
       ++posInExpressions) {
    NonConstExpression* toReplace =
        _infos.getNonConstExpressions()[posInExpressions].get();
    auto exp = toReplace->expression.get();

    bool mustDestroy;
    AqlValue a = exp->execute(_expressionContext.get(), mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(&_trx.vpackOptions());
    VPackSlice slice = materializer.slice(a);
    AstNode* evaluatedNode = _ast.nodeFromVPack(slice, true);

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
        if (_infos.isOneIndexCondition()) {
          TRI_ASSERT(numTotal == 1);
          conditionNode = _infos.getCondition();
        } else {
          TRI_ASSERT(_infos.getIndexes().size() ==
                     _infos.getCondition()->numMembers());
          TRI_ASSERT(_infos.getCondition()->numMembers() > infoIndex);
          conditionNode = _infos.getCondition()->getMember(infoIndex);
        }
      }
      _cursors.emplace_back(_trx, _infos, conditionNode,
                            _infos.getIndexes()[infoIndex],
                            _documentProducingFunctionContext, _cursorStats,
                            needsUniquenessCheck());
    } else {
      // Next index exists, need a reset.
      getCursor().reset();
    }
    // We have a cursor now.
    TRI_ASSERT(_currentIndex < _cursors.size());
    // Check if this cursor has more (some might know already)
    if (getCursor().hasMore()) {
      // The current cursor has data.
      _documentProducingFunctionContext.setAllowCoveringIndexOptimization(
          getCursor().isCovering());
      if (!_infos.hasMultipleExpansions()) {
        // If we have multiple expansions
        // We unfortunately need to insert found documents
        // in every index
        _documentProducingFunctionContext.setIsLastIndex(_currentIndex ==
                                                         numTotal - 1);
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

[[nodiscard]] auto IndexExecutor::expectedNumberOfRows(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
    -> size_t {
  if (_infos.strategy() == IndexNode::Strategy::kCount) {
    // when we are counting, we will always return a single row
    return std::max<size_t>(input.countShadowRows(), 1);
  }
  // Otherwise we do not know.
  return call.getLimit();
}

auto IndexExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("IndexExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  IndexStats stats{};

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumFiltered() == 0);
  _documentProducingFunctionContext.setOutputRow(&output);

  INTERNAL_LOG_IDX << "IndexExecutor::produceRows " << output.getClientCall();

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
          << "IndexExecutor::produceRows input not initialized, peek next row: "
          << _state << " " << std::boolalpha << _input.isInitialized();

      if (_input.isInitialized()) {
        INTERNAL_LOG_IDX << "IndexExecutor::produceRows initIndexes";
        initIndexes(_input);
        if (!advanceCursor()) {
          INTERNAL_LOG_IDX
              << "IndexExecutor::produceRows failed to advanceCursor "
                 "after init";
          inputRange.advanceDataRow();
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
      INTERNAL_LOG_IDX << "IndexExecutor::produceRows::innerLoop hasMore = "
                       << std::boolalpha << getCursor().hasMore() << " "
                       << output.numRowsLeft();

      if (!getCursor().hasMore() && !advanceCursor()) {
        INTERNAL_LOG_IDX << "IndexExecutor::produceRows::innerLoop cursor does "
                            "not have more and advancing failed";
        inputRange.advanceDataRow();
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        break;
      }

      TRI_ASSERT(getCursor().hasMore());

      // Read the next elements from the index
      bool more =
          getCursor().readIndex(output, _documentProducingFunctionContext);
      TRI_ASSERT(more == getCursor().hasMore());

      if (!more && _currentIndex + 1 >= _cursors.size() && output.isFull()) {
        // optimization for the case that the cursor cannot produce more data,
        // we are at the last cursor and we have filled up the output block
        // completely.
        inputRange.advanceDataRow();
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
      }

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

    stats.incrScanned(
        _documentProducingFunctionContext.getAndResetNumScanned());
    stats.incrFiltered(
        _documentProducingFunctionContext.getAndResetNumFiltered());
    stats.incrDocumentLookups(
        _documentProducingFunctionContext.getAndResetNumLookups());
  }

  // ok to update the stats at the end of the method
  stats.incrCursorsCreated(_cursorStats.getAndResetCursorsCreated());
  stats.incrCursorsRearmed(_cursorStats.getAndResetCursorsRearmed());
  stats.incrCacheHits(_cursorStats.getAndResetCacheHits());
  stats.incrCacheMisses(_cursorStats.getAndResetCacheMisses());

  INTERNAL_LOG_IDX << "IndexExecutor::produceRows reporting state "
                   << returnState();
  return {returnState(), stats, AqlCall{}};
}

auto IndexExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                  AqlCall& clientCall)
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
    INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange skipped " << _skipped
                     << " " << clientCall.getOffset();
    // get an input row first, if necessary
    if (!_input.isInitialized()) {
      std::tie(_state, _input) = inputRange.peekDataRow();
      INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange input not initialized, "
                          "peek next row: "
                       << _state << " " << std::boolalpha
                       << _input.isInitialized();

      if (_input.isInitialized()) {
        INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange initIndexes";
        initIndexes(_input);
        if (!advanceCursor()) {
          INTERNAL_LOG_IDX
              << "IndexExecutor::skipRowsRange failed to advanceCursor "
                 "after init";
          inputRange.advanceDataRow();
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
      inputRange.advanceDataRow();
      _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
      continue;
    }

    auto toSkip = clientCall.getOffset();
    if (toSkip == 0) {
      TRI_ASSERT(clientCall.needsFullCount());
      toSkip = ExecutionBlock::SkipAllSize();
    }
    TRI_ASSERT(toSkip > 0);
    INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange skipIndex(" << toSkip
                     << ")";
    size_t skippedNow = getCursor().skipIndex(toSkip);
    INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange skipIndex(...) == "
                     << skippedNow;

    stats.incrScanned(skippedNow);
    _skipped += skippedNow;
    clientCall.didSkip(skippedNow);
  }

  size_t skipped = _skipped;
  _skipped = 0;

  AqlCall upstreamCall;

  INTERNAL_LOG_IDX << "IndexExecutor::skipRowsRange returning " << returnState()
                   << " " << skipped << " " << upstreamCall;
  return {returnState(), stats, skipped, std::move(upstreamCall)};
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
