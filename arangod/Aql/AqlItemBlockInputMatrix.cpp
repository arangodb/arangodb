////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
  if (hasDataRow() || hasShadowRow()) {
    return ExecutorState::DONE;
  }
  return _finalState;
}

bool AqlItemBlockInputMatrix::upstreamHasMore() const noexcept {
  return upstreamState() == ExecutorState::HASMORE;
}

bool AqlItemBlockInputMatrix::hasDataRow() const noexcept {
  if (_aqlItemMatrix == nullptr) {
    return false;
  }
  return (!_shadowRow.isInitialized() && _aqlItemMatrix->size() != 0);
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputMatrix::nextShadowRow() {
  auto tmpShadowRow = _shadowRow;

  if (_aqlItemMatrix->size() == 0 && _aqlItemMatrix->stoppedOnShadowRow() &&
      !_aqlItemMatrix->peekShadowRow().isRelevant()) {
    // next row will be a shadow row
    _shadowRow = _aqlItemMatrix->popShadowRow();
  } else {
    _shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint()};
  }

  auto state = ExecutorState::HASMORE;
  if (_shadowRow.isInitialized()) {
    TRI_ASSERT(!_shadowRow.isRelevant());
    state = ExecutorState::HASMORE;
  } else if (_aqlItemMatrix->stoppedOnShadowRow() &&
             _aqlItemMatrix->peekShadowRow().isRelevant()) {
    state = ExecutorState::DONE;
  } else {
    state = _finalState;
  }

  return {state, tmpShadowRow};
}

ShadowAqlItemRow AqlItemBlockInputMatrix::peekShadowRow() const {
  return _shadowRow;
}

bool AqlItemBlockInputMatrix::hasShadowRow() const noexcept {
  return _shadowRow.isInitialized();
}

void AqlItemBlockInputMatrix::skipAllRemainingDataRows() {
  if (_aqlItemMatrix == nullptr) {
    // Have not been initialized.
    // We need to be called before.
    TRI_ASSERT(!hasShadowRow());
    TRI_ASSERT(!hasDataRow());
    return;
  }
  TRI_ASSERT(!_shadowRow.isInitialized());

  if (_aqlItemMatrix->stoppedOnShadowRow()) {
    _shadowRow = _aqlItemMatrix->popShadowRow();
    TRI_ASSERT(_shadowRow.isRelevant());
  } else {
    TRI_ASSERT(_finalState == ExecutorState::DONE);
    _aqlItemMatrix->clear();
  }
}
