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

#include "EnumerateCollectionExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

EnumerateCollectionExecutorInfos::EnumerateCollectionExecutorInfos(
    RegisterId outputRegister, aql::QueryContext& query,
    Collection const* collection, Variable const* outVariable,
    bool produceResult, Expression* filter,
    arangodb::aql::Projections projections,
    std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs,
    bool random, bool count, ReadOwnWrites readOwnWrites)
    : _query(query),
      _collection(collection),
      _outVariable(outVariable),
      _filter(filter),
      _projections(std::move(projections)),
      _outputRegisterId(outputRegister),
      _filterVarsToRegs(std::move(filterVarsToRegs)),
      _produceResult(produceResult),
      _random(random),
      _count(count),
      _readOwnWrites(readOwnWrites) {}

Collection const* EnumerateCollectionExecutorInfos::getCollection() const {
  return _collection;
}

Variable const* EnumerateCollectionExecutorInfos::getOutVariable() const {
  return _outVariable;
}

QueryContext& EnumerateCollectionExecutorInfos::getQuery() const {
  return _query;
}

Expression* EnumerateCollectionExecutorInfos::getFilter() const noexcept {
  return _filter;
}

ResourceMonitor&
EnumerateCollectionExecutorInfos::getResourceMonitor() noexcept {
  return _query.resourceMonitor();
}

arangodb::aql::Projections const&
EnumerateCollectionExecutorInfos::getProjections() const noexcept {
  return _projections;
}

arangodb::aql::Projections const&
EnumerateCollectionExecutorInfos::getFilterProjections() const noexcept {
  return _filterProjections;
}

bool EnumerateCollectionExecutorInfos::getProduceResult() const noexcept {
  return _produceResult;
}

bool EnumerateCollectionExecutorInfos::getRandom() const noexcept {
  return _random;
}

bool EnumerateCollectionExecutorInfos::getCount() const noexcept {
  return _count;
}

RegisterId EnumerateCollectionExecutorInfos::getOutputRegisterId() const {
  return _outputRegisterId;
}

std::vector<std::pair<VariableId, RegisterId>> const&
EnumerateCollectionExecutorInfos::getFilterVarsToRegister() const noexcept {
  return _filterVarsToRegs;
}

EnumerateCollectionExecutor::EnumerateCollectionExecutor(Fetcher& fetcher,
                                                         Infos& infos)
    : _trx(infos.getQuery().newTrxContext()),
      _infos(infos),
      _documentProducingFunctionContext(_trx, _currentRow, infos),
      _state(ExecutionState::HASMORE),
      _executorState(ExecutorState::HASMORE),
      _cursorHasMore(false),
      _currentRow(InputAqlItemRow{CreateInvalidInputRowHint{}}) {
  TRI_ASSERT(_trx.status() == transaction::Status::RUNNING);

  _cursor = _trx.indexScan(_infos.getCollection()->name(),
                           (_infos.getRandom()
                                ? transaction::Methods::CursorType::ANY
                                : transaction::Methods::CursorType::ALL),
                           infos.canReadOwnWrites())
                .waitAndGet();

  if (!_infos.getCount()) {
    if (_infos.getProduceResult()) {
      _documentProducer = buildDocumentCallback<false, false>(
          _documentProducingFunctionContext);
    } else {
      _documentNonProducer =
          getNullCallback<false, false>(_documentProducingFunctionContext);
    }
  }

  if (_infos.getFilter() != nullptr) {
    _documentSkipper =
        buildDocumentCallback<false, true>(_documentProducingFunctionContext);
  }
}

EnumerateCollectionExecutor::~EnumerateCollectionExecutor() = default;

uint64_t EnumerateCollectionExecutor::skipEntries(
    size_t toSkip, EnumerateCollectionStats& stats) {
  uint64_t actuallySkipped = 0;

  TRI_ASSERT(!_infos.getCount());

  if (_infos.getFilter() == nullptr) {
    _cursor->skip(toSkip, actuallySkipped);
    stats.incrScanned(actuallySkipped);
    std::ignore = _documentProducingFunctionContext.getAndResetNumScanned();
  } else {
    TRI_ASSERT(_documentSkipper != nullptr);
    _cursor->nextDocument(_documentSkipper, toSkip);
    uint64_t filtered =
        _documentProducingFunctionContext.getAndResetNumFiltered();
    uint64_t scanned =
        _documentProducingFunctionContext.getAndResetNumScanned();
    TRI_ASSERT(scanned >= filtered);
    stats.incrFiltered(filtered);
    stats.incrScanned(scanned);
    actuallySkipped = scanned - filtered;
  }
  _cursorHasMore = _cursor->hasMore();

  return actuallySkipped;
}

std::tuple<ExecutorState, EnumerateCollectionStats, size_t, AqlCall>
EnumerateCollectionExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                           AqlCall& call) {
  TRI_ASSERT(!_infos.getCount());

  AqlCall upstreamCall{};
  EnumerateCollectionStats stats{};

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumFiltered() == 0);
  while ((inputRange.hasDataRow() || _cursorHasMore) && call.needSkipMore()) {
    uint64_t skipped = 0;

    if (!_cursorHasMore) {
      initializeNewRow(inputRange);
    }

    if (_cursorHasMore) {
      TRI_ASSERT(_currentRow.isInitialized());
      // if offset is > 0, we're in offset skip phase
      if (call.getOffset() > 0) {
        skipped += skipEntries(call.getOffset(), stats);
      } else {
        // fullCount phase
        if (_infos.getFilter() == nullptr) {
          _cursor->skipAll(skipped);
          stats.incrScanned(skipped);
          /* For some reason this does not hold
           * TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned()
           * == skipped);
           */
          std::ignore =
              _documentProducingFunctionContext.getAndResetNumScanned();
        } else {
          // We need to call this to do the Accounting of FILTERED correctly.
          skipped += skipEntries(ExecutionBlock::SkipAllSize(), stats);
        }
      }
      _cursorHasMore = _cursor->hasMore();
      call.didSkip(skipped);
    }
  }
  if (_cursorHasMore) {
    return {ExecutorState::HASMORE, stats, call.getSkipCount(), upstreamCall};
  }
  if (!call.needsFullCount()) {
    // Do not overfetch too much
    upstreamCall.softLimit = call.getOffset();
    // else we do unlimited softLimit.
    // we are going to return everything anyways.
  }

  return {inputRange.upstreamState(), stats, call.getSkipCount(), upstreamCall};
}

void EnumerateCollectionExecutor::initializeNewRow(
    AqlItemBlockInputRange& inputRange) {
  if (_currentRow) {
    // moves one row forward
    inputRange.advanceDataRow();
  }
  std::tie(std::ignore, _currentRow) = inputRange.peekDataRow();
  if (!_currentRow) {
    return;
  }

  TRI_ASSERT(_currentRow.isInitialized());

  _cursor->reset();
  _cursorHasMore = _cursor->hasMore();
}

[[nodiscard]] auto EnumerateCollectionExecutor::expectedNumberOfRows(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
    -> size_t {
  if (_infos.getCount()) {
    // when we are counting, we will always return a single row
    return std::max<size_t>(input.countShadowRows(), 1);
  }
  // Otherwise we do not know.
  return call.getLimit();
}

std::tuple<ExecutorState, EnumerateCollectionStats, AqlCall>
EnumerateCollectionExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                         OutputAqlItemRow& output) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  EnumerateCollectionStats stats{};
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumFiltered() == 0);
  _documentProducingFunctionContext.setOutputRow(&output);

  // validate that the output pointer in documentProducingFunctionContext is the
  // same as in output
  TRI_ASSERT(&output == &_documentProducingFunctionContext.getOutputRow());

  while (inputRange.hasDataRow() && !output.isFull()) {
    // validate that output has a valid AqlItemBlock
    TRI_ASSERT(output.isInitialized());

    if (!_cursorHasMore) {
      // the following call can change the value of _cursorHasMore
      initializeNewRow(inputRange);
    }

    if (_cursorHasMore) {
      TRI_ASSERT(_currentRow.isInitialized());
      if (_infos.getCount()) {
        // count optimization
        TRI_ASSERT(!_documentProducingFunctionContext.hasFilter());
        uint64_t counter = 0;
        _cursor->skipAll(counter);

        InputAqlItemRow const& input =
            _documentProducingFunctionContext.getInputRow();
        RegisterId registerId =
            _documentProducingFunctionContext.getOutputRegister();
        TRI_ASSERT(!output.isFull());
        AqlValue v((AqlValueHintUInt(counter)));
        AqlValueGuard guard{v, true};
        output.moveValueInto(registerId, input, &guard);
        TRI_ASSERT(output.produced());
        output.advanceRow();

        _cursorHasMore = _cursor->hasMore();
      } else if (_infos.getProduceResult()) {
        // properly build up results by fetching the actual documents
        // using nextDocument()
        TRI_ASSERT(_documentProducer != nullptr);
        _cursorHasMore = _cursor->nextDocument(_documentProducer,
                                               output.numRowsLeft() /*atMost*/);
      } else {
        // performance optimization: we do not need the documents at all.
        // so just call next()
        TRI_ASSERT(!_documentProducingFunctionContext.hasFilter());
        TRI_ASSERT(_documentNonProducer != nullptr);
        _cursorHasMore = _cursor->next(_documentNonProducer,
                                       output.numRowsLeft() /*atMost*/);
      }

      stats.incrScanned(
          _documentProducingFunctionContext.getAndResetNumScanned());
      stats.incrFiltered(
          _documentProducingFunctionContext.getAndResetNumFiltered());
    }

    TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  if (!_cursorHasMore) {
    initializeNewRow(inputRange);
  }
  return {inputRange.upstreamState(), stats, upstreamCall};
}

void EnumerateCollectionExecutor::initializeCursor() {
  _state = ExecutionState::HASMORE;
  _executorState = ExecutorState::HASMORE;
  _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  _cursorHasMore = false;
  _cursor->reset();
}
