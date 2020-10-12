////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "AqlItemBlockInputMatrix.h"
#include "Aql/ShadowAqlItemRow.h"

#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <numeric>

using namespace arangodb;
using namespace arangodb::aql;

AqlItemBlockInputMatrix::AqlItemBlockInputMatrix(ExecutorState state)
    : _finalState{state}, _aqlItemMatrix{nullptr} {
  // TODO As we only allow HASMORE, just use the default constructor and remove
  //      this one.
  TRI_ASSERT(state == ExecutorState::HASMORE);
  TRI_ASSERT(_aqlItemMatrix == nullptr);
  TRI_ASSERT(!hasDataRow());
}

// only used for block passthrough
AqlItemBlockInputMatrix::AqlItemBlockInputMatrix(arangodb::aql::SharedAqlItemBlockPtr const& block)
    : _block{block}, _aqlItemMatrix{nullptr} {
  TRI_ASSERT(_aqlItemMatrix == nullptr);
  TRI_ASSERT(!hasDataRow());
}

AqlItemBlockInputMatrix::AqlItemBlockInputMatrix(ExecutorState state, AqlItemMatrix* aqlItemMatrix)
    : _finalState{state}, _aqlItemMatrix{aqlItemMatrix} {
  TRI_ASSERT(_block == nullptr);
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  if (_aqlItemMatrix->size() == 0 && _aqlItemMatrix->stoppedOnShadowRow()) {
    // Fast forward to initialize the _shadowRow
    skipAllRemainingDataRows();
  }
}

AqlItemBlockInputRange& AqlItemBlockInputMatrix::getInputRange() {
  TRI_ASSERT(_aqlItemMatrix != nullptr);

  if (_lastRange.hasDataRow()) {
    return _lastRange;
  }
  // Need initialze lastRange
  if (_aqlItemMatrix->numberOfBlocks() == 0) {
    _lastRange = {AqlItemBlockInputRange{upstreamState()}};
  } else {
    auto const [blockPtr, start] =  _aqlItemMatrix->getBlock(_currentBlockRowIndex);
    ExecutorState state = incrBlockIndex();
    _lastRange = {state, 0, std::move(blockPtr), start};
  }
  return _lastRange;
}

SharedAqlItemBlockPtr AqlItemBlockInputMatrix::getBlock() const noexcept {
  TRI_ASSERT(_aqlItemMatrix == nullptr);
  return _block;
}

std::pair<ExecutorState, AqlItemMatrix const*> AqlItemBlockInputMatrix::getMatrix() noexcept {
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  TRI_ASSERT(_block == nullptr);
  TRI_ASSERT(!_shadowRow.isInitialized());

  // We are always done. This InputMatrix
  // guarantees that we have all data in our hand at once.
  return {ExecutorState::DONE, _aqlItemMatrix};
}

ExecutorState AqlItemBlockInputMatrix::upstreamState() const noexcept {
  if (_aqlItemMatrix == nullptr) {
    return _finalState;
  }
  if (hasShadowRow() || _aqlItemMatrix->stoppedOnShadowRow()) {
    return ExecutorState::DONE;
  }
  return _finalState;
}

bool AqlItemBlockInputMatrix::upstreamHasMore() const noexcept {
  return upstreamState() == ExecutorState::HASMORE;
}
  
bool AqlItemBlockInputMatrix::hasValidRow() const noexcept {
  return _shadowRow.isInitialized() ||
         (_aqlItemMatrix != nullptr && _aqlItemMatrix->size() != 0);
}

bool AqlItemBlockInputMatrix::hasDataRow() const noexcept {
  if (_aqlItemMatrix == nullptr) {
    return false;
  }

  return !hasShadowRow() && _aqlItemMatrix != nullptr &&
         ((_aqlItemMatrix->stoppedOnShadowRow()) ||
          (_aqlItemMatrix->size() > 0 && _finalState == ExecutorState::DONE));
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputMatrix::nextShadowRow() {
  auto tmpShadowRow = std::move(_shadowRow);
  TRI_ASSERT(_aqlItemMatrix != nullptr);

  if (_aqlItemMatrix->size() == 0 && _aqlItemMatrix->stoppedOnShadowRow()) {
    // next row will be a shadow row
    _shadowRow = _aqlItemMatrix->popShadowRow();
    resetBlockIndex();
  } else {
    _shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint()};
  }

  auto state = ExecutorState::HASMORE;
  if (_shadowRow.isInitialized() || _aqlItemMatrix->stoppedOnShadowRow()) {
    state = ExecutorState::HASMORE;
  } else {
    state = _finalState;
  }

  return {state, std::move(tmpShadowRow)};
}

ShadowAqlItemRow AqlItemBlockInputMatrix::peekShadowRow() const {
  return _shadowRow;
}

bool AqlItemBlockInputMatrix::hasShadowRow() const noexcept {
  return _shadowRow.isInitialized();
}

size_t AqlItemBlockInputMatrix::skipAllRemainingDataRows() {
  if (_aqlItemMatrix == nullptr) {
    // Have not been initialized.
    // We need to be called before.
    TRI_ASSERT(!hasShadowRow());
    TRI_ASSERT(!hasDataRow());
    return 0;
  }
  if (!hasShadowRow()) {
    if (_aqlItemMatrix->stoppedOnShadowRow()) {
      _shadowRow = _aqlItemMatrix->popShadowRow();
      TRI_ASSERT(_shadowRow.isRelevant());
    } else {
      // This can happen if we are either DONE.
      // or if the executor above produced
      // exactly one full block and was done
      // but there was no place for the ShadowRow.
      _aqlItemMatrix->clear();
    }
    resetBlockIndex();
  }
  // Else we did already skip once.
  // nothing to do
  return 0;
}

ExecutorState AqlItemBlockInputMatrix::incrBlockIndex() {
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  if (_currentBlockRowIndex + 1 < _aqlItemMatrix->numberOfBlocks()) {
    _currentBlockRowIndex++;
    // we were able to increase the size as we reached not the end yet
    return ExecutorState::HASMORE;
  }
  // we could not increase the index, we already reached the end
  return ExecutorState::DONE;
}

void AqlItemBlockInputMatrix::resetBlockIndex() noexcept {
  _lastRange = {AqlItemBlockInputRange{upstreamState()}};
  _currentBlockRowIndex = 0;
}

[[nodiscard]] auto AqlItemBlockInputMatrix::countDataRows() const noexcept -> std::size_t {
  if (_aqlItemMatrix == nullptr) {
    return 0;
  }
  return _aqlItemMatrix->countDataRows();
}

[[nodiscard]] auto AqlItemBlockInputMatrix::countShadowRows() const noexcept -> std::size_t {
  if (_aqlItemMatrix == nullptr) {
    return 0;
  }
  // Count the remainder in the Matrix
  // And count the one shadowRow we have here, but not delivered
  return _aqlItemMatrix->countShadowRows() + (hasShadowRow() ? 1 : 0);
}

[[nodiscard]] auto AqlItemBlockInputMatrix::finalState() const noexcept -> ExecutorState {
  return _finalState;
}
