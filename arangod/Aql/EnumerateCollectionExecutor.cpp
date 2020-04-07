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

#include "EnumerateCollectionExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "AqlCall.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"

#include <Logger/LogMacros.h>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
std::vector<size_t> const emptyAttributePositions;
}

EnumerateCollectionExecutorInfos::EnumerateCollectionExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep, ExecutionEngine* engine,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    Expression* filter, std::vector<std::string> const& projections,
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool random)
    : ExecutorInfos(make_shared_unordered_set(),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _engine(engine),
      _collection(collection),
      _outVariable(outVariable),
      _filter(filter),
      _projections(projections),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _outputRegisterId(outputRegister),
      _produceResult(produceResult),
      _random(random) {}

ExecutionEngine* EnumerateCollectionExecutorInfos::getEngine() {
  return _engine;
}

Collection const* EnumerateCollectionExecutorInfos::getCollection() const {
  return _collection;
}

Variable const* EnumerateCollectionExecutorInfos::getOutVariable() const {
  return _outVariable;
}

Query* EnumerateCollectionExecutorInfos::getQuery() const {
  return _engine->getQuery();
}

transaction::Methods* EnumerateCollectionExecutorInfos::getTrxPtr() const {
  return _engine->getQuery()->trx();
}

Expression* EnumerateCollectionExecutorInfos::getFilter() const {
  return _filter;
}

std::vector<std::string> const& EnumerateCollectionExecutorInfos::getProjections() const noexcept {
  return _projections;
}

std::vector<size_t> const& EnumerateCollectionExecutorInfos::getCoveringIndexAttributePositions() const
    noexcept {
  return _coveringIndexAttributePositions;
}

bool EnumerateCollectionExecutorInfos::getProduceResult() const {
  return _produceResult;
}

bool EnumerateCollectionExecutorInfos::getRandom() const { return _random; }
RegisterId EnumerateCollectionExecutorInfos::getOutputRegisterId() const {
  return _outputRegisterId;
}

EnumerateCollectionExecutor::EnumerateCollectionExecutor(Fetcher&, Infos& infos)
    : _infos(infos),
      _documentProducer(nullptr),
      _documentProducingFunctionContext(_currentRow, nullptr, _infos.getOutputRegisterId(),
                                        _infos.getProduceResult(), _infos.getQuery(),
                                        _infos.getFilter(), _infos.getProjections(),
                                        _infos.getCoveringIndexAttributePositions(),
                                        true, false),
      _state(ExecutionState::HASMORE),
      _executorState(ExecutorState::HASMORE),
      _cursorHasMore(false),
      _currentRow(InputAqlItemRow{CreateInvalidInputRowHint{}}) {
  _cursor = std::make_unique<OperationCursor>(
      _infos.getTrxPtr()->indexScan(_infos.getCollection()->name(),
                                    (_infos.getRandom()
                                         ? transaction::Methods::CursorType::ANY
                                         : transaction::Methods::CursorType::ALL)));

  if (!waitForSatellites(_infos.getEngine(), _infos.getCollection())) {
    double maxWait = _infos.getEngine()->getQuery()->queryOptions().satelliteSyncWait;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
                                   "collection " + _infos.getCollection()->name() +
                                       " did not come into sync in time (" +
                                       std::to_string(maxWait) + ")");
  }
  if (_infos.getProduceResult()) {
    _documentProducer =
        buildDocumentCallback<false, false>(_documentProducingFunctionContext);
  }
  _documentSkipper = buildDocumentCallback<false, true>(_documentProducingFunctionContext);
}

EnumerateCollectionExecutor::~EnumerateCollectionExecutor() = default;

uint64_t EnumerateCollectionExecutor::skipEntries(size_t toSkip,
                                                  EnumerateCollectionStats& stats) {
  uint64_t actuallySkipped = 0;

  if (_infos.getFilter() == nullptr) {
    _cursor->skip(toSkip, actuallySkipped);
    stats.incrScanned(actuallySkipped);
    _documentProducingFunctionContext.getAndResetNumScanned();
  } else {
    _cursor->nextDocument(_documentSkipper, toSkip);
    size_t filtered = _documentProducingFunctionContext.getAndResetNumFiltered();
    size_t scanned = _documentProducingFunctionContext.getAndResetNumScanned();
    TRI_ASSERT(scanned >= filtered);
    stats.incrFiltered(filtered);
    stats.incrScanned(scanned);
    actuallySkipped = scanned - filtered;
  }
  _cursorHasMore = _cursor->hasMore();

  return actuallySkipped;
}

std::tuple<ExecutorState, EnumerateCollectionStats, size_t, AqlCall> EnumerateCollectionExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  AqlCall upstreamCall{};
  EnumerateCollectionStats stats{};

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumFiltered() == 0);
  while ((inputRange.hasDataRow() || _cursorHasMore) && call.shouldSkip()) {
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
           * TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == skipped);
           */
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

void EnumerateCollectionExecutor::initializeNewRow(AqlItemBlockInputRange& inputRange) {
  if (_currentRow) {
    std::ignore = inputRange.nextDataRow();
  }
  std::tie(_currentRowState, _currentRow) = inputRange.peekDataRow();
  if (!_currentRow) {
    return;
  }

  TRI_ASSERT(_currentRow.isInitialized());

  _cursor->reset();
  _cursorHasMore = _cursor->hasMore();
}

std::tuple<ExecutorState, EnumerateCollectionStats, AqlCall> EnumerateCollectionExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  EnumerateCollectionStats stats{};
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumFiltered() == 0);
  _documentProducingFunctionContext.setOutputRow(&output);
  while (inputRange.hasDataRow() && !output.isFull()) {
    if (!_cursorHasMore) {
      initializeNewRow(inputRange);
    }

    if (_cursorHasMore) {
      TRI_ASSERT(_currentRow.isInitialized());
      if (_infos.getProduceResult()) {
        // properly build up results by fetching the actual documents
        // using nextDocument()
        _cursorHasMore =
            _cursor->nextDocument(_documentProducer, output.numRowsLeft() /*atMost*/);
      } else {
        // performance optimization: we do not need the documents at all,
        // so just call next()
        TRI_ASSERT(!_documentProducingFunctionContext.hasFilter());
        _cursorHasMore =
            _cursor->next(getNullCallback<false>(_documentProducingFunctionContext),
                          output.numRowsLeft() /*atMost*/);
      }

      stats.incrScanned(_documentProducingFunctionContext.getAndResetNumScanned());
      stats.incrFiltered(_documentProducingFunctionContext.getAndResetNumFiltered());
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
#ifndef USE_ENTERPRISE
bool EnumerateCollectionExecutor::waitForSatellites(ExecutionEngine* engine,
                                                    Collection const* collection) const {
  return true;
}
#endif
