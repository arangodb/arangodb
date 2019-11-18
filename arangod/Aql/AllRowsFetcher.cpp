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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AllRowsFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/DependencyProxy.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ShadowAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

std::pair<ExecutionState, AqlItemMatrix const*> AllRowsFetcher::fetchAllRows() {
  switch (_dataFetchedState) {
    case ALL_DATA_FETCHED:
      // Avoid unnecessary upstream calls
      return {ExecutionState::DONE, nullptr};
    case NONE:
    case SHADOW_ROW_FETCHED: {
      auto state = fetchData();
      if (state == ExecutionState::WAITING) {
        return {state, nullptr};
      }
      _dataFetchedState = ALL_DATA_FETCHED;
      return {state, _aqlItemMatrix.get()};
    }
    case DATA_FETCH_ONGOING:
      // Invalid state, we switch between singleRow
      // and allRows fetches.
      TRI_ASSERT(false);
      // In production hand out the Matrix
      // This way the function behaves as if no single row fetch would have taken place
      _dataFetchedState = ALL_DATA_FETCHED;
      return {ExecutionState::DONE, _aqlItemMatrix.get()};
  }
  // Unreachable code
  TRI_ASSERT(false);
  return {ExecutionState::DONE, nullptr};
}

std::pair<ExecutionState, InputAqlItemRow> AllRowsFetcher::fetchRow(size_t atMost) {
  switch (_dataFetchedState) {
    case ALL_DATA_FETCHED:
      // We have already returned all data to the next shadow row
      // Avoid unnecessary upstream calls
      return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
    case NONE:
    case SHADOW_ROW_FETCHED: {
      // We need to get new data!
      auto state = fetchData();
      if (state == ExecutionState::WAITING) {
        return {state, InputAqlItemRow{CreateInvalidInputRowHint{}}};
      }
      TRI_ASSERT(_aqlItemMatrix != nullptr);
      _rowIndexes = _aqlItemMatrix->produceRowIndexes();
      if (_rowIndexes.empty()) {
        // We do not ahve indexes in the next block
        // So we return invalid row and can set ALL_DATA_FETCHED
        _dataFetchedState = ALL_DATA_FETCHED;
        return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
      }
      _nextReturn = 0;
      _dataFetchedState = DATA_FETCH_ONGOING;
    }
      [[fallthrough]];
    case DATA_FETCH_ONGOING: {
      TRI_ASSERT(_nextReturn < _rowIndexes.size());
      TRI_ASSERT(_aqlItemMatrix != nullptr);
      auto row = _aqlItemMatrix->getRow(_rowIndexes[_nextReturn]);
      _nextReturn++;
      if (_nextReturn == _rowIndexes.size()) {
        _dataFetchedState = ALL_DATA_FETCHED;
        return {ExecutionState::DONE, row};
      }
      return {ExecutionState::HASMORE, row};
    }
  }

  return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
}

ExecutionState AllRowsFetcher::fetchData() {
  if (_upstreamState == ExecutionState::DONE) {
    TRI_ASSERT(_aqlItemMatrix != nullptr);
    return ExecutionState::DONE;
  }
  if (fetchUntilDone() == ExecutionState::WAITING) {
    return ExecutionState::WAITING;
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  return ExecutionState::DONE;
}

ExecutionState AllRowsFetcher::fetchUntilDone() {
  if (_aqlItemMatrix == nullptr) {
    _aqlItemMatrix = std::make_unique<AqlItemMatrix>(getNrInputRegisters());
  }

  ExecutionState state = ExecutionState::HASMORE;
  SharedAqlItemBlockPtr block;

  while (state == ExecutionState::HASMORE && !_aqlItemMatrix->stoppedOnShadowRow()) {
    std::tie(state, block) = fetchBlock();
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(block == nullptr);
      return state;
    }
    if (block == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
    } else {
      _aqlItemMatrix->addBlock(std::move(block));
    }
  }

  TRI_ASSERT(_aqlItemMatrix != nullptr);
  return state;
}

std::pair<ExecutionState, size_t> AllRowsFetcher::preFetchNumberOfRows(size_t) {
  // TODO: Fix this as soon as we have counters for ShadowRows within here.
  if (_upstreamState == ExecutionState::DONE) {
    TRI_ASSERT(_aqlItemMatrix != nullptr);
    return {ExecutionState::DONE, _aqlItemMatrix->size()};
  }
  if (fetchUntilDone() == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, 0};
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  return {ExecutionState::DONE, _aqlItemMatrix->size()};
}

AllRowsFetcher::AllRowsFetcher(DependencyProxy<BlockPassthrough::Disable>& executionBlock)
    : _dependencyProxy(&executionBlock),
      _aqlItemMatrix(nullptr),
      _upstreamState(ExecutionState::HASMORE),
      _blockToReturnNext(0),
      _dataFetchedState(NONE) {}

RegisterId AllRowsFetcher::getNrInputRegisters() const {
  return _dependencyProxy->getNrInputRegisters();
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> AllRowsFetcher::fetchBlock() {
  auto res = _dependencyProxy->fetchBlock();

  _upstreamState = res.first;

  return res;
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> AllRowsFetcher::fetchBlockForModificationExecutor(
    std::size_t limit = ExecutionBlock::DefaultBatchSize()) {
  // TODO this method is considered obsolete.
  // It cannot yet be removed as we need modification on the calling Executors which is ongoing
  // However this method will not be fixed and updated for ShadowRows
  while (_upstreamState != ExecutionState::DONE) {
    auto state = fetchUntilDone();
    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  // This is to remember that this function is obsolete and needs to be removed
  // before releasing the ShadowRow improvement!
  TRI_ASSERT(!_aqlItemMatrix->stoppedOnShadowRow());
  auto size = _aqlItemMatrix->numberOfBlocks();
  if (_blockToReturnNext >= size) {
    return {ExecutionState::DONE, nullptr};
  }
  auto blk = _aqlItemMatrix->getBlock(_blockToReturnNext);
  ++_blockToReturnNext;

  return {(_blockToReturnNext < size ? ExecutionState::HASMORE : ExecutionState::DONE),
          std::move(blk)};
}

ExecutionState AllRowsFetcher::upstreamState() {
  if (_aqlItemMatrix == nullptr) {
    // We have not pulled anything yet!
    return ExecutionState::HASMORE;
  }
  if (_upstreamState == ExecutionState::WAITING) {
    return ExecutionState::WAITING;
  }
  if (_blockToReturnNext >= _aqlItemMatrix->numberOfBlocks()) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

std::pair<ExecutionState, ShadowAqlItemRow> AllRowsFetcher::fetchShadowRow(size_t atMost) {
  TRI_ASSERT(_dataFetchedState != DATA_FETCH_ONGOING);
  if (ADB_UNLIKELY(_dataFetchedState == DATA_FETCH_ONGOING)) {
    // If we get into this case the logic of the executors is violated.
    // We urgently need to investigate every query that gets into this sate.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Internal AQL dataFlow error.");
  }
  // We are required to fetch data rows first!
  TRI_ASSERT(_aqlItemMatrix != nullptr);

  ExecutionState state = _upstreamState;

  if (!_aqlItemMatrix->stoppedOnShadowRow() || _aqlItemMatrix->size() == 0) {
    // We ended on a ShadowRow, we are required to fetch data.
    state = fetchData();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
    }
    // reset to upstream state (might be modified by fetchData())
    state = _upstreamState;
  }

  ShadowAqlItemRow row = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
  // We do only POP a shadow row, if we are actually stopping on one.
  // and if we have either returned all data before it (ALL_DATA_FETCHED)
  // or it is NOT relevant.
  if (_aqlItemMatrix->stoppedOnShadowRow() &&
      (_dataFetchedState == ALL_DATA_FETCHED ||
       !_aqlItemMatrix->peekShadowRow().isRelevant())) {
    row = _aqlItemMatrix->popShadowRow();
    // We handed out a shadowRow
    _dataFetchedState = SHADOW_ROW_FETCHED;
  }

  // We need to return more only if we are in the state that we have read a
  // ShadowRow And we still have items in the Matrix.
  // else we return the upstream state.
  if (_dataFetchedState == SHADOW_ROW_FETCHED &&
      (_aqlItemMatrix->size() > 0 || _aqlItemMatrix->stoppedOnShadowRow())) {
    state = ExecutionState::HASMORE;
  }

  return {state, row};
}
