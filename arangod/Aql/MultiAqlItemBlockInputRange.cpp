////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "MultiAqlItemBlockInputRange.h"
#include "Aql/ShadowAqlItemRow.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <numeric>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

MultiAqlItemBlockInputRange::MultiAqlItemBlockInputRange(ExecutorState state,
                                                         std::size_t skipped,
                                                         std::size_t nrInputRanges) {
  _inputs.resize(nrInputRanges, AqlItemBlockInputRange{state, skipped});
  TRI_ASSERT(nrInputRanges > 0);
}

auto MultiAqlItemBlockInputRange::resizeOnce(ExecutorState state, size_t skipped,
                                             size_t nrInputRanges) -> void {
  // Is expected to be called exactly once to set the number of dependencies.
  // We never want to reduce the number of dependencies.
  TRI_ASSERT(_inputs.size() <= nrInputRanges);
  TRI_ASSERT(nrInputRanges > 0);
  _inputs.resize(nrInputRanges, AqlItemBlockInputRange{state, skipped});
}

auto MultiAqlItemBlockInputRange::upstreamState(size_t const dependency) const
    noexcept -> ExecutorState {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).upstreamState();
}

auto MultiAqlItemBlockInputRange::hasDataRow(size_t const dependency) const noexcept -> bool {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).hasDataRow();
}

auto MultiAqlItemBlockInputRange::hasDataRow() const noexcept -> bool {
  return std::any_of(std::begin(_inputs), std::end(_inputs),
                     [](AqlItemBlockInputRange const& i) -> bool {
                       return i.hasDataRow();
                     });
}

auto MultiAqlItemBlockInputRange::rangeForDependency(size_t const dependency)
    -> AqlItemBlockInputRange& {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency);
}

auto MultiAqlItemBlockInputRange::peekDataRow(size_t const dependency) const
    -> std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).peekDataRow();
}

auto MultiAqlItemBlockInputRange::skipAll(size_t const dependency) noexcept -> std::size_t {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).skipAll();
}

auto MultiAqlItemBlockInputRange::skippedInFlight(size_t const dependency) const
    noexcept -> std::size_t {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).skippedInFlight();
}

auto MultiAqlItemBlockInputRange::nextDataRow(size_t const dependency)
    -> std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).nextDataRow();
}

// We have a shadow row, iff all our inputs have o
auto MultiAqlItemBlockInputRange::hasShadowRow() const noexcept -> bool {
  return std::all_of(std::begin(_inputs), std::end(_inputs),
                     [](AqlItemBlockInputRange const& i) -> bool {
                       return i.hasShadowRow();
                     });
}

// TODO: * It doesn't matter which shadow row we peek, they should all be the same
//       * assert that all dependencies are on a shadow row?
auto MultiAqlItemBlockInputRange::peekShadowRow() const -> arangodb::aql::ShadowAqlItemRow {
  TRI_ASSERT(!hasDataRow());
  TRI_ASSERT(!_inputs.empty());
  // TODO: Correct?
  return _inputs.at(0).peekShadowRow();
}

auto MultiAqlItemBlockInputRange::nextShadowRow()
    -> std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> {
  TRI_ASSERT(!hasDataRow());

  // Need to consume all shadow rows simultaneously.
  // TODO: Assert we're on the correct shadow row for all upstreams
  auto state = ExecutorState::HASMORE;
  auto shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint()};

  for (auto& i : _inputs) {
    std::tie(state, shadowRow) = i.nextShadowRow();
  }
  return {state, shadowRow};
}

auto MultiAqlItemBlockInputRange::getBlock(size_t const dependency) const
    noexcept -> SharedAqlItemBlockPtr {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).getBlock();
}

auto MultiAqlItemBlockInputRange::setDependency(size_t const dependency,
                                                AqlItemBlockInputRange const& range) -> void {
  TRI_ASSERT(dependency < _inputs.size());
  _inputs.at(dependency) = range;
}

auto MultiAqlItemBlockInputRange::isDone() const -> bool {
  auto res = std::all_of(std::begin(_inputs), std::end(_inputs),
                         [](AqlItemBlockInputRange const& i) -> bool {
                           return !i.hasDataRow() &&
                                  i.upstreamState() == ExecutorState::DONE;
                         });
  return res;
}

auto MultiAqlItemBlockInputRange::state() const -> ExecutorState {
  return isDone() ? ExecutorState::DONE : ExecutorState::HASMORE;
}

auto MultiAqlItemBlockInputRange::skipAllRemainingDataRows() -> size_t {
  for (size_t i = 0; i < _inputs.size(); i++) {
    std::ignore = _inputs.at(i).skipAllRemainingDataRows();
    if (_inputs.at(i).upstreamState() == ExecutorState::HASMORE) {
      return i;
    }
  }
  return 0;
}

// Subtract up to count rows from the local _skipped state
auto MultiAqlItemBlockInputRange::skipForDependency(size_t const dependency,
                                                    size_t count) -> size_t {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).skip(count);
}

// Skip all that is available
auto MultiAqlItemBlockInputRange::skipAllForDependency(size_t const dependency) -> size_t {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).skipAll();
}

auto MultiAqlItemBlockInputRange::numberDependencies() const noexcept -> size_t {
  return _inputs.size();
}

auto MultiAqlItemBlockInputRange::reset() -> void {
  for (size_t i = 0; i < _inputs.size(); ++i) {
    _inputs[i] = AqlItemBlockInputRange(ExecutorState::HASMORE);
  }
}
