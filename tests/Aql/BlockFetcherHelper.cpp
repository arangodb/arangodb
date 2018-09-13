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

#include "BlockFetcherHelper.h"

#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemRow.h"
#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/AqlItemRow.h"
#include "Aql/FilterExecutor.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortExecutor.h"

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::aql;

namespace {
static void VPackToAqlItemBlock(VPackSlice data, uint64_t nrRegs, AqlItemBlock& block) {
  // coordinates in the matrix rowNr, entryNr
  size_t rowIndex = 0;
  RegisterId entry = 0;
  for (auto const& row : VPackArrayIterator(data)) {
    // Walk through the rows
    REQUIRE(row.isArray());
    REQUIRE(row.length() == nrRegs);
    for (auto const& oneEntry : VPackArrayIterator(row)) {
      // Walk through on row values
      block.setValue(rowIndex, entry, AqlValue{oneEntry});
      entry++;
    }
    rowIndex++;
    entry = 0;
  }
}
}

// -----------------------------------------
// - SECTION SINGLEROWFETCHER              -
// -----------------------------------------

SingleRowFetcherHelper::SingleRowFetcherHelper(
    std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer, bool returnsWaiting)
    : SingleRowFetcher(),
      _vPackBuffer(std::move(vPackBuffer)),
      _returnsWaiting(returnsWaiting),
      _nrItems(0),
      _nrCalled(0),
      _didWait(false),
      _resourceMonitor(),
      _itemBlock(nullptr) {
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    _nrItems = _data.length();
    if (_nrItems > 0) {
      VPackSlice oneRow = _data.at(0);
      REQUIRE(oneRow.isArray());
      uint64_t nrRegs = oneRow.length();
      _itemBlock =
          std::make_unique<AqlItemBlock>(&_resourceMonitor, _nrItems, nrRegs);
      VPackToAqlItemBlock(_data, nrRegs, *(_itemBlock.get()));
    }
  }
};

SingleRowFetcherHelper::~SingleRowFetcherHelper() = default;

std::pair<ExecutionState, AqlItemRow const*>
SingleRowFetcherHelper::fetchRow() {
  // If this REQUIRE fails, the Executor has fetched more rows after DONE.
  REQUIRE(_nrCalled <= _nrItems);
  if (_returnsWaiting) {
    if(!_didWait) {
      _didWait = true;
      return {ExecutionState::WAITING, nullptr};
    }
    _didWait = false;
  }
  _nrCalled++;
  if (_nrCalled > _nrItems) {
    return {ExecutionState::DONE, nullptr};
  }
  _lastReturnedRow = std::make_unique<AqlItemRow>(*_itemBlock.get(), _nrCalled -1);
  ExecutionState state;
  if (_nrCalled < _nrItems) {
    state = ExecutionState::HASMORE;
  } else {
    state = ExecutionState::DONE;
  }
  return {state, _lastReturnedRow.get()};
};

// -----------------------------------------
// - SECTION ALLROWSFETCHER                -
// -----------------------------------------

AllRowsFetcherHelper::AllRowsFetcherHelper(
    std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer, bool returnsWaiting)
    : AllRowsFetcher(),
      _vPackBuffer(std::move(vPackBuffer)),
      _returnsWaiting(returnsWaiting),
      _nrItems(0),
      _nrCalled(0),
      _resourceMonitor(),
      _itemBlock(nullptr),
      _matrix(nullptr) {
  _matrix = std::make_unique<AqlItemMatrix>();
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    _nrItems = _data.length();
    if (_nrItems > 0) {
      VPackSlice oneRow = _data.at(0);
      REQUIRE(oneRow.isArray());
      uint64_t nrRegs = oneRow.length();
      _itemBlock =
          std::make_shared<AqlItemBlock>(&_resourceMonitor, _nrItems, nrRegs);
      VPackToAqlItemBlock(_data, nrRegs, *(_itemBlock.get()));
      _matrix->addBlock(_itemBlock);
    }
  }
}

AllRowsFetcherHelper::~AllRowsFetcherHelper() = default;

std::pair<ExecutionState, AqlItemMatrix const*>
AllRowsFetcherHelper::fetchAllRows() {
  // If this REQUIRE fails, a the Executor has fetched more rows after DONE.
  REQUIRE(_nrCalled <= _nrItems + 1);
  if (_returnsWaiting) {
    if (_nrCalled < _nrItems || _nrCalled == 0) {
      _nrCalled++;
      // We will return waiting once for each item
      return {ExecutionState::WAITING, nullptr};
    }
  } else {
    REQUIRE(_nrCalled == 0);
  }
  _nrCalled++;
  return {ExecutionState::DONE, _matrix.get()};
};
