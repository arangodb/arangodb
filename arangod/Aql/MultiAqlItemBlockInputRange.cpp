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

using namespace arangodb;
using namespace arangodb::aql;

MultiAqlItemBlockInputRange::MultiAqlItemBlockInputRange(ExecutorState state,
                                                         std::size_t skipped,
                                                         std::size_t nrInputRanges) {
  _inputs.resize(nrInputRanges, AqlItemBlockInputRange{state, skipped});
}

auto MultiAqlItemBlockInputRange::resize(ExecutorState state, size_t skipped,
                                         size_t nrInputRanges) -> void {
  _inputs.resize(nrInputRanges, AqlItemBlockInputRange{state, skipped});
}

auto MultiAqlItemBlockInputRange::upstreamState(size_t const dependency) const
    noexcept -> ExecutorState {
  return _inputs.at(dependency).upstreamState();
}

auto MultiAqlItemBlockInputRange::hasDataRow(size_t const dependency) const noexcept -> bool {
  return _inputs.at(dependency).hasDataRow();
}

auto MultiAqlItemBlockInputRange::peekDataRow(size_t const dependency) const
    -> std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> {
  return _inputs.at(dependency).peekDataRow();
}

auto MultiAqlItemBlockInputRange::nextDataRow(size_t const dependency)
    -> std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> {
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
auto MultiAqlItemBlockInputRange::peekShadowRow(size_t const dependency) const
    -> std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> {
  return _inputs.at(dependency).peekShadowRow();
}

auto MultiAqlItemBlockInputRange::nextShadowRow(size_t const dependency)
    -> std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> {
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
  return _inputs.at(dependency).getBlock();
}

auto MultiAqlItemBlockInputRange::setDependency(size_t const dependency,
                                                AqlItemBlockInputRange& range) -> void {
  _inputs.at(dependency) = range;
}
