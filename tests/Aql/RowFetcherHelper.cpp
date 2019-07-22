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

#include "RowFetcherHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortExecutor.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::aql;

namespace {
}  // namespace

// -----------------------------------------
// - SECTION SINGLEROWFETCHER              -
// -----------------------------------------

template <bool passBlocksThrough>
SingleRowFetcherHelper<passBlocksThrough>::SingleRowFetcherHelper(
    AqlItemBlockManager& manager,
    std::shared_ptr<VPackBuffer<uint8_t>> const& vPackBuffer, bool returnsWaiting)
    : SingleRowFetcherHelper(manager, 1, returnsWaiting,
                             vPackBufferToAqlItemBlock(manager, vPackBuffer)) {}

template <bool passBlocksThrough>
SingleRowFetcherHelper<passBlocksThrough>::SingleRowFetcherHelper(::arangodb::aql::AqlItemBlockManager& manager,
                       size_t const blockSize, bool const returnsWaiting,
                       ::arangodb::aql::SharedAqlItemBlockPtr input)
    : SingleRowFetcher<passBlocksThrough>(),
      _returnsWaiting(returnsWaiting),
      _nrItems(input == nullptr ? 0 : input->size()),
      _blockSize(blockSize),
      _itemBlockManager(manager),
      _itemBlock(std::move(input)),
      _lastReturnedRow{CreateInvalidInputRowHint{}} {
  TRI_ASSERT(_blockSize > 0);
}

template <bool passBlocksThrough>
SingleRowFetcherHelper<passBlocksThrough>::~SingleRowFetcherHelper() = default;

template <bool passBlocksThrough>
// NOLINTNEXTLINE google-default-arguments
std::pair<ExecutionState, InputAqlItemRow> SingleRowFetcherHelper<passBlocksThrough>::fetchRow(size_t) {
  // If this assertion fails, the Executor has fetched more rows after DONE.
  TRI_ASSERT(_nrCalled <= _nrItems);
  if (wait()) {
    // if once DONE is returned, always return DONE
    if (_returnedDone) {
      return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
    }
    return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }
  _nrCalled++;
  if (_nrCalled > _nrItems) {
    _returnedDone = true;
    return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }
  _lastReturnedRow = InputAqlItemRow{getItemBlock(), _curRowIndex};
  nextRow();
  ExecutionState state;
  if (_nrCalled < _nrItems) {
    state = ExecutionState::HASMORE;
  } else {
    _returnedDone = true;
    state = ExecutionState::DONE;
  }
  return {state, _lastReturnedRow};
}

template <bool passBlocksThrough>
std::pair<ExecutionState, size_t> SingleRowFetcherHelper<passBlocksThrough>::skipRows(size_t const atMost) {
  ExecutionState state = ExecutionState::HASMORE;

  while (atMost > _skipped) {
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    std::tie(state, row) = fetchRow();
    if (state == ExecutionState::WAITING) {
      return {state, 0};
    }
    if (row.isInitialized()) {
      ++_skipped;
    }
    if (state == ExecutionState::DONE) {
      size_t skipped = _skipped;
      _skipped = 0;
      _totalSkipped += skipped;
      return {state, skipped};
    }
  }

  size_t skipped = _skipped;
  _skipped = 0;
  _totalSkipped += skipped;
  return {state, skipped};
}

template <bool passBlocksThrough>
std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>
SingleRowFetcherHelper<passBlocksThrough>::fetchBlockForPassthrough(size_t const atMost) {
  if (wait()) {
    return {ExecutionState::WAITING, nullptr};
  }

  size_t const remainingRows = _blockSize - _curIndexInBlock;
  size_t const from = _curRowIndex;
  size_t const to = _curRowIndex + remainingRows;

  bool const isLastBlock = _curRowIndex + _blockSize >= _nrItems;
  bool const askingForMore = _curRowIndex + atMost > _nrItems;

  bool const done = isLastBlock && askingForMore;

  ExecutionState const state = done ? ExecutionState::DONE : ExecutionState::HASMORE;

  return {state, _itemBlock->slice(from, to)};
}

// -----------------------------------------
// - SECTION ALLROWSFETCHER                -
// -----------------------------------------

AllRowsFetcherHelper::AllRowsFetcherHelper(std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer,
                                           bool returnsWaiting)
    : AllRowsFetcher(),
      _vPackBuffer(std::move(vPackBuffer)),
      _returnsWaiting(returnsWaiting),
      _nrItems(0),
      _nrRegs(0),
      _nrCalled(0),
      _resourceMonitor(),
      _itemBlockManager(&_resourceMonitor),
      _matrix(nullptr) {
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    _nrItems = _data.length();
  }
  if (_nrItems > 0) {
    VPackSlice oneRow = _data.at(0);
    TRI_ASSERT(oneRow.isArray());
    _nrRegs = static_cast<arangodb::aql::RegisterCount>(oneRow.length());
    SharedAqlItemBlockPtr itemBlock{new AqlItemBlock(_itemBlockManager, _nrItems, _nrRegs)};
    VPackToAqlItemBlock(_data, _nrRegs, *itemBlock);
    // Add all registers as valid input registers:
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
    for (RegisterId i = 0; i < _nrRegs; i++) {
      inputRegisters->emplace(i);
    }
    _matrix = std::make_unique<AqlItemMatrix>(_nrRegs);
    _matrix->addBlock(itemBlock);
  }
  if (_matrix == nullptr) {
    _matrix = std::make_unique<AqlItemMatrix>(_nrRegs);
  }
}

AllRowsFetcherHelper::~AllRowsFetcherHelper() = default;

std::pair<ExecutionState, AqlItemMatrix const*> AllRowsFetcherHelper::fetchAllRows() {
  // If this assertion fails, a the Executor has fetched more rows after DONE.
  TRI_ASSERT(_nrCalled <= _nrItems + 1);
  if (_returnsWaiting) {
    if (_nrCalled < _nrItems || _nrCalled == 0) {
      _nrCalled++;
      // if once DONE is returned, always return DONE
      if (_returnedDone) {
        return {ExecutionState::DONE, nullptr};
      }
      // We will return waiting once for each item
      return {ExecutionState::WAITING, nullptr};
    }
  } else {
    TRI_ASSERT(_nrCalled == 0);
  }
  _nrCalled++;
  return {ExecutionState::DONE, _matrix.get()};
};

// -----------------------------------------
// - SECTION CONSTFETCHER              -
// -----------------------------------------

ConstFetcherHelper::ConstFetcherHelper(AqlItemBlockManager& itemBlockManager,
                                       std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer)
    : ConstFetcher(), _vPackBuffer(std::move(vPackBuffer)) {
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    auto nrItems = _data.length();
    if (nrItems > 0) {
      VPackSlice oneRow = _data.at(0);
      TRI_ASSERT(oneRow.isArray());
      arangodb::aql::RegisterCount nrRegs = static_cast<arangodb::aql::RegisterCount>(oneRow.length());
      auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
      for (RegisterId i = 0; i < nrRegs; i++) {
        inputRegisters->emplace(i);
      }
      SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, nrItems, nrRegs)};
      VPackToAqlItemBlock(_data, nrRegs, *block);
      this->injectBlock(block);
    }
  }
};

ConstFetcherHelper::~ConstFetcherHelper() = default;

std::pair<ExecutionState, InputAqlItemRow> ConstFetcherHelper::fetchRow() {
  return ConstFetcher::fetchRow();
};

template class ::arangodb::tests::aql::SingleRowFetcherHelper<false>;
template class ::arangodb::tests::aql::SingleRowFetcherHelper<true>;
