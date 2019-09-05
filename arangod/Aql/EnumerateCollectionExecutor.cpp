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

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Logger/LogMacros.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

EnumerateCollectionExecutorInfos::EnumerateCollectionExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep, ExecutionEngine* engine,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    std::vector<std::string> const& projections, transaction::Methods* trxPtr,
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool useRawDocumentPointers, bool random)
    : ExecutorInfos(make_shared_unordered_set(),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _engine(engine),
      _collection(collection),
      _outVariable(outVariable),
      _trxPtr(trxPtr),
      _projections(projections),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _outputRegisterId(outputRegister),
      _useRawDocumentPointers(useRawDocumentPointers),
      _produceResult(produceResult),
      _random(random) {}

EnumerateCollectionExecutor::EnumerateCollectionExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _documentProducer(nullptr),
      _documentProducingFunctionContext(_input, nullptr, _infos.getOutputRegisterId(),
                                        _infos.getProduceResult(),
                                        _infos.getProjections(), _infos.getTrxPtr(),
                                        _infos.getCoveringIndexAttributePositions(),
                                        true, _infos.getUseRawDocumentPointers(), false),
      _state(ExecutionState::HASMORE),
      _cursorHasMore(false),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}) {
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
    this->setProducingFunction(buildCallback<false>(_documentProducingFunctionContext));
  }
}

EnumerateCollectionExecutor::~EnumerateCollectionExecutor() = default;

std::pair<ExecutionState, EnumerateCollectionStats> EnumerateCollectionExecutor::produceRows(
    OutputAqlItemRow& output) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  EnumerateCollectionStats stats{};
  TRI_ASSERT(_documentProducingFunctionContext.getAndResetNumScanned() == 0);
  _documentProducingFunctionContext.setOutputRow(&output);

  while (true) {
    if (!_cursorHasMore) {
      std::tie(_state, _input) = _fetcher.fetchRow();

      if (_state == ExecutionState::WAITING) {
        return {_state, stats};
      }

      if (!_input) {
        TRI_ASSERT(_state == ExecutionState::DONE);
        return {_state, stats};
      }
      _cursor->reset();
      _cursorHasMore = _cursor->hasMore();
      continue;
    }

    TRI_ASSERT(_input.isInitialized());

    TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (_infos.getProduceResult()) {
      // properly build up results by fetching the actual documents
      // using nextDocument()
      _cursorHasMore =
          _cursor->nextDocument(_documentProducer, output.numRowsLeft() /*atMost*/);
    } else {
      // performance optimization: we do not need the documents at all,
      // so just call next()
      _cursorHasMore =
          _cursor->next(getNullCallback<false>(_documentProducingFunctionContext),
                        output.numRowsLeft() /*atMost*/);
    }

    stats.incrScanned(_documentProducingFunctionContext.getAndResetNumScanned());

    if (_state == ExecutionState::DONE && !_cursorHasMore) {
      return {_state, stats};
    }
    return {ExecutionState::HASMORE, stats};
  }
}

std::tuple<ExecutionState, EnumerateCollectionStats, size_t> EnumerateCollectionExecutor::skipRows(size_t const toSkip) {
  EnumerateCollectionStats stats{};
  TRI_IF_FAILURE("EnumerateCollectionExecutor::skipRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!_cursorHasMore) {
    std::tie(_state, _input) = _fetcher.fetchRow();

    if (_state == ExecutionState::WAITING) {
      return std::make_tuple(_state, stats, 0); // tupple, cannot use initializer list due to build failure
    }

    if (!_input) {
      TRI_ASSERT(_state == ExecutionState::DONE);
      return std::make_tuple(_state, stats, 0); // tupple, cannot use initializer list due to build failure
    }

    _cursor->reset();
    _cursorHasMore = _cursor->hasMore();
  }

  TRI_ASSERT(_input.isInitialized());

  uint64_t actuallySkipped = 0;
  _cursor->skip(toSkip, actuallySkipped);
  _cursorHasMore = _cursor->hasMore();
  stats.incrScanned(actuallySkipped);

  if (_state == ExecutionState::DONE && !_cursorHasMore) {
    return std::make_tuple(ExecutionState::DONE, stats, actuallySkipped); // tupple, cannot use initializer list due to build failure
  }

  return std::make_tuple(ExecutionState::HASMORE, stats, actuallySkipped); // tupple, cannot use initializer list due to build failure
}

void EnumerateCollectionExecutor::initializeCursor() {
  _state = ExecutionState::HASMORE;
  _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
  setAllowCoveringIndexOptimization(true);
  _cursorHasMore = false;
  _cursor->reset();
}

#ifndef USE_ENTERPRISE
bool EnumerateCollectionExecutor::waitForSatellites(ExecutionEngine* engine,
                                                    Collection const* collection) const {
  return true;
}
#endif
