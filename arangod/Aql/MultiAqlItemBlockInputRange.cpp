////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "MultiAqlItemBlockInputRange.h"
#include "Aql/ShadowAqlItemRow.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>
#include <numeric>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
  static auto RowHasNonEmptyValue(ShadowAqlItemRow const& row) -> bool {
    if (row.isInitialized()) {
      RegisterCount const numRegisters = static_cast<RegisterCount>(row.getNumRegisters());
      for (RegisterId::value_t registerId = 0; registerId < numRegisters; ++registerId) {
        if (!row.getValue(registerId).isEmpty()) {
          return true;
        }
      }
    }
    return false;
  }
}


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

auto MultiAqlItemBlockInputRange::hasValidRow() const noexcept -> bool {
  return std::any_of(std::begin(_inputs), std::end(_inputs),
                     [](AqlItemBlockInputRange const& i) -> bool {
                       return i.hasValidRow();
                     });
}

auto MultiAqlItemBlockInputRange::hasDataRow() const noexcept -> bool {
  return std::any_of(std::begin(_inputs), std::end(_inputs),
                     [](AqlItemBlockInputRange const& i) -> bool {
                       return i.hasDataRow();
                     });
}

auto MultiAqlItemBlockInputRange::hasDataRow(size_t const dependency) const noexcept -> bool {
  TRI_ASSERT(dependency < _inputs.size());
  return _inputs.at(dependency).hasDataRow();
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

// * It doesn't matter which shadow row we peek, they should all be the same
auto MultiAqlItemBlockInputRange::peekShadowRow() const -> arangodb::aql::ShadowAqlItemRow {
  if (!hasShadowRow()) {
    return ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
  }

  // All Ranges are on the same shadow Row.
  auto row = _inputs.at(0).peekShadowRow();
  if (RowHasNonEmptyValue(row)) {
    return row;
  }
  size_t const n = _inputs.size();
  for (size_t i = 1; i < n; ++i) {
    auto const otherrow = _inputs[i].peekShadowRow();
    if (RowHasNonEmptyValue(otherrow)) {
      return otherrow;
    }
  }
  // There is a smallish chance that actually
  // None of the shadowRows contains data
  // That is if no register needs to be bypassed
  // in the query.
  return row;
}

auto MultiAqlItemBlockInputRange::nextShadowRow()
    -> std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> {
  TRI_ASSERT(!hasDataRow());
  
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!_dependenciesDontAgreeOnState);
  auto oneDependencyDone = bool{false};
  auto oneDependencyHasMore = bool{false};
#endif

  // Need to consume all shadow rows simultaneously.
  // Only one (a random one) will contain the data.
  TRI_ASSERT(!_inputs.empty());
  auto [state, shadowRow] = _inputs.at(0).nextShadowRow();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (state == ExecutorState::DONE) {
    oneDependencyDone = true;
  } else if (state == ExecutorState::HASMORE) {
    oneDependencyHasMore = true;
  }
#endif

  bool foundData = ::RowHasNonEmptyValue(shadowRow);
  size_t const n = _inputs.size();
  for (size_t i = 1; i < n; ++i) {
    auto [otherState, otherRow] = _inputs[i].nextShadowRow();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if (otherState == ExecutorState::DONE) {
      oneDependencyDone = true;
    } else if (otherState == ExecutorState::HASMORE) {
      oneDependencyHasMore = true;
    }
#endif

    // if any dependency thinks it has more, maybe we'll have to be
    // asked again to finish up.
    // In maintainer mode we assert that not more data is being
    // produced, because that must not happen!
    if (otherState == ExecutorState::HASMORE) {
      state = ExecutorState::HASMORE;
    }
    // All inputs need to be in the same part of the query.
    // We cannot have overlapping subquery executions.
    // We can only have all rows initialized or none.
    TRI_ASSERT(shadowRow.isInitialized() == otherRow.isInitialized());
    if (!foundData) {
      if (::RowHasNonEmptyValue(otherRow)) {
        // We found the one that contains data.
        // Take it
        shadowRow = std::move(otherRow);
        foundData = true;
      }
    } else {
      // we already have the filled one.
      // Just assert the others are empty for correctness
      TRI_ASSERT(! ::RowHasNonEmptyValue(otherRow));
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _dependenciesDontAgreeOnState = oneDependencyHasMore && oneDependencyDone;
#endif

  return {state, std::move(shadowRow)};
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
  return  std::all_of(std::begin(_inputs), std::end(_inputs),
                      [](AqlItemBlockInputRange const& i) -> bool {
                        return i.upstreamState() == ExecutorState::DONE &&
                               !i.hasDataRow();
                      });
}

auto MultiAqlItemBlockInputRange::state() const -> ExecutorState {
  return isDone() ? ExecutorState::DONE : ExecutorState::HASMORE;
}

auto MultiAqlItemBlockInputRange::skipAllRemainingDataRows() -> size_t {
  size_t const n = _inputs.size();
  for (size_t i = 0; i < n; i++) {
    std::ignore = _inputs[i].skipAllRemainingDataRows();
    if (_inputs.at(i).upstreamState() == ExecutorState::HASMORE) {
      return i;
    }
  }
  return 0;
}

auto MultiAqlItemBlockInputRange::skipAllShadowRowsOfDepth(size_t depth)
    -> std::vector<size_t> {
  size_t const n = _inputs.size();
  std::vector<size_t> skipped{};
  skipped.reserve(n);
  for (auto& input : _inputs) {
    skipped.emplace_back(input.skipAllShadowRowsOfDepth(depth));
  }
  return skipped;
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
  for (auto& range : _inputs) {
    range = AqlItemBlockInputRange(ExecutorState::HASMORE);
  }
}

[[nodiscard]] auto MultiAqlItemBlockInputRange::countDataRows() const noexcept
    -> std::size_t {
  size_t count = 0;
  for (auto const& range : _inputs) {
    count += range.countDataRows();
  }
  return count;
}

[[nodiscard]] auto MultiAqlItemBlockInputRange::countShadowRows() const noexcept
    -> std::size_t {
  size_t count = 0;
  for (auto const& range : _inputs) {
    // Every range will convey the same shadowRows.
    // They can only be taken in a synchronous way on all ranges
    // at the same time.
    // So the amount of ShadowRows that we can produce here is
    // upperbounded by the maximum of shadowRows in each range.
    count = std::max(count, range.countShadowRows());
  }
  return count;
}

[[nodiscard]] auto MultiAqlItemBlockInputRange::finalState() const noexcept -> ExecutorState {
  bool hasMore = std::any_of(_inputs.begin(), _inputs.end(), [](auto const& range) {
    return range.finalState() == ExecutorState::HASMORE;
  });
  if (hasMore) {
    return ExecutorState::HASMORE;
  }
  return ExecutorState::DONE;
}
