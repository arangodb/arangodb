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
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"

#include <lib/Logger/LogMacros.h>

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
      _outputRegisterId(outputRegister),
      _engine(engine),
      _collection(collection),
      _outVariable(outVariable),
      _projections(projections),
      _trxPtr(trxPtr),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _useRawDocumentPointers(useRawDocumentPointers),
      _produceResult(produceResult),
      _random(random) {}

EnumerateCollectionExecutor::EnumerateCollectionExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _state(ExecutionState::HASMORE),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _allowCoveringIndexOptimization(true),
      _cursorHasMore(false) {
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
  this->setProducingFunction(
      buildCallback(_documentProducer, _infos.getOutVariable(),
                    _infos.getProduceResult(), _infos.getProjections(),
                    _infos.getTrxPtr(), _infos.getCoveringIndexAttributePositions(),
                    _allowCoveringIndexOptimization, _infos.getUseRawDocumentPointers()));
}

EnumerateCollectionExecutor::~EnumerateCollectionExecutor() = default;

std::pair<ExecutionState, EnumerateCollectionStats> EnumerateCollectionExecutor::produceRow(
    OutputAqlItemRow& output) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  EnumerateCollectionStats stats{};

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
    TRI_ASSERT(_cursor->hasMore());

    TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (_infos.getProduceResult()) {
      // properly build up results by fetching the actual documents
      // using nextDocument()
      _cursorHasMore = _cursor->nextDocument(
          [&](LocalDocumentId const&, VPackSlice slice) {
            _documentProducer(_input, output, slice, _infos.getOutputRegisterId());
            stats.incrScanned();
          },
          1 /*atMost*/);
    } else {
      // performance optimization: we do not need the documents at all,
      // so just call next()
      _cursorHasMore = _cursor->next(
          [&](LocalDocumentId const&) {
            _documentProducer(_input, output, VPackSlice::nullSlice(),
                              _infos.getOutputRegisterId());
            stats.incrScanned();
          },
          1 /*atMost*/);
    }

    if (_state == ExecutionState::DONE && !_cursorHasMore) {
      return {_state, stats};
    }
    return {ExecutionState::HASMORE, stats};
  }
}

std::pair<ExecutionState, size_t> EnumerateCollectionExecutor::skipRows(size_t toSkip) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::skipRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  EnumerateCollectionStats stats{};

  if (!_cursorHasMore) {
    std::tie(_state, _input) = _fetcher.fetchRow();

    if (_state == ExecutionState::WAITING) {
      LOG_DEVEL << "returned waiting";
      return {_state, 0};
    }

    if (!_input) {
      LOG_DEVEL << "no input, we're done. returning done,";
      TRI_ASSERT(_state == ExecutionState::DONE);
      return {_state, 0};
    }
    _cursor->reset();
    _cursorHasMore = _cursor->hasMore();
  }

  TRI_ASSERT(_input.isInitialized());
  TRI_ASSERT(_cursor->hasMore());

  TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  uint64_t actuallySkipped = 0;
  _cursor->skip(toSkip, actuallySkipped);
  _cursorHasMore = _cursor->hasMore();

  if (actuallySkipped == toSkip && !_cursorHasMore) {
    LOG_DEVEL << "returned done and skipped with: " << actuallySkipped;
    return {ExecutionState::DONE, actuallySkipped};
  }

  /* TODO: just a note, we do not need those checks, because we always run into hasmore
  if (actuallySkipped < toSkip) {
    return {ExecutionState::HASMORE, stats};
  }
  if (actuallySkipped == 0) {
    return {ExecutionState::HASMORE, stats};
  }*/

  LOG_DEVEL << "returned HASMORE and skipped with: " << actuallySkipped;
  return {ExecutionState::HASMORE, actuallySkipped};
}

#ifndef USE_ENTERPRISE
bool EnumerateCollectionExecutor::waitForSatellites(ExecutionEngine* engine,
                                                    Collection const* collection) const {
  return true;
}
#endif
