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

#include "Aql/AqlItemRow.h"
#include "Aql/AqlItemMatrix.h"

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::aql;

// -----------------------------------------
// - SECTION SINGLEROWFETCHER              -
// -----------------------------------------

SingleRowFetcherHelper::SingleRowFetcherHelper(
    std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer, bool returnsWaiting)
    : SingleRowFetcher(),
      _vPackBuffer(vPackBuffer),
      _returnsWaiting(returnsWaiting),
      _nrItems(0),
      _nrCalled(0),
      _didWait(false) {
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    _nrItems = _data.length();
  }
};

SingleRowFetcherHelper::~SingleRowFetcherHelper() = default;

std::pair<ExecutionState, AqlItemRow const*>
SingleRowFetcherHelper::fetchRow() {
  // If this REQUIRE fails, a the Executor has fetched more rows after DONE.
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
  // NOT YET IMPLEMENTED!
  REQUIRE(false);
  return {ExecutionState::DONE, nullptr};
};

// -----------------------------------------
// - SECTION ALLROWSFETCHER                -
// -----------------------------------------

AllRowsFetcherHelper::AllRowsFetcherHelper(
    std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer, bool returnsWaiting)
    : AllRowsFetcher(),
      _vPackBuffer(vPackBuffer),
      _returnsWaiting(returnsWaiting),
      _nrItems(0),
      _nrCalled(0),
      _didWait(false) {
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    _nrItems = _data.length();
  }
};

AllRowsFetcherHelper::~AllRowsFetcherHelper() = default;

std::pair<ExecutionState, AqlItemMatrix const*>
AllRowsFetcherHelper::fetchAllRows() {
  // If this REQUIRE fails, a the Executor has fetched more rows after DONE.
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
  // NOT YET IMPLEMENTED!
  REQUIRE(false);
  return {ExecutionState::DONE, nullptr};
};
