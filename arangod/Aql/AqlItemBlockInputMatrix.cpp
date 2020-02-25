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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <numeric>

using namespace arangodb;
using namespace arangodb::aql;

AqlItemBlockInputMatrix::AqlItemBlockInputMatrix(ExecutorState state, std::size_t skipped)
    : _finalState{state}, _skipped{skipped}, _isRelevant{false} {}

// only used for block passthrough
AqlItemBlockInputMatrix::AqlItemBlockInputMatrix(arangodb::aql::SharedAqlItemBlockPtr const& block)
    : _block{block}, _aqlItemMatrix{nullptr}, _isRelevant{false} {}

AqlItemBlockInputMatrix::AqlItemBlockInputMatrix(ExecutorState state, std::size_t skipped,
                                                 AqlItemMatrix* aqlItemMatrix)
    : _finalState{state}, _skipped{skipped}, _aqlItemMatrix{aqlItemMatrix}, _isRelevant{true} {
  TRI_ASSERT(_block == nullptr);
}

SharedAqlItemBlockPtr AqlItemBlockInputMatrix::getBlock() const noexcept {
  TRI_ASSERT(!_isRelevant && _aqlItemMatrix == nullptr);
  return _block;
}

std::pair<ExecutorState, AqlItemMatrix const*> AqlItemBlockInputMatrix::getMatrix() const noexcept {
  TRI_ASSERT(_isRelevant && _aqlItemMatrix);
  TRI_ASSERT(_finalState == ExecutorState::DONE);
  return {_finalState, _aqlItemMatrix};
}

ExecutorState AqlItemBlockInputMatrix::upstreamState() const noexcept {
  return _finalState;
}

bool AqlItemBlockInputMatrix::upstreamHasMore() const noexcept {
  return upstreamState() == ExecutorState::HASMORE;
}

ExecutorState AqlItemBlockInputMatrix::skipAllRemainingDataRows() {
  _aqlItemMatrix->popShadowRow();
  return ExecutorState::DONE;
}

auto AqlItemBlockInputMatrix::skip(std::size_t const toSkip) noexcept -> std::size_t {
  auto const skipCount = std::min(_skipped, toSkip);
  _skipped -= skipCount;
  return skipCount;
}

auto AqlItemBlockInputMatrix::skippedInFlight() const noexcept -> std::size_t {
  return _skipped;
}

auto AqlItemBlockInputMatrix::skipAll() noexcept -> std::size_t {
  auto const skipped = _skipped;
  _skipped = 0;
  return skipped;
}
